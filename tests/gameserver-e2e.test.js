'use strict';
// SIMULATOR-TESTED: the BLOXEN simulator client against the BLOXEN game server. Says nothing about the real Roblox client.
const test = require('node:test'); const assert = require('node:assert/strict');
const { GameServer } = require('../src/gameserver/server'); const { SimClient } = require('../src/sim/client'); const { syntheticPlace } = require('./helpers');
const avatar = { bodyColors: { HeadColor: 24, TorsoColor: 23 }, shirtId: 111, pantsId: 222, faceId: 333, hatIds: [444] };
async function boot(extra = {}) { const srv = await new GameServer({ place: syntheticPlace(), port: 0, respawnSeconds: 0.1, verifyTicket: t => (t === 'good' ? { userId: 7, username: 'Tester', avatar } : t === 'good2' ? { userId: 8, username: 'Other', avatar: {} } : null), ...extra }).listen(); return srv; }
const join = async (srv, t = 'good') => { const c = new SimClient({ port: srv.port, ticket: t }); await c.connect(); await c.waitFor(() => c.state === 'ingame', 10000); return c; };

test('join flow: descriptors -> SET_GLOBALS -> ID_DATA -> character', async () => {
  const srv = await boot(); const c = await join(srv);
  assert.equal(c.descriptors.names.length, 332); assert.equal(c.globals[0].className, 'ReplicatedFirst'); assert.equal(c.globals[0].classId, 231);
  assert.ok([...c.instances.values()].some(i => i.className === 'Part' && i.props.Name === 'Baseplate'), 'place Part replicated');
  assert.ok(![...c.instances.values()].some(i => i.className === 'Script'), 'server Scripts are not replicated and never executed');
  assert.equal(c.character.props.Name, 'Tester'); const root = c.part('HumanoidRootPart'); assert.deepEqual([root.props.CFrame.pos.x, root.props.CFrame.pos.z], [10, 20], 'spawned at SpawnLocation');
  assert.equal(c.part('Head').props.BrickColor, 24); const colors = [...c.instances.values()].find(i => i.className === 'BodyColors'); assert.equal(colors.props.HeadColor, 24);
  assert.equal([...c.instances.values()].find(i => i.className === 'Shirt').props.ShirtTemplate, 'http://www.roblox.com/asset/?id=111');
  assert.deepEqual(srv.world.players.get(7).missingAssets, [444], 'hat asset unavailable -> reported MISSING, not substituted');
  c.close(); srv.close();
});
test('movement: validated; teleports are rejected and corrected', async () => {
  const srv = await boot(); const c = await join(srv); const p0 = { ...c.part('HumanoidRootPart').props.CFrame.pos }; const R = [1, 0, 0, 0, 1, 0, 0, 0, 1];
  c.move({ pos: { x: p0.x + 3, y: p0.y, z: p0.z }, rot: R }); await c.waitFor(() => Math.abs(c.part('HumanoidRootPart').props.CFrame.pos.x - p0.x - 3) < 1e-3);
  c.move({ pos: { x: p0.x + 900, y: p0.y, z: p0.z }, rot: R }); await c.waitFor(() => srv.stats.movesRejected >= 1); await new Promise(r => setTimeout(r, 200));
  assert.ok(Math.abs(c.part('HumanoidRootPart').props.CFrame.pos.x - p0.x - 3) < 1e-3, 'server kept the authoritative position'); c.close(); srv.close();
});
test('respawn: reset creates a fresh character, old one deleted', async () => {
  const srv = await boot(); const c = await join(srv); const old = c.character.id; c.reset(); await c.waitFor(() => c.character && c.character.id !== old, 5000); assert.ok(!c.instances.has(old)); c.close(); srv.close();
});
test('fall below FallenPartsDestroyHeight kills and respawns', async () => {
  const srv = await boot(); const c = await join(srv); const old = c.character.id; const p0 = c.part('HumanoidRootPart').props.CFrame.pos;
  srv.world.applyMove(srv.world.players.get(7), { pos: { x: p0.x, y: -600, z: p0.z }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] }); await c.waitFor(() => c.character && c.character.id !== old, 5000); c.close(); srv.close();
});
test('two players see each other', async () => {
  const srv = await boot(); const a = await join(srv); const b = await join(srv, 'good2');
  await a.waitFor(() => [...a.instances.values()].some(i => i.className === 'Model' && i.props.Name === 'Other')); assert.ok([...b.instances.values()].some(i => i.className === 'Model' && i.props.Name === 'Tester'));
  const pb = b.part('HumanoidRootPart').props.CFrame.pos; b.move({ pos: { x: pb.x + 2, y: pb.y, z: pb.z }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] });
  await a.waitFor(() => { const o = [...a.instances.values()].find(i => i.props.Name === 'HumanoidRootPart' && i.parent !== a.character.id); return o && Math.abs(o.props.CFrame.pos.x - pb.x - 2) < 1e-3; });
  b.close(); await a.waitFor(() => ![...a.instances.values()].some(i => i.props.Name === 'Other'), 20000); a.close(); srv.close();
});
test('ticket and protocol gate', async () => {
  const srv = await boot(); const bad = new SimClient({ port: srv.port, ticket: 'nope' }); await bad.connect(); await bad.waitFor(() => bad.state === 'refused'); assert.match(bad.refusal, /ticket/);
  const old = new SimClient({ port: srv.port, ticket: 'good', protocolVersion: 30 }); await old.connect(); await old.waitFor(() => old.state === 'refused'); assert.ok(old.mismatch);
  assert.equal(srv.world.players.size, 0); srv.close();
});
test('refuses to bind to a non-loopback address by default', () => { assert.throws(() => new GameServer({ place: syntheticPlace(), host: '0.0.0.0', verifyTicket: () => null }), /non-loopback/); });
test('malformed packets do not crash the server', async () => {
  const srv = await boot(); const { RakClient } = require('../src/gameserver/raknet'); const rc = new RakClient({ port: srv.port }); await rc.connect();
  for (let i = 0; i < 8; i++) rc.send(Buffer.from([0x80, 0xff, 0xff, 0xff])); await new Promise(r => setTimeout(r, 400)); assert.ok(srv.stats.badPackets >= 1);
  const c = await join(srv); assert.equal(c.state, 'ingame'); c.close(); rc.close(); srv.close();
});
