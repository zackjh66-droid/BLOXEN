'use strict';
const fs = require('fs'); const path = require('path'); const http = require('http');
const cfg = require('../lib/config'); const auth = require('../lib/auth'); const S = require('../lib/services'); const V = require('./views'); const registry = require('../lib/registry'); const { paginate } = require('./components');
const { esc, readForm, send, redirect, cookie, parseCookies, safeEqual, safeNext, SEC_HEADERS } = require('../lib/http');

const STATIC = path.join(cfg.root, 'static'); const MIME = { '.css': 'text/css; charset=utf-8', '.js': 'application/javascript; charset=utf-8', '.png': 'image/png', '.svg': 'image/svg+xml' };
const SESSION = 'bloxen_session', PRE = 'bx_pre', FLASH = 'bx_flash';

function createWebApp({ db, gameservers, log = () => {}, secureCookies = false }) {
  const routes = [];
  const add = (method, pattern, opts, fn) => { if (typeof opts === 'function') { fn = opts; opts = {}; } const keys = []; const re = new RegExp('^' + pattern.replace(/:([a-z]+)/g, (_, k) => { keys.push(k); return '([^/]+)'; }) + '/?$'); routes.push({ method, re, keys, opts, fn }); };
  const flashSet = (res, text, err) => ({ 'Set-Cookie': cookie(FLASH, encodeURIComponent(JSON.stringify({ text, err: !!err })), { maxAge: 30, secure: secureCookies }) });

  async function handle(req, res) {
    const url = new URL(req.url, 'http://x'); const pathname = url.pathname;
    if (req.method === 'GET' && pathname.startsWith('/static/')) return serveStatic(pathname, res);
    const cookies = parseCookies(req.headers.cookie); const sess = auth.getSession(db, cookies[SESSION]);
    const c = { req, res, url, user: sess || null, csrf: sess ? sess.csrf : '', cookies, query: Object.fromEntries(url.searchParams), params: {}, setCookies: [], flash: null, token: cookies[SESSION] };
    if (cookies[FLASH]) { try { const f = JSON.parse(decodeURIComponent(cookies[FLASH])); if (typeof f.text === 'string') c.flash = { text: f.text.slice(0, 300), err: !!f.err }; } catch {} c.setCookies.push(cookie(FLASH, '', { maxAge: 0 })); }
    if (sess) db.prepare('UPDATE users SET last_online=? WHERE id=?').run(Date.now(), sess.id);
    for (const r of routes) {
      if (r.method !== req.method) continue; const m = r.re.exec(pathname); if (!m) continue; r.keys.forEach((k, i) => { c.params[k] = decodeURIComponent(m[i + 1]); });
      if (r.opts.auth && !c.user) return redirect(res, '/login?next=' + encodeURIComponent(safeNext(pathname)), { 'Set-Cookie': c.setCookies });
      if (req.method === 'POST') { c.form = await readForm(req); const tok = r.opts.pre ? cookies[PRE] : c.csrf; if (!tok || !c.form._csrf || !safeEqual(tok, c.form._csrf)) return send(res, 403, V.layout(c, 'Forbidden', '<h1>Request rejected</h1><p>Invalid or missing security token. Go back, reload the page and try again.</p>'), { 'Set-Cookie': c.setCookies }); }
      try { return await r.fn(c); } catch (e) { log('error ' + pathname + ': ' + (e.stack || e)); return out(c, 500, V.layout(c, 'Error', '<h1>Something went wrong</h1>')); }
    }
    return out(c, 404, V.notFound(c));
  }
  const out = (c, status, html, extra = {}) => send(c.res, status, html, { ...(c.setCookies.length ? { 'Set-Cookie': c.setCookies } : {}), 'Cache-Control': 'no-store', ...extra });
  const go = (c, to, text, err) => { const cs = [...c.setCookies]; if (text) cs.push(cookie(FLASH, encodeURIComponent(JSON.stringify({ text, err: !!err })), { maxAge: 30, secure: secureCookies })); redirect(c.res, to, { 'Set-Cookie': cs }); };
  function serveStatic(p, res) { const rel = path.normalize(decodeURIComponent(p.slice('/static/'.length))); const f = path.join(STATIC, rel); if (rel.startsWith('..') || path.isAbsolute(rel) || !f.startsWith(STATIC + path.sep) || !fs.existsSync(f) || !fs.statSync(f).isFile()) return send(res, 404, 'Not found', { 'Content-Type': 'text/plain' }); res.writeHead(200, { ...SEC_HEADERS, 'Content-Type': MIME[path.extname(f)] || 'application/octet-stream', 'Cache-Control': 'public, max-age=300' }); res.end(fs.readFileSync(f)); }
  const preToken = c => { let t = c.cookies[PRE]; if (!t) { t = auth.rand(18); c.setCookies.push(cookie(PRE, t, { secure: secureCookies })); } return t; };
  const withAvail = p => ({ ...p, available: gameservers.available(p) });

  // ---------------- public
  add('GET', '/', c => c.user ? go(c, '/home') : out(c, 200, V.homeOut(c, preToken(c))));
  add('GET', '/about', c => out(c, 200, V.about(c)));
  add('GET', '/login', c => out(c, 200, V.login(c, preToken(c), c.query.next)));
  add('GET', '/Login', c => redirect(c.res, '/login'));
  add('GET', '/register', c => out(c, 200, V.register(c, preToken(c))));
  add('POST', '/register', { pre: true }, async c => {
    const f = c.form; const name = String(f.username || ''); const pw = String(f.password || '');
    const fail = m => { const t = preToken(c); c.flash = { text: m, err: true }; return out(c, 400, V.register(c, t)); };
    if (!auth.validUsername(name)) return fail('Usernames are 3-20 characters: letters, numbers and single underscores, not starting or ending with _.'); if (!auth.validPassword(pw)) return fail('Password must be 8-200 characters.'); if (pw.toLowerCase() === name.toLowerCase()) return fail('Password cannot be your username.'); if (f.confirm !== undefined && f.confirm !== '' && f.confirm !== pw) return fail('Passwords do not match.'); if (f.confirm === '') return fail('Please confirm your password.');
    const by = +f.by, bm = +f.bm, bd = +f.bd; const birthday = by >= 1915 && by <= 2015 && bm >= 1 && bm <= 12 && bd >= 1 && bd <= 31 ? `${by}-${String(bm).padStart(2, '0')}-${String(bd).padStart(2, '0')}` : null;
    if (S.userByName(db, name)) return fail('That username is already taken.');
    const hash = await auth.hashPassword(pw); let id; try { id = S.createUser(db, { username: name, hash, birthday, gender: ['m', 'f'].includes(f.gender) ? f.gender : null }); } catch (e) { return fail('That username is already taken.'); }
    const s = auth.createSession(db, id); c.setCookies.push(cookie(SESSION, s.token, { maxAge: cfg.sessionTtlMs / 1000, secure: secureCookies }), cookie(PRE, '', { maxAge: 0 })); go(c, '/home', 'Welcome to BLOXEN!');
  });
  add('POST', '/login', { pre: true }, async c => {
    const name = String(c.form.username || '').slice(0, 40); const ip = c.req.socket.remoteAddress; const key = ip + '|' + name.toLowerCase();
    const fail = (m, st = 400) => { c.flash = { text: m, err: true }; return out(c, st, V.login(c, preToken(c), c.form.next)); };
    if (auth.throttled(key)) return fail('Too many failed attempts. Try again later.', 429);
    const u = S.userByName(db, name); const ok = u ? await auth.verifyPassword(String(c.form.password || ''), u.pass_hash) : (await auth.verifyPassword('x', await DUMMY_HASH()), false);
    if (!ok) { auth.noteFail(key); return fail('Incorrect username or password.'); } auth.clearFails(key);
    auth.destroySession(db, c.token); const s = auth.createSession(db, u.id); c.setCookies.push(cookie(SESSION, s.token, { maxAge: cfg.sessionTtlMs / 1000, secure: secureCookies }), cookie(PRE, '', { maxAge: 0 })); go(c, safeNext(c.form.next));
  });
  add('POST', '/logout', { auth: true }, c => { auth.destroySession(db, c.token); c.setCookies.push(cookie(SESSION, '', { maxAge: 0 })); go(c, '/', 'You have been logged out.'); });
  let dummy; const DUMMY_HASH = async () => dummy || (dummy = await auth.hashPassword('dummy-password'));

  // ---------------- home
  add('GET', '/home', { auth: true }, c => { const u = c.user; const ids = db.prepare('SELECT place_id FROM recent_plays WHERE user_id=? ORDER BY at DESC LIMIT 6').all(u.id).map(r => S.placeById(db, r.place_id)).filter(Boolean); const favs = db.prepare('SELECT place_id FROM favorites WHERE user_id=?').all(u.id).map(r => S.placeById(db, r.place_id)).filter(Boolean);
    out(c, 200, V.homeIn(c, { avatar: S.avatarGet(db, u.id), recent: ids, favs, friends: S.friendsOf(db, u.id), unread: db.prepare('SELECT COUNT(*) n FROM messages WHERE to_id=? AND read=0').get(u.id).n })); });

  // ---------------- games
  const PER = 24;
  add('GET', '/games', c => {
    const sort = ['favorite', 'recent'].includes(c.query.sort) && (c.query.sort !== 'recent' || c.user) ? c.query.sort : 'default';
    let list = S.placeList(db).map(withAvail); const favs = Object.fromEntries(list.map(g => [g.id, S.placeFavoriteCount(db, g.id)])); const statuses = Object.fromEntries(list.map(g => [g.id, (registry.byId(g.id) || {}).status]));
    if (sort === 'favorite') list = [...list].sort((a, b) => favs[b.id] - favs[a.id] || a.title.localeCompare(b.title)); if (sort === 'recent') list = S.recentPlaces(db, c.user.id).map(withAvail);
    out(c, 200, V.games(c, list, { sort, favs, statuses }));
  });
  add('GET', '/games/registry', c => {
    const status = registry.STATUSES.includes(c.query.status) ? c.query.status : ''; const all = registry.registry().filter(r => !status || r.status === status); const pg = paginate(all.length, c.query.page, 40);
    out(c, 200, V.gameRegistry(c, { rows: all.slice(pg.offset, pg.offset + pg.limit), status, counts: registry.counts(), pg }));
  });
  add('GET', '/games/:id', c => { const g = S.placeById(db, c.params.id); if (!g || !/^ACCEPTED/.test(g.status)) return out(c, 404, V.notFound(c));
    const entry = registry.byId(g.id); const srv = gameservers.servers.get(g.id);
    out(c, 200, V.gameDetails(c, g, { tab: ['compat', 'evidence'].includes(c.query.tab) ? c.query.tab : 'about', registry: entry, report: registry.compatReport(g.id), playability: registry.playability(entry, { storeHas: gameservers.available(g) }), running: srv ? { players: srv.gs.world.players.size, startedAt: srv.startedAt } : null,
      available: gameservers.available(g), favCount: S.placeFavoriteCount(db, g.id), launches: S.launchCount(db, g.id), fav: c.user ? !!db.prepare('SELECT 1 FROM favorites WHERE user_id=? AND place_id=?').get(c.user.id, g.id) : false })); });
  add('POST', '/games/:id/favorite', { auth: true }, c => { const g = S.placeById(db, c.params.id); if (!g || !/^ACCEPTED/.test(g.status)) return out(c, 404, V.notFound(c)); S.favorite(db, c.user.id, g.id, c.form.on === '1'); go(c, '/games/' + g.id); });
  // Shared by Play and Travel->Continue: the ONLY place launch tickets are issued. Gated by the registry (PRESERVED only) and the local store.
  async function startPlay(c, g) {
    const entry = registry.byId(g.id); const pl = registry.playability(entry, { storeHas: gameservers.available(g) }); if (!pl.eligible) return { ok: false, error: 'Not playable: ' + pl.reason };
    let srv; try { srv = await gameservers.ensure(g); } catch (e) { return { ok: false, error: 'Cannot start this game: ' + e.message }; }
    const ticket = auth.issueTicket(db, 'launch_tickets', { userId: c.user.id, placeId: g.id, serverId: srv.serverId, ttl: cfg.launchTicketTtlMs });
    db.prepare('INSERT OR REPLACE INTO recent_plays(user_id,place_id,at) VALUES(?,?,?)').run(c.user.id, g.id, Date.now());
    return { ok: true, uri: `bloxen-player:1+ticket:${ticket}+version:${cfg.clientVersion}` };
  }
  add('POST', '/games/:id/play', { auth: true }, async c => {
    const g = S.placeById(db, c.params.id); if (!g || !/^ACCEPTED/.test(g.status)) return out(c, 404, V.notFound(c));
    const r = await startPlay(c, g); if (!r.ok) return go(c, '/games/' + g.id, r.error, true);
    if (/application\/json/.test(c.req.headers.accept || '')) return out(c, 200, JSON.stringify({ uri: r.uri, expiresInSeconds: cfg.launchTicketTtlMs / 1000 }), { 'Content-Type': 'application/json' });
    out(c, 200, V.launchPage(c, g, r.uri));
  });
  // ---------------- travel (TeleportService records; Continue = a normal Play ticket)
  add('GET', '/my/travel', { auth: true }, c => out(c, 200, V.travel(c, db.prepare('SELECT t.*, pf.title from_title, pt.title to_title FROM teleports t LEFT JOIN places pf ON pf.id=t.from_place LEFT JOIN places pt ON pt.id=t.to_place WHERE t.user_id=? AND t.consumed_at IS NULL ORDER BY t.created_at DESC LIMIT 50').all(c.user.id))));
  add('POST', '/my/travel/:id/continue', { auth: true }, async c => {
    const t = db.prepare('SELECT * FROM teleports WHERE id=? AND user_id=? AND consumed_at IS NULL').get(+c.params.id, c.user.id); if (!t) return go(c, '/my/travel', 'That travel request is not pending.', true);
    const g = S.placeById(db, t.to_place); if (!g || !/^ACCEPTED/.test(g.status)) return go(c, '/my/travel', 'Destination is not available.', true);
    const r = await startPlay(c, g); if (!r.ok) return go(c, '/my/travel', r.error, true);
    db.prepare('UPDATE teleports SET consumed_at=? WHERE id=? AND consumed_at IS NULL').run(Date.now(), t.id); out(c, 200, V.launchPage(c, g, r.uri));
  });
  add('POST', '/my/travel/:id/dismiss', { auth: true }, c => { db.prepare('UPDATE teleports SET consumed_at=? WHERE id=? AND user_id=? AND consumed_at IS NULL').run(Date.now(), +c.params.id, c.user.id); go(c, '/my/travel', 'Dismissed.'); });

  // ---------------- catalog
  add('GET', '/catalog', c => { const q = { q: String(c.query.q || '').slice(0, 60), type: +c.query.type || 0, sort: ['relevance', 'updated', 'price_asc', 'price_desc'].includes(c.query.sort) ? c.query.sort : 'relevance' }; const pg = paginate(S.catalogCount(db, q), c.query.page, PER); out(c, 200, V.catalog(c, S.catalogList(db, { ...q, limit: pg.limit, offset: pg.offset }), q, pg)); });
  add('GET', '/catalog/item/:id', c => { const i = S.catalogItem(db, c.params.id); if (!i) return out(c, 404, V.notFound(c)); out(c, 200, V.catalogItem(c, i, { owned: c.user ? !!db.prepare('SELECT 1 FROM inventory WHERE user_id=? AND asset_id=?').get(c.user.id, i.asset_id) : false, fav: c.user ? S.isItemFavorite(db, c.user.id, i.asset_id) : false, favCount: S.itemFavoriteCount(db, i.asset_id), asset: db.prepare('SELECT * FROM assets WHERE id=?').get(i.asset_id) })); });
  add('POST', '/catalog/item/:id/favorite', { auth: true }, c => { const r = S.itemFavorite(db, c.user.id, c.params.id, c.form.on === '1'); go(c, '/catalog/item/' + +c.params.id, r.ok ? null : r.error, !r.ok); });
  add('POST', '/catalog/item/:id/buy', { auth: true }, c => { const r = S.acquire(db, c.user.id, c.params.id); go(c, '/catalog/item/' + +c.params.id, r.ok ? 'You now own ' + r.item.name + '.' : r.error, !r.ok); });

  // ---------------- character / inventory
  add('GET', '/my/character', { auth: true }, c => out(c, 200, V.character(c, { avatar: S.avatarGet(db, c.user.id), inv: S.inventory(db, c.user.id) })));
  add('POST', '/my/character/equip', { auth: true }, c => { const r = S.equip(db, c.user.id, c.form.asset); go(c, '/my/character', r.ok ? 'Now wearing ' + r.item.name + '.' : r.error, !r.ok); });
  add('POST', '/my/character/remove', { auth: true }, c => { S.unequip(db, c.user.id, c.form.asset); go(c, '/my/character', 'Removed.'); });
  add('POST', '/my/character/color', { auth: true }, c => { const r = S.setColor(db, c.user.id, String(c.form.part), +c.form.color); go(c, '/my/character', r.ok ? 'Body colour saved.' : r.error, !r.ok); });
  add('GET', '/my/inventory', { auth: true }, c => { const type = +c.query.type || 0; const counts = S.inventoryCounts(db, c.user.id); const total = type ? counts[type] || 0 : Object.values(counts).reduce((x, y) => x + y, 0); const pg = paginate(total, c.query.page, PER); out(c, 200, V.inventory(c, { type, counts, pg, items: S.inventoryPage(db, c.user.id, type, pg.limit, pg.offset) })); });
  add('GET', '/my/favorites', { auth: true }, c => { const type = +c.query.type || 0; out(c, 200, V.favorites(c, { type, places: S.favoritePlaces(db, c.user.id), items: S.favoriteItems(db, c.user.id, type) })); });
  add('GET', '/users', c => { const q = String(c.query.q || '').slice(0, 40); const pg = paginate(S.userSearch(db, q, { limit: 1 }).total, c.query.page, 20); out(c, 200, V.people(c, q, S.userSearch(db, q, { limit: pg.limit, offset: pg.offset }), pg)); });

  // ---------------- profile / friends / groups / messages
  add('GET', '/users/:id/profile', c => { const u = S.userById(db, c.params.id); if (!u) return out(c, 404, V.notFound(c)); let btn = '';
    if (c.user && c.user.id !== u.id) { if (S.areFriends(db, c.user.id, u.id)) btn = '<span class="badge g">Friends</span>'; else btn = V.postForm(c, '/my/friends/request', `<input type="hidden" name="username" value="${esc(u.username)}"><button class="btn-small btn-neutral">Add Friend</button>`); btn += ` <a class="btn-small btn-neutral" href="/my/messages?to=${encodeURIComponent(u.username)}">Message</a>`; }
    out(c, 200, V.profile(c, u, { avatar: S.avatarGet(db, u.id), friends: S.friendsOf(db, u.id), groups: db.prepare('SELECT g.id,g.name FROM group_members m JOIN groups g ON g.id=m.group_id WHERE m.user_id=?').all(u.id), friendBtn: btn, msg: '', online: false, favCat: String(c.query.fav || 'places'), favPlaces: S.favoritePlaces(db, u.id), favItems: S.favoriteItems(db, u.id, /^\d+$/.test(String(c.query.fav)) ? +c.query.fav : 0) })); });
  add('GET', '/my/friends', { auth: true }, c => out(c, 200, V.friends(c, { friends: S.friendsOf(db, c.user.id), requests: S.requestsFor(db, c.user.id) })));
  add('POST', '/my/friends/request', { auth: true }, c => { const r = S.sendFriendRequest(db, c.user.id, String(c.form.username || '')); go(c, '/my/friends', r.ok ? (r.pending ? 'Friend request sent.' : 'You are now friends.') : r.error, !r.ok); });
  add('POST', '/my/friends/respond', { auth: true }, c => { const r = S.respondFriend(db, c.user.id, c.form.id, c.form.accept === '1'); go(c, '/my/friends', r.ok ? 'Done.' : r.error, !r.ok); });
  add('POST', '/my/friends/remove', { auth: true }, c => { const x = +c.form.id, me = c.user.id; const [a, b] = me < x ? [me, x] : [x, me]; db.prepare('DELETE FROM friends WHERE a=? AND b=?').run(a, b); go(c, '/my/friends', 'Removed.'); });
  add('GET', '/my/groups', { auth: true }, c => out(c, 200, V.groups(c, { mine: db.prepare('SELECT g.id,g.name,m.role FROM group_members m JOIN groups g ON g.id=m.group_id WHERE m.user_id=?').all(c.user.id), all: db.prepare('SELECT g.id,g.name,(SELECT COUNT(*) FROM group_members WHERE group_id=g.id) members FROM groups g ORDER BY g.name').all() })));
  add('POST', '/my/groups/create', { auth: true }, c => { const r = S.createGroup(db, c.user.id, c.form.name, c.form.description); go(c, r.ok ? '/groups/' + r.id : '/my/groups', r.ok ? 'Group created.' : r.error, !r.ok); });
  add('GET', '/groups/:id', c => { const g = db.prepare('SELECT * FROM groups WHERE id=?').get(+c.params.id); if (!g) return out(c, 404, V.notFound(c)); out(c, 200, V.group(c, g, { owner: S.userById(db, g.owner_id), members: db.prepare('SELECT u.id,u.username,m.role FROM group_members m JOIN users u ON u.id=m.user_id WHERE m.group_id=?').all(g.id), member: c.user ? !!db.prepare('SELECT 1 FROM group_members WHERE group_id=? AND user_id=?').get(g.id, c.user.id) : false })); });
  add('POST', '/groups/:id/join', { auth: true }, c => { const r = S.joinGroup(db, c.user.id, c.params.id); go(c, '/groups/' + +c.params.id, r.ok ? 'Joined.' : r.error, !r.ok); });
  add('GET', '/my/messages', { auth: true }, c => out(c, 200, V.messages(c, { msgs: db.prepare('SELECT m.*, u.username from_name FROM messages m JOIN users u ON u.id=m.from_id WHERE m.to_id=? ORDER BY m.created_at DESC LIMIT 100').all(c.user.id) }, { to: String(c.query.to || '').slice(0, 20) })));
  add('GET', '/my/messages/:id', { auth: true }, c => { const m = db.prepare('SELECT m.*, u.username from_name FROM messages m JOIN users u ON u.id=m.from_id WHERE m.id=? AND m.to_id=?').get(+c.params.id, c.user.id); if (!m) return out(c, 404, V.notFound(c)); db.prepare('UPDATE messages SET read=1 WHERE id=?').run(m.id); out(c, 200, V.message(c, m)); });
  add('POST', '/my/messages/send', { auth: true }, c => { const r = S.sendMessage(db, c.user.id, String(c.form.to || ''), c.form.subject, c.form.body); go(c, '/my/messages', r.ok ? 'Message sent.' : r.error, !r.ok); });
  add('GET', '/search', c => { const q = String(c.query.q || '').slice(0, 60); out(c, 200, V.searchPage(c, q, S.search(db, q))); });
  add('GET', '/develop', c => out(c, 200, V.develop(c, S.placeList(db), db.prepare('SELECT * FROM places ORDER BY sort,title').all())));

  // ---------------- account
  add('GET', '/my/account', { auth: true }, c => out(c, 200, V.account(c)));
  add('POST', '/my/account/blurb', { auth: true }, c => { db.prepare('UPDATE users SET blurb=? WHERE id=?').run(String(c.form.blurb || '').slice(0, 1000), c.user.id); go(c, '/my/account', 'Saved.'); });
  add('POST', '/my/account/password', { auth: true }, async c => { if (!(await auth.verifyPassword(String(c.form.current || ''), c.user.pass_hash))) return go(c, '/my/account', 'Current password is incorrect.', true); if (!auth.validPassword(String(c.form.password || ''))) return go(c, '/my/account', 'New password must be 8-200 characters.', true);
    db.prepare('UPDATE users SET pass_hash=? WHERE id=?').run(await auth.hashPassword(c.form.password), c.user.id); db.prepare('DELETE FROM sessions WHERE user_id=? AND token_hash<>?').run(c.user.id, auth.sha(c.token)); go(c, '/my/account', 'Password changed; other sessions were logged out.'); });
  add('POST', '/my/account/logout-all', { auth: true }, c => { db.prepare('DELETE FROM sessions WHERE user_id=?').run(c.user.id); c.setCookies.push(cookie(SESSION, '', { maxAge: 0 })); go(c, '/', 'Logged out everywhere.'); });

  return (req, res) => handle(req, res).catch(e => { log('fatal ' + (e.stack || e)); if (!res.headersSent) send(res, e.status || 500, e.status === 413 ? 'Payload too large' : 'Server error', { 'Content-Type': 'text/plain' }); });
}
module.exports = { createWebApp };
