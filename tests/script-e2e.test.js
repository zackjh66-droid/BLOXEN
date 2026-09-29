'use strict';
// SIMULATOR-TESTED: preserved-style server scripts (opt-in `scripts: true`) changing the world, seen by the BLOXEN simulator client over the wire.
// Says nothing about the real Roblox client.
const test = require('node:test'); const assert = require('node:assert/strict');
const { GameServer } = require('../src/gameserver/server'); const { SimClient } = require('../src/sim/client'); const { parsePlace } = require('../src/importer/parse');
const xml = src => `<roblox version="4"><Item class="Workspace" referent="RBX0"><Properties><string name="Name">Workspace</string></Properties><Item class="Model" referent="RBX1"><Properties><string name="Name">Holder</string></Properties><Item class="Script" referent="RBX2"><Properties><string name="Name">S</string><ProtectedString name="Source"><![CDATA[${src}]]></ProtectedString></Properties></Item></Item></Item></roblox>`;
const SRC = `
  local m = Instance.new("Message") m.Text = "Regenerating..." m.Parent = workspace
  game.Players.PlayerAdded:connect(function(p) local s = Instance.new("IntValue") s.Name = "leaderstats" local k = Instance.new("IntValue") k.Name = "KOs" k.Value = 3 k.Parent = s s.Parent = p end)
  wait(1.5) m.Parent = nil wait(1.5) local b = Instance.new("Part") b.Name = "ScriptMade" b.Parent = workspace`;
async function boot(scripts) { return new GameServer({ place: parsePlace(Buffer.from(xml(SRC))), port: 0, scripts, verifyTicket: t => (t === 'good' ? { userId: 7, username: 'Tester', avatar: {} } : null) }).listen(); }
const join = async srv => { const c = new SimClient({ port: srv.port, ticket: 'good' }); await c.connect(); await c.waitFor(() => c.state === 'ingame', 10000); return c; };
const has = (c, cls, name) => [...c.instances.values()].some(i => i.className === cls && (name === undefined || i.props.Name === name));

test('scripts: false (default) -> the place loads, scripts stay inert, nothing is created', async () => {
  const srv = await boot(false); const c = await join(srv); await new Promise(r => setTimeout(r, 3500));
  assert.equal(srv.scriptHost, null); assert.ok(!has(c, 'Message')); assert.ok(!has(c, 'Part', 'ScriptMade')); c.close(); srv.close();
});
test('scripts: true -> script-created instances, reparenting, PlayerAdded leaderstats are replicated to the client', async () => {
  const srv = await boot(true); const c = await join(srv);
  await c.waitFor(() => has(c, 'Message'), 5000); assert.equal([...c.instances.values()].find(i => i.className === 'Message').props.Text, 'Regenerating...');
  await c.waitFor(() => has(c, 'IntValue', 'leaderstats') && has(c, 'IntValue', 'KOs'), 5000);
  const ko = [...c.instances.values()].find(i => i.props.Name === 'KOs'); assert.equal(ko.props.Value, 3); const stats = [...c.instances.values()].find(i => i.props.Name === 'leaderstats'); assert.equal(ko.parent, stats.id); assert.equal(stats.parent, c.instances.get(stats.parent).id);
  await c.waitFor(() => !has(c, 'Message'), 5000); await c.waitFor(() => has(c, 'Part', 'ScriptMade'), 5000);
  const r = srv.scriptHost.report(); assert.equal(r.statusCounts.error ?? 0, 0); assert.deepEqual(srv.scriptHost.errors, []);
  c.close(); srv.close();
});
