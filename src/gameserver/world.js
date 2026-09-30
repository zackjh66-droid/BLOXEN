'use strict';
// Server-side world: instance graph built from a parsed place (scripts are INERT DATA, never executed), player/character creation,
// movement validation and respawn. STATUS: UNIT-TESTED + SIMULATOR-TESTED. Not REAL-CLIENT-TESTED.
const schemaMod = require('./schema'); const wire = require('./wire');
const SERVER_ONLY = new Set(['Script', 'ServerScriptService', 'ServerStorage', 'ScriptService', 'RuntimeScriptService']);
const SERVICE_TOP_ORDER = ['ReplicatedFirst']; // INFERRED (observed first in real client SET_GLOBALS)
const SCRIPT_CLASSES = new Set(['Script', 'LocalScript', 'ModuleScript', 'CoreScript']);

function classStatus(className, schema = schemaMod.load()) {
  if (!schema.classes.has(className)) return 'unknown-class';
  if (SERVER_ONLY.has(className)) return 'server-only';
  return 'replicable';
}
const classReplicable = c => classStatus(c) === 'replicable';

class WInst {
  constructor(id, cls) { this.id = id; this.className = cls; this.props = new Map(); this.parent = null; this.children = []; this.referent = null; }
  get name() { const n = this.props.get('Name'); return n === undefined ? this.className : String(n); }
  setParent(p) { if (this.parent) this.parent.children.splice(this.parent.children.indexOf(this), 1); this.parent = p; if (p) p.children.push(this); }
  *descendants() { for (const c of this.children) { yield c; yield* c.descendants(); } }
}

