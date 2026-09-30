'use strict';
// BLOXEN-safe TeleportService. UNIT/SIMULATOR level. The mapping rules live in preservation/manifests/place-mapping.json.
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const path = require('path');
const { parsePlace } = require('../src/importer/parse'); const { World } = require('../src/gameserver/world'); const { ScriptHost } = require('../src/script/engine');
const placemap = require('../src/lib/placemap'); const registry = require('../src/lib/registry');

let n = 0; const esc = s => String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;');
const item = (cls, name, props = '', kids = '') => `<Item class="${cls}" referent="RBX${n++}"><Properties><string name="Name">${esc(name)}</string>${props}</Properties>${kids}</Item>`;
const script = (name, src) => item('Script', name, `<ProtectedString name="Source"><![CDATA[${src}]]></ProtectedString>`);
const xml = src => `<roblox version="4">${item('Workspace', 'Workspace', '', script('S', src))}${item('Players', 'Players')}</roblox>`;
function boot(src, teleport) { const world = new World({ place: parsePlace(Buffer.from(xml(src))), respawnSeconds: 0.01 }); const out = []; const host = new ScriptHost(world, { print: s => out.push(s), teleport }).start(); host.step(0); return { world, host, out }; }
const tp = `local pl = game:GetService("Players") pl.PlayerAdded:connect(function(p) wait(0.1) game:GetService("TeleportService"):Teleport(%ID%, p) print("after", p.Name) end)`;

