'use strict';
// 2015-evidence page refit: components, pagination, favorites, registry gating, travel, people search. SIMULATOR/UNIT level.
const test = require('node:test'); const assert = require('node:assert/strict');
const { start } = require('../src/web/main'); const { Browser } = require('./helpers'); const C = require('../src/web/components');
let app; test.before(async () => { app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true }); }); test.after(() => app.close());
const uid = () => 'u' + Math.random().toString(36).slice(2, 9);
async function user() { const b = new Browser(app.webUrl); const name = uid(); await b.register(name); const id = app.db.prepare('SELECT id FROM users WHERE username=?').get(name).id; return { b, name, id }; }

test('components: escape everything, buttons/tabs/pager behave', () => {
  assert.match(C.btn('<x>', { href: '/a"b' }), /&lt;x&gt;/); assert.ok(!C.btn('x', { href: '/a"b' }).includes('/a"b'));
  assert.match(C.btn('Play', { disabled: true, title: 'why' }), /btn-disabled/); assert.equal(C.pager({ page: 1, pages: 1, hrefFn: () => '' }), '');
  const p = C.pager({ page: 2, pages: 3, hrefFn: n => '/x?page=' + n }); assert.match(p, /Page 2 of 3/); assert.match(p, /page=1/); assert.match(p, /page=3/);
  assert.deepEqual(C.paginate(100, 99, 24), { page: 5, pages: 5, offset: 96, limit: 24, total: 100 }); assert.equal(C.paginate(0, 'x', 24).page, 1); assert.equal(C.paginate(10, -5, 24).page, 1);
  assert.match(C.tabs([{ label: 'A', href: '/a', selected: true }, { label: 'B', disabled: true, title: 't' }]), /class="sel"/);
  assert.match(C.statusBadge('PRESERVED'), /badge g/); assert.match(C.statusBadge('REJECTED'), /badge r/);
});
test('chrome: fixed blue header, nav only lists pages that exist, footer is labelled, no inline style anywhere', async () => {
  const { b } = await user(); for (const p of ['/home', '/games', '/games/registry', '/catalog', '/my/inventory', '/my/favorites', '/my/travel', '/users', '/my/character', '/develop', '/about']) {
    const r = await b.get(p); assert.equal(r.status, 200, p); assert.ok(!/\sstyle=/.test(r.text), 'inline style on ' + p); assert.match(r.text, /id="Footer"/); assert.match(r.text, /unofficial/i); }
  const t = (await b.get('/home')).text; assert.match(t, /class="rbx-header/); assert.match(t, /<a href="\/games">Games<\/a>/); assert.ok(!/ROBUX/.test(t.split('id="Container"')[0]), 'no ROBUX nav tab');
  for (const href of [...t.matchAll(/id="navContent".*?<\/ul>/gs)][0][0].match(/href="([^"]+)"/g).map(x => x.slice(6, -1))) { const r = await b.get(href); assert.equal(r.status, 200, 'left nav link ' + href); }
  const css = await (await fetch(app.webUrl + '/static/css/bloxen.css')).text(); assert.match(css, /\.rbx-header \{[^}]*#0074bd/); assert.match(css, /min-width: 1480px/);
});
test('catalog pagination: pages differ, clamp, type filter keeps count, every page lists real items only', async () => {
  const b = new Browser(app.webUrl); const p1 = (await b.get('/catalog')).text, p2 = (await b.get('/catalog?page=2')).text; assert.match(p1, /Page 1 of \d+/); assert.match(p2, /Page 2 of \d+/);
  const ids = t => [...t.matchAll(/href="\/catalog\/item\/(\d+)"/g)].map(m => m[1]); const a = new Set(ids(p1)), bb = ids(p2); assert.ok(a.size === 24 && bb.length > 0 && bb.every(x => !a.has(x)), 'page 2 has different items');
  assert.match((await b.get('/catalog?page=9999')).text, /Page \d+ of \d+/); assert.equal((await b.get('/catalog?page=abc')).status, 200);
  const total = app.db.prepare('SELECT COUNT(*) n FROM catalog_items').get().n; assert.match(p1, new RegExp(total + ' item\\(s\\)'));
  const hats = (await b.get('/catalog?type=8')).text; assert.ok(ids(hats).every(id => app.db.prepare('SELECT type_id FROM catalog_items WHERE asset_id=?').get(+id).type_id === 8));
  assert.ok(!(await b.get('/catalog?sort=1;drop table users')).text.includes('drop table'));
});
test('item favorites: CSRF-protected, toggles, counted per item, appear on favorites page and profile category', async () => {
  const { b, id } = await user(); const item = app.db.prepare('SELECT asset_id,name,type_id FROM catalog_items WHERE type_id=8 LIMIT 1').get(); const page = '/catalog/item/' + item.asset_id;
  assert.equal((await b.req('POST', page + '/favorite', { form: { on: '1' } })).status, 403);
  assert.match((await b.get(page)).text, /Favorited<\/span> <b>0<\/b>/); await b.post(page + '/favorite', { on: '1' }, { page }); assert.match((await b.get(page)).text, /Favorited<\/span> <b>1<\/b>/);
  assert.match((await b.get('/my/favorites')).text, new RegExp(item.name.replace(/[()]/g, '.'))); assert.match((await b.get(`/users/${id}/profile?fav=8`)).text, new RegExp(String(item.asset_id)));
  assert.ok(!(await b.get(`/users/${id}/profile?fav=11`)).text.includes(`/catalog/item/${item.asset_id}"`), 'other category excludes it');
  await b.post(page + '/favorite', { on: '0' }, { page }); assert.match((await b.get(page)).text, /Favorited<\/span> <b>0<\/b>/);
  assert.equal((await b.post('/catalog/item/999999999/favorite', { on: '1' }, { page })).status, 303);
});
test('game details: registry status, three tabs, compat report, provenance; Top Favorite sort follows local favorites', async () => {
  const { b } = await user(); const t = (await b.get('/games/crossroads-uncopylocked-commit')).text;
  assert.match(t, /badge g" title="BLOXEN game registry status">PRESERVED/); assert.match(t, /GEOMETRY-ONLY/); assert.match(t, /Real client: <span class="badge r">NOT TESTED/); assert.match(t, /SHA-256/); assert.match(t, /Running Games/);
  const c = (await b.get('/games/crossroads-uncopylocked-commit?tab=compat')).text; assert.match(c, /Importer/); assert.match(c, /unsupported-local-destination/); assert.match(c, /REAL-CLIENT-TESTED: no/); assert.match(c, /14403/);
  assert.match((await b.get('/games/crossroads-uncopylocked-commit?tab=evidence')).text, /Acquisition method/);
  await b.post('/games/tabularasa/favorite', { on: '1' }, { page: '/games/tabularasa' }); assert.match((await b.get('/games/tabularasa')).text, /Favorited<\/span> <b>1<\/b>/);
  const g = (await b.get('/games?sort=favorite')).text; assert.ok(g.indexOf('/games/tabularasa"') < g.indexOf('/games/robloxhq"'), 'most favorited first');
  assert.match((await b.get('/games?sort=recent')).text, /Recently Played/); const anon = (await new Browser(app.webUrl).get('/games?sort=recent')).text; assert.ok(!/Recently Played/.test(anon), 'recent is login-only');
});
test('registry page lists everything, filters by status, never offers non-PRESERVED as a game; unknown places have no game page', async () => {
  const b = new Browser(app.webUrl); const r = (await b.get('/games/registry')).text; assert.match(r, /PRESERVED \(7\)/); assert.match(r, /UNKNOWN \(112\)/); assert.match(r, /QUARANTINED \(3\)/); assert.match(r, /REJECTED \(3\)/);
  const rej = (await b.get('/games/registry?status=REJECTED')).text; assert.ok(!/badge g" title="BLOXEN game registry status">PRESERVED/.test(rej)); assert.ok(!/<td><a href="\/games\//.test(rej), 'rejected rows are not linked');
  assert.match((await b.get('/games/registry?status=UNKNOWN&page=2')).text, /Page 2 of 3/); const first = require('../src/lib/registry').registry().find(x => x.status === 'UNKNOWN');
  assert.equal((await b.get('/games/' + encodeURIComponent(first.id))).status, 404); assert.equal((await b.get('/games/quarantined-x')).status, 404);
  const list = (await b.get('/games')).text; assert.equal([...list.matchAll(/href="\/games\/([^"]+)"><div class="thumb/g)].length, 7);
});
test('play is gated by the registry even if the places table were tampered with', async () => {
  const { b } = await user(); app.db.prepare("INSERT INTO places(id,title,creator,tier,grade,status,sha256,size,format,stats,evidence,date_note,method,public_evidence,source,notes,compat,playable,sort) VALUES('unknown:zz','Fake','x','Q','UNKNOWN','ACCEPTED','0','0','rbxl','{}','[]','','','','{}','','', 'GEOMETRY-ONLY',9)").run();
  const before = app.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n; const r = await b.post('/games/unknown:zz/play', {}, { page: '/games/unknown:zz' }); assert.equal(r.status, 303); assert.match((await b.follow(r)).text, /Not playable: registry status is absent/);
  assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n, before, 'no ticket created'); assert.match((await b.get('/games/unknown:zz')).text, /btn-disabled/);
});
test('inventory categories with counts and pagination, only for the owner', async () => {
  const { b } = await user(); const items = app.db.prepare('SELECT asset_id,type_id FROM catalog_items WHERE price_robux IS NOT NULL AND limited_unique=0 AND resale_only=0 AND min_membership=0 LIMIT 3').all();
  for (const i of items) await b.post(`/catalog/item/${i.asset_id}/buy`, {}, { page: `/catalog/item/${i.asset_id}` });
  const inv = (await b.get('/my/inventory')).text; assert.match(inv, /All<\/a> <span class="tip">\(3\)/); const t0 = items[0].type_id; assert.match((await b.get('/my/inventory?type=' + t0)).text, new RegExp(`/catalog/item/${items[0].asset_id}"`));
  assert.ok(!(await b.get('/my/inventory?type=' + t0 + '&page=50')).text.includes('undefined')); assert.equal((await new Browser(app.webUrl).get('/my/inventory')).status, 303);
});
test('people search: escaped, LIKE-wildcards literal, paginated', async () => {
  const { b } = await user(); const a = await user(); const p = (await b.get('/users?q=' + a.name)).text; assert.match(p, new RegExp(`/users/${a.id}/profile`));
  assert.ok(!(await b.get('/users?q=' + encodeURIComponent('<script>x</script>'))).text.includes('<script>x')); assert.match((await b.get('/users?q=' + encodeURIComponent('%'))).text, /0 user\(s\)/);
});
test('travel: pending teleports list, Continue issues a normal scoped ticket once, other users cannot use it, Dismiss works', async () => {
  const a = await user(), o = await user(); const ins = (u, to) => Number(app.db.prepare('INSERT INTO teleports(user_id,from_place,to_place,roblox_place_id,created_at) VALUES(?,?,?,?,?)').run(u, 'crossroads-2007-client', to, 1501, Date.now()).lastInsertRowid);
  const t1 = ins(a.id, 'roblox-world-headquarters'), t2 = ins(a.id, 'unknown:zz');
  const pg = (await a.b.get('/my/travel')).text; assert.match(pg, /Continue/); assert.match((await o.b.get('/my/travel')).text, /No pending travel/);
  assert.match((await o.b.follow(await o.b.post(`/my/travel/${t1}/continue`, {}, { page: '/my/travel' }))).text, /not pending/);
  const before = app.db.prepare('SELECT COUNT(*) n FROM launch_tickets WHERE user_id=?').get(a.id).n;
  const r = await a.b.post(`/my/travel/${t1}/continue`, {}, { page: '/my/travel' }); assert.equal(r.status, 200); assert.match(r.text, /bloxen-player:1\+ticket:[A-Za-z0-9_-]+\+version:version-0d46087630eb46cd/);
  const row = app.db.prepare('SELECT * FROM launch_tickets WHERE user_id=? ORDER BY created_at DESC').get(a.id); assert.equal(row.place_id, 'roblox-world-headquarters'); assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets WHERE user_id=?').get(a.id).n, before + 1);
  assert.match((await a.b.follow(await a.b.post(`/my/travel/${t1}/continue`, {}, { page: '/my/travel' }))).text, /not pending/, 'single use');
  assert.match((await a.b.follow(await a.b.post(`/my/travel/${t2}/continue`, {}, { page: '/my/travel' }))).text, /Destination is not available|Not playable/);
  const t3 = ins(a.id, 'tabularasa'); await a.b.post(`/my/travel/${t3}/dismiss`, {}, { page: '/my/travel' }); assert.ok(app.db.prepare('SELECT consumed_at FROM teleports WHERE id=?').get(t3).consumed_at);
});