class World {
  constructor({ schema = schemaMod.load(), place = null, placeInfo = {}, respawnSeconds = 5, fallenPartsDestroyHeight = -500, loadAsset = null } = {}) {
    this.schema = schema; this.nextId = 1; this.byId = new Map(); this.roots = []; this.listeners = new Set(); this.players = new Map();
    this.respawnSeconds = respawnSeconds; this.fallenY = fallenPartsDestroyHeight; this.loadAsset = loadAsset; this.placeInfo = placeInfo;
    this.scriptSources = []; // inert: {className,name,source,disabled,parent(WInst|null),path}. Never executed here; src/script/engine.js decides whether to run them.
    this.stats = { imported: 0, serverOnlyDropped: 0, unknownClassDropped: 0, propsDropped: 0, propsReplicable: 0, scriptsInert: 0, droppedClasses: {}, droppedProps: {} };
    if (place) this._importPlace(place);
    this._ensureServices();
  }
  create(cls, parent = null) { const i = new WInst(this.nextId++, cls); this.byId.set(i.id, i); if (parent) i.setParent(parent); else this.roots.push(i); return i; }
  service(cls) { return this.roots.find(r => r.className === cls) || this.create(cls); }
  _ensureServices() { for (const s of ['ReplicatedFirst', 'Workspace', 'Players', 'Lighting', 'ReplicatedStorage', 'StarterGui', 'StarterPack', 'SoundService']) this.service(s); }
  _importPlace(place) {
    const map = new Map(); const pending = [];
    const visit = (src, parent) => {
      const st = classStatus(src.className, this.schema);
      if (SCRIPT_CLASSES.has(src.className)) { this.stats.scriptsInert++; const so = src.props.get('Source'); const rec = { className: src.className, name: String(src.props.get('Name')?.value ?? src.className), source: typeof so?.value === 'string' ? so.value : '', disabled: src.props.get('Disabled')?.value === true, parent, referent: src.referent, shadow: null };
        if (parent) { rec.shadow = this._shadowTree(src); rec.shadow.parent = parent; (parent.shadowKids || (parent.shadowKids = [])).push(rec.shadow); } this.scriptSources.push(rec); }
      if (st !== 'replicable') { if (st === 'server-only') this.stats.serverOnlyDropped++; else { this.stats.unknownClassDropped++; } this.stats.droppedClasses[src.className] = (this.stats.droppedClasses[src.className] || 0) + 1; return; }
      const w = this.create(src.className, parent); w.referent = src.referent; map.set(src.referent, w); this.stats.imported++;
      const cls = this.schema.classes.get(src.className);
      for (const [k, pv] of src.props) {
        const pd = cls.byName.get(k); if (!pd || !wire.SUPPORTED.has(pd.kind)) { this.stats.propsDropped++; const key = src.className + '.' + k; this.stats.droppedProps[key] = (this.stats.droppedProps[key] || 0) + 1; continue; }
        const v = wire.coerce(pd.kind, pv); if (v === undefined) { this.stats.propsDropped++; const key = src.className + '.' + k + '(type)'; this.stats.droppedProps[key] = (this.stats.droppedProps[key] || 0) + 1; continue; }
        if (pd.kind === 'Object') pending.push([w, k, v]); else { w.props.set(k, v); this.stats.propsReplicable++; }
      }
      if (src.className !== 'Players') for (const c of src.children) visit(c, w);
    };
    for (const r of place.roots) { const st = classStatus(r.className, this.schema); if (st !== 'replicable') { visit(r, null); continue; } const existing = this.roots.find(x => x.className === r.className && this.schema.classes.get(r.className).tags.includes('Service')); visit(r, null); if (existing) { /* duplicate service root: merged not needed */ } }
    for (const [w, k, ref] of pending) { const t = map.get(ref); if (t) w.props.set(k, t.id); }
  }
  // Scripts and everything under them are NOT replicated (non-replicable classes), but other scripts look them up by name
  // (script.Parent.TouchScript.TeleportScript.PlaceId.Value). A shadow tree keeps that subtree as detached, never-replicated, never-in-byId
  // WInsts reachable through parent.shadowKids; `parent.children` (what clients see) is untouched. Only plain values are kept.
  _shadowTree(src, parent = null) {
    const w = new WInst(this.nextId++, src.className); w.referent = src.referent; w.shadow = true; if (SCRIPT_CLASSES.has(src.className)) w.sourceText = String(src.props.get('Source')?.value ?? '');
    for (const [k, pv] of src.props) { if (k === 'Source' || k === 'LinkedSource') continue; const t = pv.type; let v;
      if (t === 'string' || t === 'ProtectedString' || t === 'Content') v = String(pv.value); else if (t === 'bool') v = !!pv.value; else if (['int', 'int64', 'float', 'double'].includes(t)) v = Number(pv.value); else if (t === 'token') v = Number(pv.value); else continue;
      if (Number.isNaN(v)) continue; w.props.set(k, v); }
    if (parent) { w.parent = parent; } for (const c of src.children) { const k = this._shadowTree(c, w); w.children.push(k); } return w;
  }
  // ---------- change feed
  onChange(fn) { this.listeners.add(fn); return () => this.listeners.delete(fn); }
  emit(ev) { for (const l of this.listeners) l(ev); }
  add(inst) { this.emit({ op: 'new', inst }); }
  remove(inst) { if (!this.byId.has(inst.id) || this.byId.get(inst.id) !== inst) { inst.setParent(null); const i0 = this.roots.indexOf(inst); if (i0 >= 0) this.roots.splice(i0, 1); return; } const parentBefore = inst.parent; for (const d of [...inst.descendants()].reverse()) this.byId.delete(d.id); this.byId.delete(inst.id); this.emit({ op: 'del', id: inst.id, inst, parent: parentBefore }); inst.setParent(null); const i = this.roots.indexOf(inst); if (i >= 0) this.roots.splice(i, 1); }
  // ---- tree operations used by the script host. Detached instances are NOT in byId and are never sent to clients.
  inWorld(inst) { return this.byId.get(inst.id) === inst; }
  createDetached(cls) { return new WInst(this.nextId++, cls); }
  // Deep copy (detached, fresh ids). Object-reference props that point inside the copied tree are remapped; others are cleared to nil (0).
  cloneTree(inst) {
    const map = new Map(); const copy = src => { const w = new WInst(this.nextId++, src.className); map.set(src.id, w); for (const [k, v] of src.props) w.props.set(k, v && typeof v === 'object' ? structuredClone(v) : v); for (const c of src.children) { const cc = copy(c); cc.parent = w; w.children.push(cc); } return w; };
    const root = copy(inst); root.parent = null;
    const cls = n => this.schema.classes.get(n);
    for (const w of [root, ...root.descendants()]) for (const [k, v] of w.props) if (cls(w.className)?.byName.get(k)?.kind === 'Object') w.props.set(k, map.get(v)?.id ?? 0);
    return root;
  }
  // Re-parent. Attaching into the live tree assigns fresh ids (ids of removed instances are never reused) and announces every replicable instance.
  attach(inst, parent) {
    const wasIn = this.inWorld(inst);
    if (wasIn) this.remove(inst); else if (inst.parent) inst.setParent(null);
    if (!parent) return;
    inst.setParent(parent);
    if (!this.inWorld(parent)) return;
    // Non-replicable classes (Script/LocalScript and their contents, server-only classes) stay in the tree so scripts can find them by name, but are never given wire ids or announced.
    const all = [...this.replSubtree(inst)]; const remap = new Map(); for (const w of all) { remap.set(w.id, this.nextId); w.id = this.nextId++; }
    for (const w of all) { for (const [k, v] of w.props) if (this.schema.classes.get(w.className)?.byName.get(k)?.kind === 'Object' && remap.has(v)) w.props.set(k, remap.get(v)); this.byId.set(w.id, w); }
    for (const w of all) this.add(w);
  }
  setProp(inst, name, value) { inst.props.set(name, value); this.emit({ op: 'prop', inst, name }); }
  topContainers() { const svc = this.roots.filter(r => this.schema.classes.get(r.className)?.tags.includes('Service') && classReplicable(r.className)); const first = SERVICE_TOP_ORDER.map(n => svc.find(s => s.className === n)).filter(Boolean); return [...first, ...svc.filter(s => !first.includes(s))]; }
  *replSubtree(inst) { if (classStatus(inst.className, this.schema) !== 'replicable') return; yield inst; for (const c of inst.children) yield* this.replSubtree(c); }
  *subtree(inst) { yield inst; for (const c of inst.children) yield* this.subtree(c); }
  // ---------- spawn
  spawnPoint() {
    const ws = this.service('Workspace'); for (const i of ws.descendants()) if (i.className === 'SpawnLocation' && i.props.get('Enabled') !== false) {
      const cf = i.props.get('CFrame'); const sz = i.props.get('size') || i.props.get('Size') || { x: 12, y: 1, z: 12 }; if (cf) return { x: cf.pos.x, y: cf.pos.y + sz.y / 2 + 3, z: cf.pos.z, from: 'SpawnLocation' }; }
    return { x: 0, y: 53, z: 0, from: 'default' };
  }
  addPlayer({ userId, username, avatar = {} }) {
    const players = this.service('Players'); const p = this.create('Player', players); p.props.set('Name', username); p.props.set('userId', userId); p.props.set('CharacterAppearance', avatar.appearanceUrl || '');
    const rec = { player: p, userId, username, avatar, character: null, lastMove: Date.now(), deaths: 0, missingAssets: [] }; this.players.set(userId, rec); this.add(p); this.loadCharacter(rec); return rec;
  }
  removePlayer(userId) { const rec = this.players.get(userId); if (!rec) return; clearTimeout(rec.respawnTimer); if (rec.character) this.remove(rec.character); this.remove(rec.player); this.players.delete(userId); }
  loadCharacter(rec) {
    const sp = this.spawnPoint(); const ws = this.service('Workspace'); const { model, parts, missing } = buildCharacter(this, ws, rec, sp); rec.character = model; rec.parts = parts; rec.spawn = sp; rec.missingAssets = missing; rec.dead = false;
    rec.player.props.set('Character', model.id); for (const i of this.subtree(model)) this.add(i); this.emit({ op: 'prop', inst: rec.player, name: 'Character' }); return rec;
  }
  kill(rec) { if (rec.dead) return; rec.dead = true; rec.deaths++; const hum = rec.character.children.find(c => c.className === 'Humanoid'); if (hum) this.setProp(hum, 'Health', 0);
    rec.respawnTimer = setTimeout(() => { if (!this.players.has(rec.userId)) return; const old = rec.character; rec.player.props.set('Character', 0); this.remove(old); this.loadCharacter(rec); }, this.respawnSeconds * 1000); rec.respawnTimer.unref?.(); }
  // Movement from the owning client: validated (ownership, finite numbers, speed budget, world bounds).
  applyMove(rec, cf, now = Date.now()) {
    if (!rec.character || rec.dead) return { ok: false, reason: 'no-character' };
    const p = cf?.pos; if (!p || ![p.x, p.y, p.z].every(Number.isFinite)) return { ok: false, reason: 'bad-number' };
    if (Math.abs(p.x) > 1e5 || Math.abs(p.z) > 1e5 || Math.abs(p.y) > 1e5) return { ok: false, reason: 'out-of-bounds' };
    const root = rec.parts.HumanoidRootPart; const old = root.props.get('CFrame').pos; const dt = Math.max((now - rec.lastMove) / 1000, 0.05);
    const dh = Math.hypot(p.x - old.x, p.z - old.z); const budget = 16 * 1.5 * Math.min(dt, 2) + 4; // WalkSpeed 16 * slack
    if (dh > budget) return { ok: false, reason: 'speed', allowed: budget, got: dh };
    rec.lastMove = now; this._moveCharacter(rec, cf);
    if (p.y < this.fallenY) this.kill(rec);
    return { ok: true };
  }
  _moveCharacter(rec, cf) {
    const root = rec.parts.HumanoidRootPart; const old = root.props.get('CFrame').pos; const d = { x: cf.pos.x - old.x, y: cf.pos.y - old.y, z: cf.pos.z - old.z };
    for (const part of Object.values(rec.parts)) { const c = part.props.get('CFrame'); const n = { pos: { x: c.pos.x + d.x, y: c.pos.y + d.y, z: c.pos.z + d.z }, rot: part === root ? (cf.rot || c.rot) : c.rot }; this.setProp(part, 'CFrame', n); }
  }
}

