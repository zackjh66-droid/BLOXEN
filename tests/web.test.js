'use strict';
const test = require('node:test'); const assert = require('node:assert/strict');
const { start } = require('../src/web/main'); const { Browser } = require('./helpers');
let app; test.before(async () => { app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true }); }); test.after(() => app.close());
const uid = () => 'u' + Math.random().toString(36).slice(2, 10);

test('security headers and static path traversal', async () => {
  const r = await fetch(app.webUrl + '/'); assert.match(r.headers.get('content-security-policy'), /script-src 'self'/); assert.equal(r.headers.get('x-frame-options'), 'DENY');
  for (const p of ['/static/../package.json', '/static/%2e%2e/package.json', '/static/..%2f..%2fsrc/lib/db.js', '/static//etc/passwd']) { const x = await fetch(app.webUrl + p); assert.equal(x.status, 404, p); }
  assert.equal((await fetch(app.webUrl + '/static/css/bloxen.css')).status, 200);
});
test('register validation, login, logout, sessions', async () => {
  const b = new Browser(app.webUrl); const name = uid();
  assert.equal((await b.register('x')).status, 400); assert.equal((await b.register('bad name!')).status, 400); assert.equal((await b.register('a__b_c1')).status, 400);
  const r = await b.register(name); assert.equal(r.status, 303); assert.ok(b.jar.has('bloxen_session')); const home = await b.follow(r); assert.match(home.text, new RegExp('Hello, ' + name));
  assert.equal((await new Browser(app.webUrl).register(name)).status, 400, 'duplicate username');
  const cookieHdr = (await fetch(app.webUrl + '/register', { method: 'GET' })).headers.get('set-cookie'); assert.match(cookieHdr, /HttpOnly/); assert.match(cookieHdr, /SameSite=Lax/);
  await b.post('/logout', {}, { page: '/home' }); assert.equal((await b.get('/home')).status, 303);
  const b2 = new Browser(app.webUrl); assert.equal((await b2.login(name, 'wrong password')).status, 400); const ok = await b2.login(name); assert.equal(ok.status, 303); assert.equal((await b2.get('/home')).status, 200);
  const hash = app.db.prepare('SELECT pass_hash FROM users WHERE username=?').get(name).pass_hash; assert.match(hash, /^scrypt\$/); assert.ok(!hash.includes('correct'));
});
test('CSRF: POST without a valid token is rejected', async () => {
  const b = new Browser(app.webUrl); await b.register(uid()); const r = await b.req('POST', '/my/account/blurb', { form: { blurb: 'x' } }); assert.equal(r.status, 403);
  const r2 = await b.req('POST', '/my/account/blurb', { form: { blurb: 'x', _csrf: 'nope' } }); assert.equal(r2.status, 403);
  const anon = await new Browser(app.webUrl).req('POST', '/login', { form: { username: 'a', password: 'b' } }); assert.equal(anon.status, 403);
});
test('XSS: user input is escaped everywhere it is rendered', async () => {
  const b = new Browser(app.webUrl); const name = uid(); await b.register(name); const evil = '<script>alert(1)</script><img src=x onerror=alert(2)>';
  await b.post('/my/account/blurb', { blurb: evil }, { page: '/my/account' }); const p = await b.get('/users/' + app.db.prepare('SELECT id FROM users WHERE username=?').get(name).id + '/profile'); assert.ok(!p.text.includes('<script>alert(1)'), 'blurb escaped'); assert.ok(p.text.includes('&lt;script&gt;'));
  const s = await b.get('/search?q=' + encodeURIComponent(evil)); assert.ok(!s.text.includes('<script>alert(1)')); const c = await b.get('/catalog?q=' + encodeURIComponent('"><script>x</script>')); assert.ok(!c.text.includes('"><script>x'));
});
test('SQL injection strings are inert', async () => {
  const b = new Browser(app.webUrl); await b.register(uid()); for (const q of ["' OR 1=1 --", "'; DROP TABLE users; --", '%', '_']) { const r = await b.get('/catalog?q=' + encodeURIComponent(q)); assert.equal(r.status, 200); const s = await b.get('/search?q=' + encodeURIComponent(q)); assert.equal(s.status, 200); }
  assert.ok(app.db.prepare('SELECT COUNT(*) n FROM users').get().n >= 1); assert.equal((await b.get('/catalog/item/1%20OR%201=1')).status, 404);
});
test('open redirect is blocked after login', async () => {
  const name = uid(); await new Browser(app.webUrl).register(name); for (const next of ['//evil.com', 'https://evil.com', '/\\evil.com', 'javascript:alert(1)']) { const b = new Browser(app.webUrl); const pg = await b.get('/login'); const r = await b.req('POST', '/login', { form: { username: name, password: 'correct horse 9', next, _csrf: b.csrfFrom(pg.text) } }); assert.equal(r.location, '/home', next); }
});
test('login throttling after repeated failures', async () => {
  const name = uid(); await new Browser(app.webUrl).register(name); const b = new Browser(app.webUrl); let last; for (let i = 0; i < 10; i++) last = await b.login(name, 'wrong-' + i); assert.equal(last.status, 429); assert.equal((await b.login(name)).status, 429);
});
test('catalog: only archived items; buy, insufficient funds, limited sold out; equip requires ownership', async () => {
  const b = new Browser(app.webUrl); const name = uid(); await b.register(name); const id = app.db.prepare('SELECT id FROM users WHERE username=?').get(name).id;
  const cat = await b.get('/catalog'); assert.match(cat.text, /Doge/); assert.match(cat.text, /ARCHIVED-NEAR-DATE|archived 2015-03-09/);
  const item = await b.get('/catalog/item/151784320'); assert.match(item.text, /Wayback 20150309034826/); assert.match(item.text, /MISSING/);
  assert.match((await b.follow(await b.post('/my/character/equip', { asset: 151784320 }, { page: '/my/character' }))).text, /do not own/);
  assert.match((await b.follow(await b.post('/catalog/item/151784320/buy', {}, { page: '/catalog/item/151784320' }))).text, /You now own Doge/);
  assert.equal(app.db.prepare('SELECT robux FROM users WHERE id=?').get(id).robux, 100000 - 250);
  assert.match((await b.follow(await b.post('/catalog/item/151784320/buy', {}, { page: '/catalog/item/151784320' }))).text, /already own/);
  assert.match((await b.follow(await b.post('/catalog/item/63043890/buy', {}, { page: '/catalog/item/63043890' }))).text, /Sold out/);
  app.db.prepare('UPDATE users SET robux=10 WHERE id=?').run(id); assert.match((await b.follow(await b.post('/catalog/item/223826660/buy', {}, { page: '/catalog/item/223826660' }))).text, /Not enough R\$/);
  assert.match((await b.follow(await b.post('/my/character/equip', { asset: 151784320 }, { page: '/my/character' }))).text, /Now wearing Doge/);
  assert.equal(app.db.prepare('SELECT COUNT(*) n FROM avatar_items WHERE user_id=?').get(id).n, 1);
  assert.match((await b.get('/my/inventory?type=8')).text, /Doge/); assert.match((await b.follow(await b.post('/my/character/color', { part: 'head_color', color: 24 }, { page: '/my/character' }))).text, /Body colour saved/);
  assert.match((await b.follow(await b.post('/my/character/color', { part: 'head_color; DROP', color: 24 }, { page: '/my/character' }))).text, /Invalid body colour/);
});
test('friends, messages, groups', async () => {
  const a = new Browser(app.webUrl), c = new Browser(app.webUrl); const na = uid(), nc = uid(); await a.register(na); await c.register(nc);
  await a.post('/my/friends/request', { username: nc }, { page: '/my/friends' }); assert.match((await c.get('/my/friends')).text, new RegExp(na));
  const aid = app.db.prepare('SELECT id FROM users WHERE username=?').get(na).id; await c.post('/my/friends/respond', { id: aid, accept: 1 }, { page: '/my/friends' }); assert.match((await a.get('/my/friends')).text, new RegExp(nc));
  await a.post('/my/messages/send', { to: nc, subject: 'Hi <b>', body: 'hello' }, { page: '/my/messages' }); const inbox = await c.get('/my/messages'); assert.match(inbox.text, /Hi &lt;b&gt;/);
  const mid = /\/my\/messages\/(\d+)/.exec(inbox.text)[1]; assert.match((await c.get('/my/messages/' + mid)).text, /hello/); assert.equal((await a.get('/my/messages/' + mid)).status, 404, 'cannot read others\' mail');
  const g = await a.post('/my/groups/create', { name: 'Preservers ' + uid(), description: 'x' }, { page: '/my/groups' }); assert.equal(g.status, 303); assert.match(g.location, /\/groups\/\d+/); assert.match((await c.get(g.location)).text, /Join Group/);
});
test('all required page types render (logged out and in)', async () => {
  const b = new Browser(app.webUrl); const pub = ['/', '/games', '/games/tabularasa', '/catalog', '/catalog/item/223785473', '/login', '/register', '/develop', '/search?q=a'];
  for (const p of pub) assert.equal((await b.get(p)).status, 200, p); await b.register(uid()); for (const p of ['/home', '/my/character', '/my/inventory', '/my/friends', '/my/groups', '/my/messages', '/my/account']) assert.equal((await b.get(p)).status, 200, p);
  assert.match((await new Browser(app.webUrl).get('/')).text, /unofficial, non-commercial preservation project/);
});
test('rejected/quarantined places are never offered', async () => {
  const b = new Browser(app.webUrl); assert.equal((await b.get('/games/natural-disaster-survival')).status, 404); assert.equal((await b.get('/games/roblox-evil-game-idk-stolen')).status, 404); const dev = await b.get('/develop'); assert.match(dev.text, /QUARANTINED/); assert.match(dev.text, /REJECTED/);
  assert.ok(!(await b.get('/games')).text.includes('Natural Disaster'));
});
test('game page states limits honestly', async () => { const t = (await new Browser(app.webUrl).get('/games/crossroads-2007-client')).text; assert.match(t, /no script runtime/); assert.match(t, /SHA-256/); assert.match(t, /INFERRED/); });

test('CSP-compatible markup: no inline style attributes; every BrickColor has a CSS class', async t => {
  const fs = require('fs'); const path = require('path'); const S = require('../src/lib/services');
  const views = fs.readFileSync(path.join(__dirname, '..', 'src', 'web', 'views.js'), 'utf8'); assert.ok(!/\sstyle=/.test(views), 'inline style= would be blocked by CSP');
  const css = fs.readFileSync(path.join(__dirname, '..', 'static', 'css', 'brickcolors.css'), 'utf8'); for (const id of Object.keys(S.BRICK_COLORS)) assert.ok(css.includes(`.bc-${id} {`), 'missing .bc-' + id);
});
