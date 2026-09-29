'use strict';
// Script host tests. UNIT/SIMULATOR level. Uses small hand-written .rbxlx places (scripts are the test subject, not historical content)
// plus, when present, the real stored Crossroads place (preservation/store is git-ignored; those tests skip if it was not fetched).
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const path = require('path');
const { parsePlace } = require('../src/importer/parse'); const { World } = require('../src/gameserver/world'); const { ScriptHost } = require('../src/script/engine');

let n = 0; const esc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');
function item(cls, name, props = '', kids = '') { return `<Item class="${cls}" referent="RBX${n++}"><Properties><string name="Name">${esc(name)}</string>${props}</Properties>${kids}</Item>`; }
const script = (name, src, extra = '') => item('Script', name, `<ProtectedString name="Source"><![CDATA[${src}]]></ProtectedString>${extra}`);
const part = (name, x = 0, y = 0, z = 0) => item('Part', name, `<Vector3 name="size"><X>4</X><Y>1</Y><Z>2</Z></Vector3><CoordinateFrame name="CFrame"><X>${x}</X><Y>${y}</Y><Z>${z}</Z><R00>1</R00><R01>0</R01><R02>0</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>0</R20><R21>0</R21><R22>1</R22></CoordinateFrame><bool name="Anchored">true</bool>`);
const intv = (name, v) => item('IntValue', name, `<int name="Value">${v}</int>`);
const mk = (kids, extra = '') => `<roblox version="4">${item('Workspace', 'Workspace', '', kids)}${extra}</roblox>`;
function boot(xml, opts = {}) { const world = new World({ place: parsePlace(Buffer.from(xml)), respawnSeconds: 0.01 }); const out = []; const host = new ScriptHost(world, { print: s => out.push(s), ...opts }).start(); return { world, host, out, events: (() => { const e = []; world.onChange(ev => e.push(ev)); return e; })() }; }
const run = (h, secs, dt = 0.25) => { for (let t = 0; t < secs; t += dt) h.step(dt); };

