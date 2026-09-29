'use strict';
// End-to-end WITHOUT the Windows client. Everything here is SIMULATOR-TESTED: the BLOXEN simulator client stands in for RobloxPlayerBeta.exe.
// register -> login -> Home -> Games -> evidence-backed game -> Catalog -> acquire/equip -> Character -> Inventory -> Profile -> Friends -> Play -> ticket ->
// launcher contract -> compat backend -> game server -> parsed historical place -> simulated client in-world.
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const os = require('os'); const path = require('path'); const crypto = require('crypto');
const { start } = require('../src/web/main'); const { Browser, XML_PLACE } = require('./helpers'); const L = require('../src/launcher/launcher'); const { SimClient } = require('../src/sim/client');

const REAL_STORE = path.join(__dirname, '..', 'preservation', 'store');
function setup(useReal) {
  const store = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-store-')); let placeId = 'tabularasa';
  if (useReal) { const real = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'preservation', 'manifests', 'places.json'))).items.find(p => p.id === 'tabularasa'); fs.copyFileSync(path.join(REAL_STORE, real.sha256 + '.rbx'), path.join(store, real.sha256 + '.rbx')); }
  return { store, placeId };
}
async function flow(t, { real }) {
  const { store } = setup(real); const app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true, storeDir: store }); t.after(() => app.close());
  if (!real) { // TEST-ONLY synthetic place record (clearly not a historical place); never present in production DB
    const buf = Buffer.from(XML_PLACE); const sha = crypto.createHash('sha256').update(buf).digest('hex'); fs.writeFileSync(path.join(store, sha + '.rbx'), buf);
    app.db.prepare("UPDATE places SET sha256=? WHERE id='tabularasa'").run(sha); }
  const b = new Browser(app.webUrl); const name = 'e2e' + Math.random().toString(36).slice(2, 8);
  assert.match((await b.get('/')).text, /You Make the Game/);
  const reg = await b.register(name); assert.equal(reg.status, 303); const home = await b.follow(reg); assert.match(home.text, /Hello,/);
  assert.match((await b.get('/games')).text, /Tabula Rasa/); const gp = await b.get('/games/tabularasa'); assert.match(gp.text, /SHA-256/); assert.match(gp.text, /Evidence/);
  await b.follow(await b.post('/catalog/item/151784320/buy', {}, { page: '/catalog/item/151784320' })); await b.follow(await b.post('/my/character/equip', { asset: 151784320 }, { page: '/my/character' }));
  await b.follow(await b.post('/my/character/color', { part: 'torso_color', color: 23 }, { page: '/my/character' })); assert.match((await b.get('/my/character')).text, /Doge/); assert.match((await b.get('/my/inventory')).text, /Doge/);
  const uid = app.db.prepare('SELECT id FROM users WHERE username=?').get(name).id; assert.equal((await b.get(`/users/${uid}/profile`)).status, 200);
  const friend = new Browser(app.webUrl); const fname = 'fr' + Math.random().toString(36).slice(2, 8); await friend.register(fname); await b.post('/my/friends/request', { username: fname }, { page: '/my/friends' });
  // ---- Play -> ticket
  const play = await b.req('POST', '/games/tabularasa/play', { form: { _csrf: b.csrfFrom((await b.get('/games/tabularasa')).text) }, headers: { accept: 'application/json' } }); assert.equal(play.status, 200, play.text); const { uri } = JSON.parse(play.text);
  assert.match(uri, /^bloxen-player:1\+ticket:[A-Za-z0-9_-]{20,64}\+version:version-0d46087630eb46cd$/);
  const row = app.db.prepare('SELECT * FROM launch_tickets').get(); assert.equal(row.user_id, uid); assert.ok(row.expires_at - row.created_at <= 60_000); assert.ok(!uri.includes(row.hash), 'only a hash is stored');
  // ---- launcher contract (stub client exe: proves argument construction, hash gate and redemption, NOT real-client behaviour)
  const cdir = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-client-')); const exe = path.join(cdir, 'RobloxPlayerBeta.exe'); fs.writeFileSync(exe, 'stub-not-a-real-client');
  const cfg = { clientDir: cdir, compatBase: app.compatUrl, logFile: path.join(cdir, 'launcher.log'), expectedSha256: crypto.createHash('sha256').update('stub-not-a-real-client').digest('hex') };
  let spawned; const res = await L.launch(uri, cfg, { spawnFn: (e, a, o) => { spawned = { e, a, o }; return { pid: 1234, unref() {} }; } });
  assert.equal(spawned.e, exe); assert.equal(spawned.o.shell, false); assert.deepEqual(spawned.a.filter((_, i) => i % 2 === 0), ['-a', '-t', '-j']); assert.ok(fs.readFileSync(path.join(cdir, 'AppSettings.xml'), 'utf8').includes(app.compatUrl));
  assert.ok(!fs.readFileSync(cfg.logFile, 'utf8').includes(uri.split('ticket:')[1].split('+')[0]), 'ticket never logged');
  await assert.rejects(() => L.launch(uri, cfg, { dryRun: true }), /ticket|rejected/, 'launch ticket is single use');
  // ---- compat backend
  const joinUrl = res.join.joinScriptUrl; const js = await (await fetch(joinUrl)).text(); assert.match(js, /PlayerConnect/); const port = +/PlayerConnect\(\d+, "127\.0\.0\.1", (\d+)/.exec(js)[1];
  assert.match(await (await fetch(`${app.compatUrl}/Asset/CharacterFetch.ashx?userId=${uid}`)).text(), /BodyColors\.ashx\?userId=/); const bc = await (await fetch(`${app.compatUrl}/Asset/BodyColors.ashx?userId=${uid}`)).text(); assert.match(bc, /<int name="TorsoColor">23<\/int>/);
  const miss = await fetch(`${app.compatUrl}/asset/?id=151784320`); assert.equal(miss.status, 404); assert.match(await miss.text(), /MISSING/); assert.ok(app.db.prepare('SELECT COUNT(*) n FROM missing_asset_log').get().n >= 1);
  // ---- game server: simulated client joins with the join token
  const c = new SimClient({ port, ticket: res.join.authenticationTicket }); await c.connect(); await c.waitFor(() => c.state === 'ingame', 15000);
  assert.equal(c.descriptors.names.length, 332); assert.equal(c.globals[0].classId, 231); assert.equal(c.character.props.Name, name);
  const bcs = [...c.instances.values()].find(i => i.className === 'BodyColors'); assert.equal(bcs.props.TorsoColor, 23, 'saved avatar reaches the spawned character');
  const srv = [...app.gameservers.servers.values()][0]; assert.deepEqual(srv.gs.world.players.get(uid).missingAssets, [151784320], 'hat content is MISSING and reported, not substituted');
  assert.ok([...c.instances.values()].some(i => i.className === 'Part'), 'historical place geometry replicated'); if (real) assert.equal(srv.gs.world.stats.imported, 10);
  const p0 = c.part('HumanoidRootPart').props.CFrame.pos; c.move({ pos: { x: p0.x + 2, y: p0.y, z: p0.z }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] }); await c.waitFor(() => Math.abs(c.part('HumanoidRootPart').props.CFrame.pos.x - p0.x - 2) < 1e-3);
  // join token is single-use
  const replay = new SimClient({ port, ticket: res.join.authenticationTicket }); await replay.connect(); await replay.waitFor(() => replay.state === 'refused'); assert.match(replay.refusal, /ticket/);
  assert.match((await b.get('/home')).text, /Tabula Rasa/, 'recently played'); c.close(); replay.close();
}
test('E2E (synthetic test place)', t => flow(t, { real: false }));
const haveReal = (() => { try { const m = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'preservation', 'manifests', 'places.json'))).items.find(p => p.id === 'tabularasa'); return fs.existsSync(path.join(REAL_STORE, m.sha256 + '.rbx')); } catch { return false; } })();
test('E2E (real archived place: TabulaRasa from preservation/store)', { skip: !haveReal && 'run: node tools/fetch_places.js tabularasa' }, t => flow(t, { real: true }));
