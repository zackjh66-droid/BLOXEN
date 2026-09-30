'use strict';
// Avatar chain: catalog -> purchase -> inventory -> equip -> saved avatar -> (compat + game-server handoff) -> spawned character. SIMULATOR-TESTED (no real client).
const test = require('node:test'); const assert = require('node:assert/strict');
const { start } = require('../src/web/main'); const { Browser } = require('./helpers'); const { SimClient } = require('../src/sim/client'); const S = require('../src/lib/services'); const fs = require('fs'); const os = require('os'); const path = require('path'); const crypto = require('crypto'); const L = require('../src/launcher/launcher');
let app; test.before(async () => { app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true }); }); test.after(() => app.close());
// Both archived Faces are Limited U (not purchasable in BLOXEN), so face ownership is granted directly in the DB to exercise equip -> server handoff only.
const grant = (uid, id) => app.db.prepare('INSERT OR IGNORE INTO inventory(user_id,asset_id,acquired_at) VALUES(?,?,?)').run(uid, id, Date.now());
const pick = t => app.db.prepare('SELECT asset_id,name FROM catalog_items WHERE type_id=? AND price_robux IS NOT NULL AND limited_unique=0 AND resale_only=0 AND min_membership=0 ORDER BY asset_id LIMIT 1').get(t);