test('scripts are inert data in World: recorded, never executed without a ScriptHost', () => {
  const world = new World({ place: parsePlace(Buffer.from(mk(script('S', 'error("must not run")')))) });
  assert.equal(world.scriptSources.length, 1); assert.equal(world.scriptSources[0].source, 'error("must not run")'); assert.ok(!world.roots.some(r => r.className === 'Script'));
  assert.equal(world.service('Workspace').children.length, 0, 'scripts are not replicated');
});
test('print, script.Parent, FindFirstChild, property get/set with type conversion, replicated as prop events', () => {
  const { host, out, world, events } = boot(mk(item('Model', 'M', '', part('P', 1, 2, 3) + intv('Count', 5) + script('S', `
    local m = script.Parent print(m.Name, m.ClassName, m:FindFirstChild("P").Name, m.P.Position.x, m.Count.Value, m:findFirstChild("nope"))
    m.Count.Value = m.Count.Value + 1 m.P.Anchored = false m.P.Position = Vector3.new(9,8,7) m.Name = "Renamed" m.P.Size = Vector3.new(1,1,1)
    print(m.Count.Value, m.P.Anchored, m.P.CFrame.x, #m:GetChildren(), m.P:IsA("BasePart"), m.P:IsA("Model"), m.P:GetFullName())`))));
  run(host, 1); assert.deepEqual(host.errors, []);
  assert.deepEqual(out, ['M\tModel\tP\t1\t5\tnil', '6\tfalse\t9\t2\ttrue\tfalse\tWorkspace.Renamed.P']);
  assert.ok(events.some(e => e.op === 'prop' && e.name === 'CFrame')); assert.ok(events.some(e => e.op === 'prop' && e.name === 'Name'));
  const p = [...world.byId.values()].find(i => i.name === 'P'); assert.deepEqual(p.props.get('CFrame').pos, { x: 9, y: 8, z: 7 }); assert.equal(p.props.get('Anchored'), false);
});
test('bad property writes are Lua errors, contained per script', () => {
  const { host, out } = boot(mk(item('Model', 'M', '', part('P') + script('A', 'script.Parent.P.Anchored = true script.Parent.P.Size = 5') + script('B', 'wait(1) print("B alive")') + script('C', 'script.Parent.P.Bogus = 1') + script('D', 'print(script.Parent.P.Nope)'))));
  run(host, 3); assert.deepEqual(out, ['B alive']); assert.equal(host.errors.length, 3);
  assert.match(host.errors.find(e => /A$/.test(e.thread)).message, /invalid value for Size: Vector3 expected, got number/); assert.match(host.errors.find(e => /C$/.test(e.thread)).message, /Bogus is not a valid member of Part/);
  assert.equal(host.report().statusCounts.error, 3);
});
test('clone / remove / re-parent regenerate a model: wire events are del then new with FRESH ids, detached clones are never replicated', () => {
  const { host, world, events } = boot(mk(item('Model', 'Castle', '', part('Wall', 0, 5, 0) + part('Gate', 4, 5, 0)) + item('Model', 'Regen', '', intv('T', 2) + script('R', `
    local model = game.Workspace.Castle local backup = model:clone()
    while true do wait(script.Parent.T.Value) model:remove() wait(1) model = backup:clone() model.Parent = game.Workspace model:makeJoints() end`))));
  const ids0 = [...world.byId.values()].filter(i => i.name === 'Castle' || i.name === 'Wall').map(i => i.id); assert.equal(ids0.length, 2);
  run(host, 2.5); const dels = events.filter(e => e.op === 'del'); assert.equal(dels.length, 1); assert.equal(dels[0].id, ids0[0]);
  assert.equal([...world.byId.values()].filter(i => i.name === 'Castle').length, 0, 'removed from the live tree');
  assert.ok(!events.some(e => e.op === 'new' && e.inst.name === 'Castle'), 'clone not announced while detached (backup is never sent)');
  run(host, 1.5); const news = events.filter(e => e.op === 'new').map(e => e.inst); assert.deepEqual(news.map(i => i.name), ['Castle', 'Wall', 'Gate']);
  assert.ok(news.every(i => !ids0.includes(i.id)), 'fresh ids'); assert.ok(news.every(i => world.byId.get(i.id) === i));
  assert.equal(host.report().unsupported['method MakeJoints: no-op (joints are not simulated)'] >= 1, true);
  run(host, 10); assert.equal(host.errors.length, 0); assert.equal([...world.byId.values()].filter(i => i.name === 'Castle').length <= 1, true);
});
test('Instance.new: creatable classes work, NotCreatable / Script / unknown / blocked services are refused with clear errors', () => {
  const { host, out } = boot(mk(script('S', `
    local m = Instance.new("Message") m.Text = "hi" print(m.Text, m.Parent) m.Parent = workspace print(m.Parent.Name, #workspace:GetChildren())
    print(pcall(Instance.new, "Workspace")) print(pcall(Instance.new, "Script")) print(pcall(Instance.new, "NoSuchClass"))
    print(pcall(function() return game:GetService("HttpService") end)) print(pcall(function() return game.HttpService end)) print(pcall(function() return game:service("Lighting").Name end))
    print(pcall(function() return loadstring("x") end)) print(pcall(function() return require end))`)));
  run(host, 1); assert.deepEqual(host.errors, []);
  assert.equal(out[0], 'hi\tnil'); assert.equal(out[1], 'Workspace\t1'); // scripts themselves are not in the replicated tree
  assert.match(out[2], /^false\t.*not creatable/); assert.match(out[3], /^false\t.*not allowed/); assert.match(out[4], /^false\t.*Unable to create an Instance of type "NoSuchClass"/);
  assert.match(out[5], /^false\t.*HttpService is not available/); assert.match(out[6], /^false\t.*HttpService is not available/); assert.equal(out[7], 'true\tLighting');
  assert.match(out[8], /^false\t.*attempt to call global 'loadstring' \(a nil value\)/); assert.equal(out[9], 'true\tnil'); // loadstring / require simply do not exist
  assert.ok(host.report().unsupported['service HttpService blocked by the sandbox'] >= 2);
});
test('per-script environments: globals do not leak between scripts, _G/shared are shared', () => {
  const { host, out } = boot(mk(script('A', 'x = "A-global" _G.k = 1 shared.s = "from A" wait(1) print("A sees x", x, _G.k, shared.t)') + script('B', 'x = "B-global" wait(0.5) _G.k = 2 shared.t = "from B" print("B sees x", x, shared.s)')));
  run(host, 2); assert.deepEqual(out, ['B sees x\tB-global\tfrom A', 'A sees x\tA-global\t2\tfrom B']);
});
test('runaway and broken scripts are stopped without affecting the server or other scripts', () => {
  const { host, out } = boot(mk(script('Spin', 'while true do end') + script('Syn', 'local = =') + script('Ok', 'wait(1) print("ok")')), { budget: 30_000 });
  run(host, 2); assert.deepEqual(out, ['ok']); const st = Object.fromEntries(host.report().scripts.map(s => [s.path.split('.').pop(), s.status]));
  assert.deepEqual(st, { Spin: 'budget-exceeded', Syn: 'syntax-error', Ok: 'finished' });
});
test('disabled scripts and LocalScripts are not run', () => {
  const world = new World({ place: parsePlace(Buffer.from(mk(script('Off', 'print("no")', '<bool name="Disabled">true</bool>') + item('LocalScript', 'L', '<ProtectedString name="Source">print("client")</ProtectedString>') + script('On', 'print("yes")')))) });
  const out = []; const host = new ScriptHost(world, { print: s => out.push(s) }).start(); run(host, 1); assert.deepEqual(out, ['yes']);
  assert.deepEqual(host.report().scripts.map(s => s.status).sort(), ['finished', 'not-run:client-side script', 'not-run:disabled']);
});
test('Players: PlayerAdded/ChildAdded, leaderstats IntValues replicate under the Player, Humanoid.Died fires on kill, CharacterAdded on respawn', async () => {
  const { host, world, out, events } = boot(mk(script('Board', `
    game.Players.PlayerAdded:connect(function(p)
      local stats = Instance.new("IntValue") stats.Name = "leaderstats" local k = Instance.new("IntValue") k.Name = "Wipeouts" k.Parent = stats stats.Parent = p
      print("added", p.Name, p.userId, p.Character)
      local function onChar(ch)
        print("char", ch.Name, ch.Humanoid.Health)
        ch.Humanoid.Died:connect(function() k.Value = k.Value + 1 print("died", p.Name, k.Value) end) end
      p.CharacterAdded:connect(onChar) if p.Character then onChar(p.Character) end
    end)`)));
  run(host, 0.5); const rec = world.addPlayer({ userId: 7, username: 'Tester', avatar: {} }); run(host, 0.5);
  assert.equal(out[0], 'added\tTester\t7\tTester'); // handlers run as new threads on the next step, after the server already spawned the character
  assert.ok(out.some(l => l === 'char	Tester	100'), out.join('|'));
  const stats = rec.player.children.find(c => c.name === 'leaderstats'); assert.ok(stats && world.byId.get(stats.id) === stats, 'leaderstats is in the replicated tree under the Player');
  assert.ok(events.some(e => e.op === 'new' && e.inst.name === 'Wipeouts'));
  world.kill(rec); run(host, 0.5); assert.ok(out.includes('died\tTester\t1'), out.join('|'));
  await new Promise(r => setTimeout(r, 60)); run(host, 1); assert.equal(out.filter(l => l.startsWith('char\t')).length, 2, 'CharacterAdded fired again after respawn');
  world.kill(rec); run(host, 0.5); assert.ok(out.includes('died\tTester\t2'), out.join('|'));
});
test('Humanoid.Health = 0 from a script kills the real player (and the respawn machinery runs); TakeDamage works', () => {
  const { host, world } = boot(mk(script('Killer', 'wait(1) local h = game.Workspace:findFirstChild("Tester").Humanoid h:TakeDamage(30) wait(1) h.Health = 0')));
  const rec = world.addPlayer({ userId: 9, username: 'Tester', avatar: {} }); const hum = rec.character.children.find(c => c.className === 'Humanoid');
  run(host, 1.5); assert.deepEqual(host.errors, []); assert.equal(hum.props.get('Health'), 70, 'TakeDamage applied'); run(host, 1); assert.equal(rec.dead, true); assert.equal(rec.deaths, 1); assert.deepEqual(host.errors, []);
});
test('Lighting time of day advances like the preserved AndYetItMoves script uses it', () => {
  const { host, world } = boot(mk(script('AndYetItMoves', 'l = game:service("Lighting") while true do l:SetMinutesAfterMidnight(l:GetMinutesAfterMidnight()+1) wait(1) end')));
  const L = world.service('Lighting'); L.props.set('TimeOfDay', '12:00:00'); run(host, 3.1, 0.1); assert.equal(L.props.get('TimeOfDay'), '12:04:00'); assert.deepEqual(host.errors, []);
});
test('Touched: a script with no player nearby never fires, and the report states the exact (limited) contact model', () => {
  const { host, out } = boot(mk(item('Model', 'M', '', part('Pad', 500, 0, 500) + script('T', 'script.Parent.Pad.Touched:connect(function() print("touched") end)'))));
  run(host, 1); const r = host.report(); assert.equal(r.touchedConnections, 1); assert.deepEqual(out, []); assert.match(Object.keys(r.unsupported).join(), /Touched: fires only when a player's character overlaps the part/);
});
test('Touched/TouchEnded fire once per contact for a player character overlapping a script-listened part (box test); not for distant parts or non-players', () => {
  const pad = part('Pad', 0, 0, 0); const far = part('Far', 300, 0, 300);
  const { host, out, world } = boot(mk(item('Model', 'M', '', pad + far + script('T', `
    script.Parent.Pad.Touched:connect(function(hit) local h = hit.Parent:FindFirstChild("Humanoid") print("pad", hit.Name, h and "humanoid" or "none") end)
    script.Parent.Pad.TouchEnded:connect(function(hit) print("end", hit.Name) end)
    script.Parent.Far.Touched:connect(function() print("far") end)`))));
  const rec = world.addPlayer({ userId: 21, username: 'Walker', avatar: {} }); run(host, 0.5);
  const root = rec.parts.HumanoidRootPart; const cfAt = (x, y, z) => ({ pos: { x, y, z }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] });
  const at = (x, y, z) => world.applyMove(rec, cfAt(x, y, z), rec.lastMove + 1000);
  at(0, 100, 0); run(host, 0.5); assert.deepEqual(out, [], 'high above: no contact');
  at(0, 1, 0); run(host, 0.5); assert.ok(out.length >= 1 && out.every(l => /^pad\t[\w ]+\thumanoid$/.test(l)), out.join('|'));
  const firstCount = out.length; run(host, 1); assert.equal(out.length, firstCount, 'no re-fire while staying in contact');
  at(0, 100, 0); run(host, 0.5); assert.ok(out.some(l => /^end\t/.test(l)), 'TouchEnded fired on leaving'); assert.ok(!out.some(l => l === 'far'));
  assert.ok(root, 'used the real character root part');
});
test('obbOverlap respects rotation: a long thin rotated plank touches where its axis-aligned box would not', () => {
  const { obbOverlap } = require('../src/script/engine'); const I = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
  const box = (c, h, a = I) => ({ c, h, a }); const d = Math.SQRT1_2; const rotZ45 = [[d, d, 0], [-d, d, 0], [0, 0, 1]];
  const small = box([3, 3, 0], [0.5, 0.5, 0.5]); const plank = box([0, 0, 0], [5, 0.25, 1], rotZ45);   // 45-degree plank reaches toward (3,3)
  assert.equal(obbOverlap(plank, small), true); assert.equal(obbOverlap(box([0, 0, 0], [5, 0.25, 1]), small), false, 'unrotated plank misses it');
  assert.equal(obbOverlap(box([0, 0, 0], [1, 1, 1]), box([1.5, 0, 0], [1, 1, 1], I)), true); assert.equal(obbOverlap(box([0, 0, 0], [1, 1, 1]), box([3, 0, 0], [1, 1, 1], I)), false);
});
const STORE = path.join(__dirname, '..', 'preservation', 'store'); const MAN = require('../preservation/manifests/places.json');
const xr = MAN.items.find(p => p.id === 'crossroads-2007-client'); const XR_FILE = xr && path.join(STORE, xr.sha256 + '.rbx');
test('REAL PLACE (Crossroads 2007 file): the preserved Regenerate*/AndYetItMoves/Leaderboard scripts run; Touched/BodyMover scripts are reported, not faked', { skip: !XR_FILE || !fs.existsSync(XR_FILE) }, () => {
  const world = new World({ place: parsePlace(fs.readFileSync(XR_FILE)) }); const out = []; const host = new ScriptHost(world, { print: s => out.push(s) }).start();
  const dels = []; world.onChange(e => { if (e.op === 'del') dels.push(e.inst.name); }); run(host, 600, 1);
  const r = host.report(); assert.equal(r.scripts.length, 25);
  const byName = n => r.scripts.filter(s => s.path.includes(n)); for (const n of ['Regenerate Castle', 'Regenerate Tower', 'Regenerate Hideout', 'Regenerate Lost Temple', 'Regenerate Ramp and Trees']) assert.equal(byName(n)[0].status, 'running', n);
  assert.ok(dels.length >= 5, 'at least some regeneration cycles removed models: ' + dels.join()); assert.ok(out.includes('Leaderboard script version 3.00 loaded'));
  assert.equal(byName('TeamBeacon').filter(s => s.status === 'error').length, 0, 'TeamBeacon now loads (lowercase BodyPosition.position); its BodyMover motion is still not simulated'); assert.equal(byName('TeamBeacon').filter(s => s.status === 'running').length, 4);
  assert.ok(r.touchedConnections >= 10); assert.ok(world.service('Lighting').props.get('TimeOfDay') !== undefined);
});


