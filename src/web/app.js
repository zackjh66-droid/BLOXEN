'use strict';
const fs = require('fs'); const path = require('path'); const http = require('http');
const cfg = require('../lib/config'); const auth = require('../lib/auth'); const S = require('../lib/services'); const V = require('./views');
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
    if (!auth.validUsername(name)) return fail('Usernames are 3-20 characters: letters, numbers and single underscores, not starting or ending with _.'); if (!auth.validPassword(pw)) return fail('Password must be 8-200 characters.'); if (pw.toLowerCase() === name.toLowerCase()) return fail('Password cannot be your username.');
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
  add('GET', '/games', c => out(c, 200, V.games(c, S.placeList(db).map(withAvail))));
  add('GET', '/games/:id', c => { const g = S.placeById(db, c.params.id); if (!g || !/^ACCEPTED/.test(g.status)) return out(c, 404, V.notFound(c)); out(c, 200, V.gameDetails(c, g, { available: gameservers.available(g), fav: c.user ? !!db.prepare('SELECT 1 FROM favorites WHERE user_id=? AND place_id=?').get(c.user.id, g.id) : false })); });
  add('POST', '/games/:id/favorite', { auth: true }, c => { const g = S.placeById(db, c.params.id); if (!g) return out(c, 404, V.notFound(c)); S.favorite(db, c.user.id, g.id, c.form.on === '1'); go(c, '/games/' + g.id); });
  add('POST', '/games/:id/play', { auth: true }, async c => {
    const g = S.placeById(db, c.params.id); if (!g || !/^ACCEPTED/.test(g.status)) return out(c, 404, V.notFound(c));
    let srv; try { srv = await gameservers.ensure(g); } catch (e) { return go(c, '/games/' + g.id, 'Cannot start this game: ' + e.message, true); }
    const ticket = auth.issueTicket(db, 'launch_tickets', { userId: c.user.id, placeId: g.id, serverId: srv.serverId, ttl: cfg.launchTicketTtlMs });
    db.prepare('INSERT OR REPLACE INTO recent_plays(user_id,place_id,at) VALUES(?,?,?)').run(c.user.id, g.id, Date.now());
    const uri = `bloxen-player:1+ticket:${ticket}+version:${cfg.clientVersion}`;
    if (/application\/json/.test(c.req.headers.accept || '')) return out(c, 200, JSON.stringify({ uri, expiresInSeconds: cfg.launchTicketTtlMs / 1000 }), { 'Content-Type': 'application/json' });
    out(c, 200, V.launchPage(c, g, uri));
  });

  // ---------------- catalog
  add('GET', '/catalog', c => { const q = { q: String(c.query.q || '').slice(0, 60), type: +c.query.type || 0, sort: c.query.sort || 'relevance' }; out(c, 200, V.catalog(c, S.catalogList(db, q), q)); });
  add('GET', '/catalog/item/:id', c => { const i = S.catalogItem(db, c.params.id); if (!i) return out(c, 404, V.notFound(c)); out(c, 200, V.catalogItem(c, i, { owned: c.user ? !!db.prepare('SELECT 1 FROM inventory WHERE user_id=? AND asset_id=?').get(c.user.id, i.asset_id) : false, asset: db.prepare('SELECT * FROM assets WHERE id=?').get(i.asset_id) })); });
  add('POST', '/catalog/item/:id/buy', { auth: true }, c => { const r = S.acquire(db, c.user.id, c.params.id); go(c, '/catalog/item/' + +c.params.id, r.ok ? 'You now own ' + r.item.name + '.' : r.error, !r.ok); });

  // ---------------- character / inventory
  add('GET', '/my/character', { auth: true }, c => out(c, 200, V.character(c, { avatar: S.avatarGet(db, c.user.id), inv: S.inventory(db, c.user.id) })));
  add('POST', '/my/character/equip', { auth: true }, c => { const r = S.equip(db, c.user.id, c.form.asset); go(c, '/my/character', r.ok ? 'Now wearing ' + r.item.name + '.' : r.error, !r.ok); });
  add('POST', '/my/character/remove', { auth: true }, c => { S.unequip(db, c.user.id, c.form.asset); go(c, '/my/character', 'Removed.'); });
  add('POST', '/my/character/color', { auth: true }, c => { const r = S.setColor(db, c.user.id, String(c.form.part), +c.form.color); go(c, '/my/character', r.ok ? 'Body colour saved.' : r.error, !r.ok); });
  add('GET', '/my/inventory', { auth: true }, c => out(c, 200, V.inventory(c, S.inventory(db, c.user.id, +c.query.type || 0), +c.query.type || 0)));

  // ---------------- profile / friends / groups / messages
  add('GET', '/users/:id/profile', c => { const u = S.userById(db, c.params.id); if (!u) return out(c, 404, V.notFound(c)); let btn = '';
    if (c.user && c.user.id !== u.id) { if (S.areFriends(db, c.user.id, u.id)) btn = '<span class="badge g">Friends</span>'; else btn = `<form method="post" action="/my/friends/request" style="display:inline"><input type="hidden" name="_csrf" value="${esc(c.csrf)}"><input type="hidden" name="username" value="${esc(u.username)}"><button class="btn-small btn-neutral">Add Friend</button></form>`; btn += ` <a class="btn-small btn-neutral" href="/my/messages?to=${encodeURIComponent(u.username)}">Message</a>`; }
    out(c, 200, V.profile(c, u, { avatar: S.avatarGet(db, u.id), friends: S.friendsOf(db, u.id), groups: db.prepare('SELECT g.id,g.name FROM group_members m JOIN groups g ON g.id=m.group_id WHERE m.user_id=?').all(u.id), friendBtn: btn, msg: '' })); });
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