test('every avatar slot flows from the website to the spawned character; unavailable content is reported MISSING, never substituted', async () => {
  const b = new Browser(app.webUrl); const name = 'av' + Math.random().toString(36).slice(2, 8); await b.register(name); const uid = app.db.prepare('SELECT id FROM users WHERE username=?').get(name).id;
  const face = app.db.prepare('SELECT asset_id,name FROM catalog_items WHERE type_id=18 ORDER BY asset_id LIMIT 1').get(), tee = pick(2), hat = pick(8), pkg = pick(32); grant(uid, face.asset_id);
  for (const i of [face, tee, hat, pkg]) { if (i !== face) await b.post(`/catalog/item/${i.asset_id}/buy`, {}, { page: `/catalog/item/${i.asset_id}` }); await b.post('/my/character/equip', { asset: i.asset_id }, { page: '/my/character' }); }
  for (const [part, col] of [['head_color', 24], ['torso_color', 23], ['left_arm_color', 21], ['right_arm_color', 28], ['left_leg_color', 119], ['right_leg_color', 37]]) await b.post('/my/character/color', { part, color: col }, { page: '/my/character' });
  const g = S.avatarForGame(app.db, uid); assert.deepEqual(g.bodyColors, { HeadColor: 24, TorsoColor: 23, LeftArmColor: 21, RightArmColor: 28, LeftLegColor: 119, RightLegColor: 37 });
  assert.equal(g.faceId, face.asset_id); assert.equal(g.tshirtId, tee.asset_id); assert.deepEqual(g.hatIds, [hat.asset_id]); assert.deepEqual(g.packageIds, [pkg.asset_id]);
  // compat services serve the same avatar the game server will use
  const bc = await (await fetch(`${app.compatUrl}/Asset/BodyColors.ashx?userId=${uid}`)).text(); for (const [k, v] of Object.entries(g.bodyColors)) assert.match(bc, new RegExp(`<int name="${k}">${v}</int>`));
  const cf = await (await fetch(`${app.compatUrl}/Asset/CharacterFetch.ashx?userId=${uid}`)).text(); assert.match(cf, /BodyColors\.ashx\?userId=/); assert.match(cf, new RegExp(`id=${tee.asset_id}`)); assert.ok(!/roblox\.com/.test(cf.replace(/http:\/\/www\.roblox\.com\/asset\/\?id=/g, '')), 'no other remote hosts');
  // a real join -> game server -> spawned character
  const play = await b.req('POST', '/games/tabularasa/play', { form: { _csrf: b.csrfFrom((await b.get('/games/tabularasa')).text) }, headers: { accept: 'application/json' } }); const { uri } = JSON.parse(play.text);
  const cdir = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-av-')); fs.writeFileSync(path.join(cdir, 'RobloxPlayerBeta.exe'), 'stub-not-a-real-client');
  const res = await L.launch(uri, { clientDir: cdir, compatBase: app.compatUrl, logFile: path.join(cdir, 'l.log'), expectedSha256: crypto.createHash('sha256').update('stub-not-a-real-client').digest('hex') }, { spawnFn: () => ({ pid: 1, unref() {} }) });
  const port = +/PlayerConnect\(\d+, "127\.0\.0\.1", (\d+)/.exec(await (await fetch(res.join.joinScriptUrl)).text())[1];
  const c = new SimClient({ port, ticket: res.join.authenticationTicket }); await c.connect(); await c.waitFor(() => c.state === 'ingame', 15000);
  const ins = [...c.instances.values()]; const bcs = ins.find(i => i.className === 'BodyColors'); for (const [k, v] of Object.entries(g.bodyColors)) assert.equal(bcs.props[k], v, k);
  const part = n => ins.find(i => i.className === 'Part' && i.props.Name === n); assert.equal(part('Head').props.BrickColor, 24); assert.equal(part('Torso').props.BrickColor, 23); assert.equal(part('Left Arm').props.BrickColor, 21); assert.equal(part('Right Leg').props.BrickColor, 37);
  const decal = ins.find(i => i.className === 'Decal' && i.props.Name === 'face'); assert.ok(decal, 'face decal'); assert.match(decal.props.Texture, new RegExp(`/asset/\\?id=${face.asset_id}$`));
  const sg = ins.find(i => i.className === 'ShirtGraphic'); assert.ok(sg && new RegExp(`id=${tee.asset_id}$`).test(sg.props.Graphic), 'T-shirt graphic');
  assert.ok(!ins.some(i => i.className === 'Hat' || i.className === 'Accessory'), 'no substitute hat'); const rec = [...app.gameservers.servers.values()][0].gs.world.players.get(uid);
  assert.deepEqual([...rec.missingAssets].sort(), [hat.asset_id, pkg.asset_id].sort(), 'hat + package content is MISSING and listed'); c.close();
  // un-equipping changes what the next join receives
  await b.post('/my/character/remove', { asset: tee.asset_id }, { page: '/my/character' }); assert.equal(S.avatarForGame(app.db, uid).tshirtId, 0);
});
test('avatar rules: only owned wearable items equip; one slot per type; nobody can change another user\'s avatar', async () => {
  const a = new Browser(app.webUrl), o = new Browser(app.webUrl); await a.register('own' + Math.random().toString(36).slice(2, 7)); const on = 'oth' + Math.random().toString(36).slice(2, 7); await o.register(on); const oid = app.db.prepare('SELECT id FROM users WHERE username=?').get(on).id;
  const f1 = app.db.prepare('SELECT asset_id FROM catalog_items WHERE type_id=18 LIMIT 2').all(); const aid = app.db.prepare('SELECT id FROM users ORDER BY id DESC LIMIT 1 OFFSET 1').get().id; const r = await a.post('/my/character/equip', { asset: f1[0].asset_id }, { page: '/my/character' }); assert.match((await a.follow(r)).text, /own/i);
  const hat = pick(8); await a.post(`/catalog/item/${hat.asset_id}/buy`, {}, { page: `/catalog/item/${hat.asset_id}` }); assert.equal((await a.post('/my/character/color', { part: 'head_color', color: 99999 }, { page: '/my/character' })).status, 303);
  assert.equal(S.avatarGet(app.db, oid).items.length, 0); assert.equal(S.avatarGet(app.db, oid).colors.head_color, 194, 'other user untouched');
  for (const i of f1) { grant(aid, i.asset_id); await a.post('/my/character/equip', { asset: i.asset_id }, { page: '/my/character' }); }
  assert.equal(S.avatarGet(app.db, aid).items.filter(i => i.type === 'Face').length, 1, 'one face at a time');
});