test('mapping: approved entries resolve to a local PRESERVED place pinned by SHA-256; unmapped / invalid / unavailable are controlled refusals', () => {
  const ok = placemap.resolve(1818); assert.equal(ok.ok, true); assert.equal(ok.result, 'local-destination'); assert.equal(registry.byId(ok.local).status, 'PRESERVED');
  for (const bad of [999999, 'abc', -4, 0, 1.5, null, '1818; drop']) { const r = placemap.resolve(bad); assert.equal(r.ok, false, String(bad)); assert.equal(r.result, 'unsupported-local-destination'); assert.equal(r.local, undefined); }
  const un = placemap.resolve(1818, { storeHas: () => false }); assert.equal(un.ok, false); assert.match(un.reason, /UNAVAILABLE/);
  const pin = JSON.parse(JSON.stringify(placemap.load())); pin.entries[0].sha256 = '0'.repeat(64); assert.match(placemap.resolve(1818, { mapping: pin }).reason, /pinned SHA-256/);
  const off = JSON.parse(JSON.stringify(placemap.load())); off.entries[0].approved = false; assert.match(placemap.resolve(1818, { mapping: off }).reason, /not approved/);
  const fake = JSON.parse(JSON.stringify(placemap.load())); fake.entries.push({ robloxPlaceId: 4242, local: 'unknown:aaaaaaaaaaaa', approved: true, sha256: null }); assert.match(placemap.resolve(4242, { mapping: fake }).reason, /not registry-PRESERVED/);
});
test('every approved mapping points at a registry-PRESERVED place whose historical PlaceId matches', () => {
  for (const e of placemap.load().entries) { const r = registry.byId(e.local); assert.ok(r, e.local); assert.equal(r.status, 'PRESERVED'); assert.equal(r.robloxPlaceId, e.robloxPlaceId); assert.equal(r.sha256, e.sha256); }
});
test('script Teleport to an unmapped PlaceId: controlled unsupported-local-destination, logged, script keeps running, no exception', () => {
  const log = []; const { world, host, out } = boot(tp.replace('%ID%', '555'), (pid, uid) => { const r = placemap.resolve(pid, { storeHas: () => true }); log.push([pid, uid, r.result]); return r; });
  world.addPlayer({ userId: 7, username: 'Tester', avatar: {} }); for (let t = 0; t < 1; t += 0.1) host.step(0.1);
  assert.deepEqual(host.errors, []); assert.ok(out.includes('after\tTester'), out.join('|')); assert.equal(host.teleportLog.length, 1);
  assert.equal(host.teleportLog[0].result, 'unsupported-local-destination'); assert.equal(host.teleportLog[0].ok, false); assert.equal(host.teleportLog[0].userId, 7); assert.deepEqual(log, [[555, 7, 'unsupported-local-destination']]);
  assert.ok([...host.report().unsupported ? Object.keys(host.report().unsupported) : []].some(k => /TeleportService/.test(k)));
});
test('script Teleport to a mapped PlaceId: resolver result is logged as local-destination (no network, no address)', () => {
  const { world, host } = boot(tp.replace('%ID%', '1501'), pid => placemap.resolve(pid, { storeHas: () => true }));
  world.addPlayer({ userId: 9, username: 'Trav', avatar: {} }); for (let t = 0; t < 1; t += 0.1) host.step(0.1);
  const e = host.teleportLog[0]; assert.equal(e.ok, true); assert.equal(e.local, 'roblox-world-headquarters'); assert.equal(e.robloxPlaceId, 1501); assert.ok(!('address' in e) && !('url' in e));
});
test('without a resolver every Teleport is refused; wrong argument types and unsupported members are Lua errors, not crashes', () => {
  const { world, host, out } = boot(`local T = game:GetService("TeleportService") print(pcall(function() T:Teleport(1818, 5) end)) print(pcall(function() return T:GetLocalPlayerTeleportData() end)) print(pcall(function() T:TeleportToPlaceInstance(1818, "x", nil) end)) print(game.TeleportService == nil)`, null);
  for (let t = 0; t < 1; t += 0.25) host.step(0.25); assert.deepEqual(host.errors, []); assert.match(out[0], /^false/); assert.match(out[1], /^false/); assert.match(out[2], /^true/); assert.equal(out[3], 'false');
  assert.equal(host.teleportLog.length, 1); assert.equal(host.teleportLog[0].ok, false);
});
const CR = path.join(__dirname, '..', 'preservation', 'store', 'acec28f504e6dc130a2409b95a01d72882d3503f0abc91d81368a0ac839e0edf.rbx');
test('Crossroads teleporter chain works through script shadow children (REAL place; skip if not fetched): no lookup errors, the server clones TeleportScript + PlaceId into the character, PlaceId resolves via the local mapping', { skip: !fs.existsSync(CR) }, () => {
  const world = new World({ place: parsePlace(fs.readFileSync(CR)), respawnSeconds: 0.01 }); const host = new ScriptHost(world, { teleport: pid => placemap.resolve(pid, { storeHas: () => true }) }).start(); host.step(1);
  const rec = world.addPlayer({ userId: 1, username: 'P', avatar: {} }); host.step(1);
  const beams = [...host.signals].filter(([w, m]) => /^teleBeam/.test(w.name) && m.get('Touched') && m.get('Touched').conns.some(c => c.connected)).map(([w]) => w); assert.ok(beams.length >= 3, 'beams with Touched listeners: ' + beams.length);
  const b = host._touchBox(beams[0]); world._moveCharacter(rec, { pos: { x: b.c[0], y: b.c[1] + b.h[1] + 2.5, z: b.c[2] }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] }); for (let i = 0; i < 12; i++) host.step(0.5);
  assert.deepEqual([...new Set(host.errors.map(e => e.message))], []);
  const tp = rec.character && rec.character.children.find(c => c.name === 'TeleportScript'); assert.ok(tp, 'TeleportScript cloned into the character'); assert.equal(tp.className, 'LocalScript');
  const pid = tp.children.find(c => c.name === 'PlaceId'); assert.ok(pid, 'PlaceId cloned under TeleportScript'); assert.equal(typeof pid.props.get('Value'), 'number');
  const r = placemap.resolve(pid.props.get('Value'), { storeHas: () => true }); assert.ok(['local-destination', 'unsupported-local-destination'].includes(r.result)); assert.equal(host.teleportLog.length, 0, 'LocalScripts never run on the BLOXEN server: no server-side teleport is issued');
});
test('shadow children are never replicated: parent.children is untouched and ids are not in byId', () => {
  const src = `<roblox version="4">${item('Workspace', 'Workspace', '', item('Model', 'M', '', script('Top', 'x=1') ))}</roblox>`; const world = new World({ place: parsePlace(Buffer.from(src)) });
  const m = world.service('Workspace').children.find(c => c.name === 'M'); assert.equal(m.children.length, 0); assert.equal(m.shadowKids.length, 1); assert.equal(world.inWorld(m.shadowKids[0]), false); assert.equal(world.byId.has(m.shadowKids[0].id), false);
});