const ID3 = [1, 0, 0, 0, 1, 0, 0, 0, 1];
const PART_DEFS = [ // name, size, offset from torso center
  ['HumanoidRootPart', [2, 2, 1], [0, 0, 0]], ['Torso', [2, 2, 1], [0, 0, 0]], ['Head', [2, 1, 1], [0, 1.5, 0]], ['Left Arm', [1, 2, 1], [-1.5, 0, 0]],
  ['Right Arm', [1, 2, 1], [1.5, 0, 0]], ['Left Leg', [1, 2, 1], [-0.5, -2, 0]], ['Right Leg', [1, 2, 1], [0.5, -2, 0]]];
const COLOR_KEY = { HeadColor: 'Head', TorsoColor: 'Torso', LeftArmColor: 'Left Arm', RightArmColor: 'Right Arm', LeftLegColor: 'Left Leg', RightLegColor: 'Right Leg' };
const assetUrl = id => `http://www.roblox.com/asset/?id=${id}`;
function buildCharacter(world, ws, rec, sp) {
  const av = rec.avatar || {}; const bc = av.bodyColors || {}; const missing = [];
  const model = world.create('Model', ws); model.props.set('Name', rec.username);
  const parts = {};
  for (const [name, size, off] of PART_DEFS) {
    const part = world.create('Part', model); part.props.set('Name', name); part.props.set('size', { x: size[0], y: size[1], z: size[2] });
    part.props.set('CFrame', { pos: { x: sp.x + off[0], y: sp.y + off[1], z: sp.z + off[2] }, rot: ID3.slice() });
    part.props.set('Anchored', false); part.props.set('CanCollide', name !== 'HumanoidRootPart'); part.props.set('Transparency', name === 'HumanoidRootPart' ? 1 : 0);
    const key = Object.keys(COLOR_KEY).find(k => COLOR_KEY[k] === name); part.props.set('BrickColor', key ? (bc[key] ?? 194) : (name === 'Torso' ? (bc.TorsoColor ?? 194) : 194));
    parts[name] = part;
  }
  const hum = world.create('Humanoid', model); hum.props.set('Name', 'Humanoid'); hum.props.set('MaxHealth', 100); hum.props.set('Health', 100); hum.props.set('WalkSpeed', 16);
  const colors = world.create('BodyColors', model); colors.props.set('Name', 'Body Colors');
  for (const k of Object.keys(COLOR_KEY)) colors.props.set(k, bc[k] ?? 194);
  const addClothing = (cls, prop, id, label) => { if (!id) return; const c = world.create(cls, model); c.props.set('Name', label); c.props.set(prop, assetUrl(id)); };
  addClothing('Shirt', 'ShirtTemplate', av.shirtId, 'Shirt'); addClothing('Pants', 'PantsTemplate', av.pantsId, 'Pants'); addClothing('ShirtGraphic', 'Graphic', av.tshirtId, 'Shirt Graphic');
  const mesh = world.create('SpecialMesh', parts.Head); mesh.props.set('Name', 'Mesh'); mesh.props.set('MeshType', 0);
  if (av.faceId) { const d = world.create('Decal', parts.Head); d.props.set('Name', 'face'); d.props.set('Texture', assetUrl(av.faceId)); }
  // Hats / heads / gear need the actual asset content. If unavailable they stay MISSING (never substituted).
  for (const id of [...(av.hatIds || []), ...(av.headId ? [av.headId] : [])]) {
    const asset = world.loadAsset ? world.loadAsset(id) : null; if (!asset) { missing.push(id); continue; }
    for (const root of asset.roots || []) if (root.className === 'Hat' || root.className === 'Accessory') { const h = cloneImported(world, root, model); if (h) { h.props.set('Name', root.name); } }
  }
  return { model, parts, missing };
}
function cloneImported(world, src, parent) {
  if (classStatus(src.className, world.schema) !== 'replicable') return null;
  const w = world.create(src.className, parent); const cls = world.schema.classes.get(src.className);
  for (const [k, pv] of src.props) { const pd = cls.byName.get(k); if (!pd || !wire.SUPPORTED.has(pd.kind) || pd.kind === 'Object') continue; const v = wire.coerce(pd.kind, pv); if (v !== undefined) w.props.set(k, v); }
  for (const c of src.children) cloneImported(world, c, w); return w;
}
module.exports = { World, WInst, classStatus, classReplicable, SCRIPT_CLASSES, SERVER_ONLY };