test('player-input events (HopperBin.Selected, Tool.Equipped) connect without error but are reported as never firing', () => {
  const { host, out } = boot(mk(item('HopperBin', 'Jet', '', script('J', 'script.Parent.Selected:connect(function() print("sel") end) print("loaded")')) + item('Tool', 'T', '', script('K', 'script.Parent.Equipped:connect(function() end) script.Parent.Activated:connect(function() end)'))));
  run(host, 1); const r = host.report(); assert.deepEqual(out, ['loaded']); assert.deepEqual(host.errors, []);
  const keys = Object.keys(r.unsupported).join('|'); assert.match(keys, /event Selected: connected but never fires/); assert.match(keys, /event Equipped/); assert.match(keys, /event Activated/);
});
test("a script's own bug is reported as that script's error (Vector3.new given the function math.random)", () => {
  const { host } = boot(mk(script('Bad', 'local v = Vector3.new(math.random(), 1, math.random)')));
  run(host, 1); assert.equal(host.errors.length, 1); assert.match(host.errors[0].message, /bad argument #3 to 'new' \(number expected, got function\)/);
});

test('legacy members: Humanoid.Torso/LeftLeg resolve to the character parts; BasePart.Color maps through BrickColor; BodyPosition.position is a server-local Vector3', () => {
  const { host, out, world } = boot(mk(item('Part', 'Beacon', '', item('BodyPosition', 'BP'))
    + script('L', `
      local p = game.Workspace.Beacon
      p.BP.position = Vector3.new(1, 2, 3) print("bp", p.BP.position.y)
      p.Color = Color3.new(0.77, 0.16, 0.11) print("brick", p.BrickColor.Name)
      print("color", p.Color.r > 0.7)
      game.Players.ChildAdded:connect(function(pl) wait(0.2) local h = pl.Character.Humanoid print("torso", h.Torso.Name, h.LeftLeg.Name) h.Torso.Color = Color3.new(0, 0, 0) print("tb", h.Torso.BrickColor.Name) end)`)));
  run(host, 0.5); world.addPlayer({ userId: 31, username: 'Legacy', avatar: {} }); run(host, 1);
  assert.deepEqual(host.errors, []); assert.deepEqual(out, ['bp\t2', 'brick\tBright red', 'color\ttrue', 'torso\tTorso\tLeft Leg', 'tb\tReally black']);
});
test('a host-internal JS error inside a script thread stops that script and is reported, it does not crash the server loop', () => {
  const { host } = boot(mk(script('X', 'print("ok")')));
  host.scheduler.spawn(function* () { throw new TypeError('boom'); }, 'internal-test'); assert.doesNotThrow(() => run(host, 0.5));
});
