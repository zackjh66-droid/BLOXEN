'use strict';
// ScriptHost: runs preserved Script instances (server Scripts only) inside the sandboxed Lua interpreter against the game-server World.
// STATUS: UNIT-TESTED + SIMULATOR-TESTED. It is DISABLED BY DEFAULT (GameServer option `scripts: true`). Nothing here is REAL-CLIENT-TESTED.
//
// What this is NOT: it is not Roblox's engine. Only the API surface below exists; everything else raises a Lua error naming the missing member and is
// recorded in host.unsupported so a place's compatibility report can say exactly what did not run. There is NO server-side physics: Touched, joints,
// BodyMovers, Velocity, collisions and Explosion effects never happen. Scripts that depend on them are reported as PARTIAL/UNSUPPORTED, not hidden.
const { Interp, Scheduler, LuaTable, LuaError, typeOf, toNumber, truthy } = require('./lua');
const { classStatus } = require('../gameserver/world'); const { BRICK_COLORS: BRICKHEX } = require('../lib/services');
const LEGACY_HUMANOID_PARTS = { Torso: 'Torso', LeftArm: 'Left Arm', RightArm: 'Right Arm', LeftLeg: 'Left Leg', RightLeg: 'Right Leg', Head: 'Head' };

const BRICK_NAMES = { 1: 'White', 5: 'Brick yellow', 9: 'Light reddish violet', 18: 'Nougat', 21: 'Bright red', 23: 'Bright blue', 24: 'Bright yellow', 26: 'Black', 28: 'Dark green', 37: 'Bright green', 102: 'Medium blue', 105: 'Br. yellowish orange', 106: 'Bright orange', 119: 'Br. yellowish green', 194: 'Medium stone grey', 199: 'Dark stone grey', 217: 'Brown', 226: 'Cool yellow', 1001: 'Institutional white', 1003: 'Really black', 1004: 'Really red', 1010: 'Really blue' }; // partial (same 22 as src/lib/services.js)
const ALIASES = { size: 'Size', formFactor: 'FormFactor', shape: 'Shape' };
const allKids = w => w.shadowKids && w.shadowKids.length ? w.children.concat(w.shadowKids) : w.children; // world children + never-replicated script shadows
const METHOD_ALIAS = { children: 'GetChildren', service: 'GetService' };
const BLOCKED_SERVICES = new Set(['HttpService', 'DataStoreService', 'MarketplaceService', 'InsertService', 'BadgeService', 'GamePassService', 'PointsService', 'ScriptContext', 'NetworkServer', 'NetworkClient']);

class LuaVec3 {
  constructor(x, y, z) { this.x = x; this.y = y; this.z = z; }
  luaGet(k) { const l = String(k).toLowerCase(); if (l === 'x') return this.x; if (l === 'y') return this.y; if (l === 'z') return this.z; if (l === 'magnitude') return Math.hypot(this.x, this.y, this.z); if (l === 'unit') { const m = Math.hypot(this.x, this.y, this.z) || 1; return new LuaVec3(this.x / m, this.y / m, this.z / m); } throw new LuaError(`${k} is not a valid member of Vector3`); }
  luaEq(o) { return o instanceof LuaVec3 && o.x === this.x && o.y === this.y && o.z === this.z; }
  luaToString() { return `${this.x}, ${this.y}, ${this.z}`; }
  get luaMeta() { return VEC3_META; }
}
const vecOp = (name, f) => (a) => { const [l, r] = a; if (l instanceof LuaVec3 && r instanceof LuaVec3) return [f(l, r)]; if (l instanceof LuaVec3 && typeof r === 'number') return [f(l, new LuaVec3(r, r, r))]; if (typeof l === 'number' && r instanceof LuaVec3) return [f(new LuaVec3(l, l, l), r)]; throw new LuaError(`attempt to perform arithmetic (${name}) on Vector3 and ${typeOf(r)}`); };
const VEC3_META = new LuaTable();
VEC3_META.set('__add', vecOp('add', (a, b) => new LuaVec3(a.x + b.x, a.y + b.y, a.z + b.z))); VEC3_META.set('__sub', vecOp('sub', (a, b) => new LuaVec3(a.x - b.x, a.y - b.y, a.z - b.z)));
VEC3_META.set('__mul', vecOp('mul', (a, b) => new LuaVec3(a.x * b.x, a.y * b.y, a.z * b.z))); VEC3_META.set('__div', vecOp('div', (a, b) => new LuaVec3(a.x / b.x, a.y / b.y, a.z / b.z)));
VEC3_META.set('__unm', a => [new LuaVec3(-a[0].x, -a[0].y, -a[0].z)]);
class LuaCFrame {
  constructor(pos, rot) { this.pos = pos; this.rot = rot || [1, 0, 0, 0, 1, 0, 0, 0, 1]; }
  luaGet(k) { const l = String(k).toLowerCase(); if (l === 'p' || l === 'position') return new LuaVec3(this.pos.x, this.pos.y, this.pos.z); if (l === 'x') return this.pos.x; if (l === 'y') return this.pos.y; if (l === 'z') return this.pos.z; throw new LuaError(`${k} is not a valid member of CFrame`); }
  get luaMeta() { return CF_META; }
  luaToString() { return [this.pos.x, this.pos.y, this.pos.z, ...this.rot].join(', '); }
}
const CF_META = new LuaTable();
CF_META.set('__mul', a => { const [l, r] = a; const R = l.rot; if (r instanceof LuaCFrame) { const Q = r.rot; const m = []; for (let i = 0; i < 3; i++) for (let j = 0; j < 3; j++) m.push(R[i * 3] * Q[j] + R[i * 3 + 1] * Q[3 + j] + R[i * 3 + 2] * Q[6 + j]); const p = r.pos; return [new LuaCFrame({ x: l.pos.x + R[0] * p.x + R[1] * p.y + R[2] * p.z, y: l.pos.y + R[3] * p.x + R[4] * p.y + R[5] * p.z, z: l.pos.z + R[6] * p.x + R[7] * p.y + R[8] * p.z }, m)]; } if (r instanceof LuaVec3) return [new LuaVec3(l.pos.x + R[0] * r.x + R[1] * r.y + R[2] * r.z, l.pos.y + R[3] * r.x + R[4] * r.y + R[5] * r.z, l.pos.z + R[6] * r.x + R[7] * r.y + R[8] * r.z)]; throw new LuaError('attempt to multiply a CFrame by ' + typeOf(r)); });
const INPUT_EVENTS = new Set(['Selected', 'Deselected', 'Equipped', 'Unequipped', 'Activated', 'Deactivated', 'MouseClick', 'MouseHoverEnter', 'MouseHoverLeave']);
class LuaColor3 { constructor(r, g, b) { this.r = r; this.g = g; this.b = b; } luaGet(k) { const l = String(k).toLowerCase(); if (l === 'r' || l === 'g' || l === 'b') return this[l]; throw new LuaError(`${k} is not a valid member of Color3`); } }
class LuaBrickColor { constructor(n) { this.n = n; } luaGet(k) { const l = String(k).toLowerCase(); if (l === 'number') return this.n; if (l === 'name') return BRICK_NAMES[this.n] || 'Unknown'; throw new LuaError(`${k} is not a valid member of BrickColor`); } luaEq(o) { return o instanceof LuaBrickColor && o.n === this.n; } luaToString() { return BRICK_NAMES[this.n] || String(this.n); } }
class LuaEnumItem { constructor(en, name, value) { this.en = en; this.name = name; this.value = value; } luaGet(k) { if (k === 'Name') return this.name; if (k === 'Value') return this.value; throw new LuaError(`${k} is not a valid member of EnumItem`); } luaEq(o) { return o instanceof LuaEnumItem && o.en === this.en && o.value === this.value; } luaToString() { return `Enum.${this.en}.${this.name}`; } }

class LuaSignal {
  constructor(host, name) { this.host = host; this.name = name; this.conns = []; }
  luaGet(k) {
    const l = String(k).toLowerCase();
    if (l === 'connect') return a => { const fn = a[1]; if (!fn || (typeOf(fn) !== 'function')) throw new LuaError('Connect: function expected'); const c = { fn, connected: true, luaGet: kk => String(kk).toLowerCase() === 'disconnect' ? (() => { c.connected = false; this.conns = this.conns.filter(x => x !== c); return []; }) : (String(kk).toLowerCase() === 'connected' ? c.connected : undefined) }; this.conns.push(c); this.host._onConnect(this); return [c]; };
    if (l === 'wait') return function* (a, I) { const sig = a[0]; const box = { done: false, args: [] }; const c = { fn: null, box, connected: true }; sig.conns.push(c); while (!box.done) yield { wait: 0.03 }; return box.args; };
    throw new LuaError(`${k} is not a valid member of Event`);
  }
  fire(args) { for (const c of this.conns.slice()) { if (!c.connected) continue; if (c.box) { c.box.done = true; c.box.args = args; this.conns = this.conns.filter(x => x !== c); continue; } this.host.scheduler.spawn(c.fn, args, 'event:' + this.name); } }
}

class LuaInstance {
  constructor(host, w) { this.host = host; this.w = w; }
  luaToString() { return this.w.name; }
  get luaMeta() { return null; }
  luaGet(key) { return this.host._get(this, String(key)); }
  luaSet(key, val) { return this.host._set(this, String(key), val); }
  luaEq(o) { return o === this; }
}

// ---- geometry for the contact test (oriented boxes; CFrame = {pos, rot:[r00,r01,r02,r10,r11,r12,r20,r21,r22]} row-major)
function boxOf(inst) {
  const cf = inst.props.get('CFrame'); const sz = inst.props.get('Size') || inst.props.get('size'); if (!cf || !cf.pos || !sz) return null;
  const v = sz.value || sz; const x = v.x ?? v.X, y = v.y ?? v.Y, z = v.z ?? v.Z; const R = cf.rot || [1, 0, 0, 0, 1, 0, 0, 0, 1]; if (![x, y, z, cf.pos.x, cf.pos.y, cf.pos.z].every(Number.isFinite)) return null;
  return { c: [cf.pos.x, cf.pos.y, cf.pos.z], h: [x / 2, y / 2, z / 2], a: [[R[0], R[3], R[6]], [R[1], R[4], R[7]], [R[2], R[5], R[8]]] };   // a[i] = i-th local axis in world space
}
const dot3 = (u, v) => u[0] * v[0] + u[1] * v[1] + u[2] * v[2], cross3 = (u, v) => [u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0]];
function obbOverlap(A, B, eps = 0.05) {   // separating-axis test over the 15 candidate axes; eps = contact tolerance in studs
  const t0 = [B.c[0] - A.c[0], B.c[1] - A.c[1], B.c[2] - A.c[2]]; const axes = [...A.a, ...B.a];
  for (const i of A.a) for (const j of B.a) { const c = cross3(i, j); if (Math.hypot(...c) > 1e-6) axes.push(c); }
  for (const ax of axes) { const n = Math.hypot(...ax); if (n < 1e-9) continue; const u = ax.map(q => q / n);
    const ra = A.h.reduce((sum, h, k) => sum + h * Math.abs(dot3(A.a[k], u)), 0), rb = B.h.reduce((sum, h, k) => sum + h * Math.abs(dot3(B.a[k], u)), 0);
    if (Math.abs(dot3(t0, u)) > ra + rb + eps) return false; }
  return true;
}
class ScriptHost {
  constructor(world, { print = () => {}, budget = 2_000_000, enabled = true, logger = () => {}, teleport = null } = {}) {
    this.teleportResolver = teleport; this.teleportLog = []; this.world = world; this.log = logger; this.output = []; this.unsupported = new Map(); this.scripts = []; this.errors = []; this.cache = new Map(); this.signals = new Map(); this.deadHum = new WeakSet(); this.touchedConnections = 0;
    this.interp = new Interp({ budget, print: s => { this.output.push(s); if (this.output.length > 500) this.output.shift(); print(s); } });
    this.scheduler = new Scheduler(this.interp, { onError: (t, e) => { const msg = e.budget ? 'script exceeded its execution budget and was stopped' : String(e.value); this.errors.push({ thread: t.name, message: msg }); this.log('script error: ' + msg); const rec = this.scripts.find(s => s.thread === t); if (rec) { rec.status = e.budget ? 'budget-exceeded' : 'error'; rec.error = msg; } } });
    this.enabled = enabled; this._installGlobals(); this.unsub = world.onChange(ev => this._onWorldChange(ev));
  }
  close() { this.unsub(); }
  // -------------------------------------------------------------- script lifecycle
  start() {
    for (const src of this.world.scriptSources) {
      const rec = { name: src.name, className: src.className, path: this._path(src), bytes: Buffer.byteLength(src.source), status: 'pending', error: null, thread: null };
      this.scripts.push(rec);
      if (src.className !== 'Script') { rec.status = 'not-run:' + (src.className === 'LocalScript' ? 'client-side script' : 'unsupported class'); continue; }
      if (src.disabled) { rec.status = 'not-run:disabled'; continue; }
      if (!src.parent || !this.world.inWorld(src.parent)) { rec.status = 'not-run:parent-not-imported'; continue; }
      const env = this.interp.makeEnv(); env.set('script', new LuaScriptObject(this, src));
      let fn; try { fn = this.interp.load(src.source.replace(/\r\n/g, '\n'), `${rec.path}`, env); } catch (e) { if (e.lua) { rec.status = 'syntax-error'; rec.error = String(e.value); this.errors.push({ thread: rec.path, message: rec.error }); continue; } throw e; }
      rec.thread = this.scheduler.spawn(fn, [], rec.path); rec.status = 'running';
    }
    return this;
  }
  // advance virtual time, running everything that is due. The game server calls this from a timer.
  step(dt) { this.scheduler.step(dt); this._touchPass(); for (const r of this.scripts) if (r.thread && r.thread.dead && r.status === 'running') r.status = 'finished'; }
  _path(src) { const parts = [src.name]; for (let p = src.parent; p; p = p.parent) parts.unshift(p.name); return parts.join('.'); }
  _unsupported(what) { this.unsupported.set(what, (this.unsupported.get(what) || 0) + 1); }
  _onConnect(sig) { if (INPUT_EVENTS.has(sig.name)) this._unsupported(`event ${sig.name}: connected but never fires (player input is not delivered to tools yet)`);
    if (sig.name === 'Touched' || sig.name === 'TouchEnded') { this.touchedConnections++; this._unsupported(`event ${sig.name}: fires only when a player's character overlaps the part (step-sampled box test); part-vs-part contact is not simulated`); } }
  // ---- Touched/TouchEnded: there is still NO physics engine. The only contact we model is a player's character parts overlapping a listening part (oriented-box test,
  // sampled once per step). Anything else that would touch in real Roblox (falling/thrown parts, vehicles, Velocity-driven motion) never fires. Not collision response.
  _touchPass() {
    if (!this.touchedConnections) return; const players = [...this.world.players.values()].filter(r => r.character && !r.dead && r.parts);
    this.touching = this.touching || new Map();
    for (const [w, m] of this.signals) {
      const tc = m.get('Touched'), te = m.get('TouchEnded'); const live = x => x && x.conns.some(c => c.connected); if (!live(tc) && !live(te)) continue;
      if (!w.parent && !this.world.byId?.has?.(w.id)) continue; if (!this.isA(w, 'BasePart') || players.some(r => this._within(w, r.character))) continue;
      const box = boxOf(w); if (!box) continue; let set = this.touching.get(w); if (!set) { set = new Set(); this.touching.set(w, set); }
      const now = new Set();
      for (const r of players) for (const part of Object.values(r.parts)) { const b = boxOf(part); if (b && obbOverlap(box, b)) now.add(part); }
      for (const part of now) if (!set.has(part)) { set.add(part); if (live(tc)) tc.fire([this.wrap(part)]); }
      for (const part of [...set]) if (!now.has(part)) { set.delete(part); if (live(te)) te.fire([this.wrap(part)]); }
    }
  }
  _touchBox(w) { return boxOf(w); }
  _within(inst, root) { for (let c = inst; c; c = c.parent) if (c === root) return true; return false; }
  report() {
    const st = {}; for (const r of this.scripts) st[r.status] = (st[r.status] || 0) + 1;
    return { scripts: this.scripts.map(r => ({ path: r.path, class: r.className, bytes: r.bytes, status: r.thread && r.thread.dead && r.status === 'running' ? 'finished' : r.status, error: r.error })), statusCounts: st, unsupported: Object.fromEntries(this.unsupported), touchedConnections: this.touchedConnections, errors: this.errors.slice(-20), note: 'No server-side physics or joints exist; scripts that depend on them are PARTIAL at best.' };
  }
  // -------------------------------------------------------------- wrapping
  wrapList(arr) { const t = new LuaTable(); arr.forEach((x, i) => t.set(i + 1, this.wrap(x))); return t; }
  wrap(w) { if (!w) return undefined; let o = this.cache.get(w); if (!o) { o = new LuaInstance(this, w); this.cache.set(w, o); } return o; }
  sig(w, name) { let m = this.signals.get(w); if (!m) { m = new Map(); this.signals.set(w, m); } let s = m.get(name); if (!s) { s = new LuaSignal(this, name); m.set(name, s); } return s; }
  fire(w, name, args) { const m = this.signals.get(w); const s = m && m.get(name); if (s) s.fire(args); }
  _onWorldChange(ev) {
    if (ev.op === 'prop') {
      const w = ev.inst; this.fire(w, 'Changed', [ev.name]);
      if (w.className === 'Player' && ev.name === 'Character' && w.props.get('Character')) this.fire(w, 'CharacterAdded', [this.wrap(this.world.byId.get(w.props.get('Character')))]);
      if (w.className === 'Humanoid' && ev.name === 'Health') { const h = w.props.get('Health'); this.fire(w, 'HealthChanged', [h]); if (h <= 0 && !this.deadHum.has(w)) { this.deadHum.add(w); this.fire(w, 'Died', []); } }
    } else if (ev.op === 'new') {
      const w = ev.inst; if (w.parent) { this.fire(w.parent, 'ChildAdded', [this.wrap(w)]); if (w.parent.className === 'Players' && w.className === 'Player') this.fire(w.parent, 'PlayerAdded', [this.wrap(w)]); }
    } else if (ev.op === 'del') {
      if (ev.parent) { this.fire(ev.parent, 'ChildRemoved', [this.wrap(ev.inst)]); if (ev.parent.className === 'Players' && ev.inst.className === 'Player') this.fire(ev.parent, 'PlayerRemoving', [this.wrap(ev.inst)]); }
    }
  }
  // -------------------------------------------------------------- value conversion
  toLua(kind, typeName, v) {
    switch (kind) {
      case 'Vector3': return new LuaVec3(v.x, v.y, v.z);
      case 'CFrame': case 'CoordinateFrame': return new LuaCFrame({ ...v.pos }, v.rot.slice());
      case 'Color3': return new LuaColor3(v.r, v.g, v.b);
      case 'BrickColor': return new LuaBrickColor(v);
      case 'Object': return v ? this.wrap(this.world.byId.get(v)) : undefined;
      case 'enum': { const items = this.enumItems(typeName); const name = items && Object.keys(items).find(k => items[k] === v); return name !== undefined ? new LuaEnumItem(typeName, name, v) : v; }
      case 'Content': case 'string': case 'ProtectedString': case 'BinaryString': return v;
      case 'bool': case 'int': case 'float': case 'double': return v;
      default: return undefined; // Vector2, UDim2 ... no Lua constructor yet
    }
  }
  enumItems(name) { const d = this._enums || (this._enums = require('../../preservation/reference/api-0.205.0.61876.json').enums); return d[name]; }
  fromLua(kind, val, prop) {
    const bad = exp => new LuaError(`invalid value for ${prop}: ${exp} expected, got ${typeOf(val)}`);
    switch (kind) {
      case 'bool': return truthy(val);
      case 'int': { const n = toNumber(val); if (typeof val === 'boolean' || n === undefined) throw bad('number'); return Math.trunc(n); }
      case 'float': case 'double': { const n = toNumber(val); if (typeof val === 'boolean' || n === undefined) throw bad('number'); return n; }
      case 'string': case 'Content': case 'ProtectedString': case 'BinaryString': { if (typeof val === 'string') return val; if (typeof val === 'number') return this.interp.tostr(val); throw bad('string'); }
      case 'Vector3': if (val instanceof LuaVec3) return { x: val.x, y: val.y, z: val.z }; throw bad('Vector3');
      case 'CFrame': case 'CoordinateFrame': if (val instanceof LuaCFrame) return { pos: { ...val.pos }, rot: val.rot.slice() }; throw bad('CFrame');
      case 'Color3': if (val instanceof LuaColor3) return { r: val.r, g: val.g, b: val.b }; throw bad('Color3');
      case 'BrickColor': if (val instanceof LuaBrickColor) return val.n; if (typeof val === 'number') return val; throw bad('BrickColor');
      case 'Object': if (val === undefined) return 0; if (val instanceof LuaInstance) return this.world.inWorld(val.w) ? val.w.id : 0; throw bad('Instance');
      case 'enum': if (val instanceof LuaEnumItem) return val.value; if (typeof val === 'number') return val; throw bad('EnumItem');
      default: throw new LuaError(`property ${prop} has an unsupported type (${kind}) in this sandbox`);
    }
  }
  propDef(w, key) { // exact, known serialization aliases (size/Size...), then legacy lowerCamel -> UpperCamel (e.g. `position` -> `Position`), then server-local (NotReplicated) members from the API dump
    const cls = this.world.schema.classes.get(w.className); if (!cls) return this._localProp(w.className, key) || this._localProp(w.className, key[0].toUpperCase() + key.slice(1)); const inv = Object.keys(ALIASES).find(a => ALIASES[a] === key);
    const tries = [key, ALIASES[key], inv, key[0].toUpperCase() + key.slice(1)]; for (const t of tries) if (t && cls.byName.has(t)) return cls.byName.get(t);
    return this._localProp(w.className, key) || this._localProp(w.className, key[0].toUpperCase() + key.slice(1));
  }
  // NotReplicated members (Humanoid.Health, Humanoid.Jump ...) live only on the server. They are stored in World props (never sent on the wire: the schema has no slot for them).
  _localProp(cls, name) {
    const d = this._dump || (this._dump = require('../../preservation/reference/api-0.205.0.61876.json').classes);
    for (let c = cls; c && d[c]; c = d[c].super) { const p = d[c].props.find(x => x[0] === name && x[3].includes('NotReplicated') && !x[3].includes('ReadOnly')); if (p) { const kind = ['bool', 'int', 'float', 'double', 'string', 'Vector3', 'Color3'].includes(p[1]) ? p[1] : null; if (kind) return { name: p[0], type: p[1], kind, local: true, tags: [] }; } }
    return null;
  }
  defaultFor(kind) { return ({ Vector3: { x: 0, y: 0, z: 0 }, Color3: { r: 0, g: 0, b: 0 }, bool: false, int: 0, float: 0, double: 0, string: '', Content: '', ProtectedString: '', BrickColor: 194, Object: 0, enum: 0 })[kind]; }
  // -------------------------------------------------------------- Instance members
  _get(o, key) {
    const w = o.w; const w0 = this.world;
    // methods (Roblox of this era accepted lowerCamel aliases)
    const canon = METHOD_ALIAS[key] || (key[0] >= 'a' && key[0] <= 'z' ? key[0].toUpperCase() + key.slice(1) : key);
    const m = this._methods()[canon]; if (m && !(key === 'Name' || key === 'Parent' || key === 'ClassName')) { if (m.classes && !m.classes.some(c => this.isA(w, c))) { /* fallthrough to props */ } else return (a) => m.fn.call(this, o, a.slice(1)); }
    if (key === 'Name' || key === 'name') return w.name; if (key === 'ClassName' || key === 'className') return w.className; if (key === 'Parent' || key === 'parent') return this.wrap(w.parent || undefined);
    const ev = { Changed: 1, ChildAdded: 1, ChildRemoved: 1, Touched: 1, TouchEnded: 1, Died: 'Humanoid', HealthChanged: 'Humanoid', PlayerAdded: 'Players', PlayerRemoving: 'Players', CharacterAdded: 'Player', CharacterRemoving: 'Player', DescendantAdded: 1,
      // player-input events: the signal exists so scripts load, but nothing in the server path delivers input to a tool yet, so they never fire (reported as unsupported)
      Selected: 'HopperBin', Deselected: 'HopperBin', Equipped: 'Tool', Unequipped: 'Tool', Activated: 'Tool', Deactivated: 'Tool', MouseClick: 'ClickDetector', MouseHoverEnter: 'ClickDetector', MouseHoverLeave: 'ClickDetector' };
    if (ev[key] && (ev[key] === 1 || this.isA(w, ev[key]))) { if (key === 'CharacterAdded') return this.sig(w, 'CharacterAdded'); return this.sig(w, key); }
    const pd = this.propDef(w, key);
    if (w.className === 'Player' && key === 'Character') { return w.props.get('Character') ? this.wrap(w0.byId.get(w.props.get('Character'))) : undefined; }
    // legacy members the 2015 API still lists (NotReplicated): Humanoid.Torso/LeftLeg/... point at the character's parts; BasePart.Color is the Color3 view of BrickColor (hex table INFERRED)
    if (w.className === 'Humanoid' && LEGACY_HUMANOID_PARTS[key]) { const par = w.parent; return this.wrap(par && par.children.find(c => c.name === LEGACY_HUMANOID_PARTS[key])); }
    if (this.isA(w, 'BasePart') && key === 'Color') { const n = w.props.get('BrickColor'); const hex = (BRICKHEX[n === undefined ? 194 : n] || BRICKHEX[194])[1]; return new LuaColor3(parseInt(hex.slice(1, 3), 16) / 255, parseInt(hex.slice(3, 5), 16) / 255, parseInt(hex.slice(5, 7), 16) / 255); }
    if (this.isA(w, 'BasePart') && (key === 'Position')) { const cf = w.props.get('CFrame'); if (cf) return new LuaVec3(cf.pos.x, cf.pos.y, cf.pos.z); }
    if (pd) { const v = w.props.has(pd.name) ? w.props.get(pd.name) : (w.className === 'Humanoid' && pd.name === 'Health' ? (w.props.get('MaxHealth') ?? 100) : this.defaultFor(pd.kind)); const out = this.toLua(pd.kind, pd.type, v); if (out === undefined && v !== undefined && v !== 0 && pd.kind !== 'Object') { this._unsupported(`property ${w.className}.${key} (${pd.kind}) has no Lua value type`); throw new LuaError(`property ${key} (${pd.kind}) is not readable in this sandbox`); } return out; }
    for (const c of allKids(w)) if (c.name === key) return this.wrap(c);
    throw new LuaError(`${key} is not a valid member of ${w.className}`);
  }
  _set(o, key, val) {
    const w = o.w; const w0 = this.world;
    if (key === 'Parent' || key === 'parent') { if (val !== undefined && !(val instanceof LuaInstance)) throw new LuaError('Parent must be an Instance or nil'); if (val && this._isDescendant(val.w, w)) throw new LuaError('Attempt to set ' + w.name + ' as its own descendant'); if (['Workspace', 'Players', 'Lighting'].includes(w.className) && !val) throw new LuaError('cannot set Parent of a service'); if (w.className === 'Player' || (w.className === 'Model' && this._isCharacter(w))) { if (val) throw new LuaError('cannot reparent a player or character from a script in this sandbox'); } w0.attach(w, val ? val.w : null); return; }
    if (key === 'Name' || key === 'name') { this._setProp(w, 'Name', String(this.interp.tostr(val))); return; }
    if (this.isA(w, 'BasePart') && key === 'Color') { if (!(val instanceof LuaColor3)) throw new LuaError('Color must be a Color3'); let best = 194, bd = Infinity; for (const [n, [, hex]] of Object.entries(BRICKHEX)) { const d = (parseInt(hex.slice(1, 3), 16) / 255 - val.r) ** 2 + (parseInt(hex.slice(3, 5), 16) / 255 - val.g) ** 2 + (parseInt(hex.slice(5, 7), 16) / 255 - val.b) ** 2; if (d < bd) { bd = d; best = +n; } } this._setProp(w, 'BrickColor', best); return; }   // nearest BrickColor: Color3 is not replicated in this era
    if (this.isA(w, 'BasePart') && key === 'Position') { const cf = w.props.get('CFrame') || { pos: { x: 0, y: 0, z: 0 }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] }; if (!(val instanceof LuaVec3)) throw new LuaError('Position must be a Vector3'); this._setProp(w, 'CFrame', { pos: { x: val.x, y: val.y, z: val.z }, rot: cf.rot.slice() }); return; }
    if (this._methods()[key[0].toUpperCase() + key.slice(1)] && !this.propDef(w, key)) throw new LuaError(`cannot assign to method ${key}`);
    const pd = this.propDef(w, key); if (!pd) throw new LuaError(`${key} is not a valid member of ${w.className}`);
    if (pd.tags && pd.tags.includes('ReadOnly')) throw new LuaError(`${key} is read-only`);
    if (w.className === 'Player' && key === 'Character') throw new LuaError('Character is managed by the server');
    const v = this.fromLua(pd.kind, val, key);
    if (w.className === 'Humanoid' && pd.name === 'Health' && v <= 0) { const rec = [...w0.players.values()].find(r => r.character && r.character.children.includes(w)); if (rec) { w0.kill(rec); return; } }
    this._setProp(w, pd.name, v);
  }
  _setProp(w, name, v) { if (this.world.inWorld(w)) this.world.setProp(w, name, v); else w.props.set(name, v); }
  _isDescendant(a, b) { for (let p = a; p; p = p.parent) if (p === b) return true; return false; }
  _isCharacter(w) { for (const r of this.world.players.values()) if (r.character === w) return true; return false; }
  isA(w, base) { const s = this.world.schema; for (let c = w.className; c; c = s.classes.get(c)?.super) if (c === base) return true; return false; }
  _methods() {
    if (this._m) return this._m; const H = this; const wrap = w => H.wrap(w); const T = arr => { const t = new LuaTable(); arr.forEach((x, i) => t.set(i + 1, x)); return t; };
    this._m = {
      FindFirstChild: { fn(o, a) { const name = String(a[0]); const rec = truthy(a[1]); const find = w => { for (const c of allKids(w)) if (c.name === name) return c; if (rec) for (const c of allKids(w)) { const r = find(c); if (r) return r; } return undefined; }; return [wrap(find(o.w))]; } },
      GetChildren: { fn(o) { return [T(allKids(o.w).map(wrap))]; } },
      IsA: { fn(o, a) { return [this.isA(o.w, String(a[0]))]; } },
      GetFullName: { fn(o) { const parts = []; for (let p = o.w; p; p = p.parent) parts.unshift(p.name); return [parts.join('.')]; } },
      IsDescendantOf: { fn(o, a) { return [a[0] instanceof LuaInstance && o.w !== a[0].w && this._isDescendant(o.w, a[0].w)]; } },
      Clone: { fn(o) { if (o.w.className === 'Player' || this._isCharacter(o.w)) return [undefined]; return [wrap(this.world.cloneTree(o.w))]; } },
      Remove: { fn(o) { if (['Workspace', 'Players', 'Lighting'].includes(o.w.className) && !o.w.parent) throw new LuaError('cannot remove a service'); this.world.attach(o.w, null); return []; } },
      Destroy: { fn(o) { if (['Workspace', 'Players', 'Lighting'].includes(o.w.className) && !o.w.parent) throw new LuaError('cannot destroy a service'); this.world.attach(o.w, null); return []; } },
      ClearAllChildren: { fn(o) { for (const c of o.w.children.slice()) this.world.attach(c, null); return []; } },
      MakeJoints: { fn(o) { this._unsupported('method MakeJoints: no-op (joints are not simulated)'); return []; } },
      BreakJoints: { fn(o) { this._unsupported('method BreakJoints: no-op (joints are not simulated)'); return []; } },
      GetMinutesAfterMidnight: { classes: ['Lighting'], fn(o) { const t = String(o.w.props.get('TimeOfDay') ?? '14:00:00').split(':').map(Number); return [(t[0] || 0) * 60 + (t[1] || 0) + (t[2] || 0) / 60]; } },
      SetMinutesAfterMidnight: { classes: ['Lighting'], fn(o, a) { const m = ((Math.floor(toNumber(a[0]) ?? 0) % 1440) + 1440) % 1440; const p = n => String(n).padStart(2, '0'); this._setProp(o.w, 'TimeOfDay', `${p(Math.floor(m / 60))}:${p(m % 60)}:00`); return []; } },
      GetPlayers: { classes: ['Players'], fn(o) { return [T(o.w.children.filter(c => c.className === 'Player').map(wrap))]; } },
      GetPlayerFromCharacter: { classes: ['Players'], fn(o, a) { const ch = a[0] instanceof LuaInstance ? a[0].w : null; for (const r of this.world.players.values()) if (r.character === ch) return [wrap(r.player)]; return [undefined]; } },
      TakeDamage: { classes: ['Humanoid'], fn(o, a) { const cur = o.w.props.get('Health') ?? 100; this._set(o, 'Health', Math.max(0, cur - (toNumber(a[0]) ?? 0))); return []; } },
    };
    return this._m;
  }
  // -------------------------------------------------------------- globals
  _installGlobals() {
    const H = this; const I = this.interp; const G = I.globals; const N = fn => fn; const num = (v, i, f) => { const n = toNumber(v); if (n === undefined || typeof v === 'boolean') throw new LuaError(`bad argument #${i} to '${f}' (number expected, got ${v === undefined ? 'no value' : typeOf(v)})`); return n; };
    const game = { luaGet(k) { k = String(k); const l = k[0].toLowerCase() + k.slice(1); if (l === 'getService' || l === 'service') return a => H._service(a[1]); if (l === 'workspace' || k === 'Workspace') return [H.world.service('Workspace')].map(w => H.wrap(w))[0]; if (k === 'PlaceId' || k === 'placeId') return H.world.placeInfo?.placeId ?? 0; if (l === 'isA') return () => [false]; const w = H.world.roots.find(r => r.className === k || r.name === k); if (w && classStatus(w.className, H.world.schema) === 'replicable') return H.wrap(w); if (k === 'TeleportService') return H._teleportService(); if (BLOCKED_SERVICES.has(k)) { H._unsupported(`service ${k} blocked by the sandbox`); throw new LuaError(`${k} is not available in the BLOXEN script sandbox`); } throw new LuaError(`${k} is not a valid member of DataModel`); }, luaToString() { return 'Game'; } };
    G.set('game', game); G.set('Game', game); G.set('workspace', this.wrap(this.world.service('Workspace'))); G.set('Workspace', G.get('workspace'));
    const inst = new LuaTable(); G.set('Instance', inst);
    inst.set('new', a => { const cls = String(a[0]); const s = this.world.schema.classes.get(cls); if (!s) throw new LuaError(`Unable to create an Instance of type "${cls}"`); if (s.tags.includes('NotCreatable') || s.tags.includes('Service')) throw new LuaError(`Unable to create an Instance of type "${cls}" (not creatable)`);
      if (cls === 'Script' || cls === 'LocalScript') { H._unsupported('Instance.new(Script): refused by sandbox'); throw new LuaError('creating scripts at runtime is not allowed in this sandbox'); }
      if (classStatus(cls, this.world.schema) !== 'replicable') { H._unsupported(`Instance.new("${cls}") unsupported (class is not replicated by this server)`); throw new LuaError(`Instance type "${cls}" is not supported by this server`); }
      const w = this.world.createDetached(cls); const o = this.wrap(w); if (a[1] !== undefined) { if (!(a[1] instanceof LuaInstance)) throw new LuaError('Instance.new: parent must be an Instance'); this.world.attach(w, a[1].w); } return [o]; });
    const V3 = new LuaTable(); G.set('Vector3', V3); V3.set('new', a => [new LuaVec3(a[0] === undefined ? 0 : num(a[0], 1, 'new'), a[1] === undefined ? 0 : num(a[1], 2, 'new'), a[2] === undefined ? 0 : num(a[2], 3, 'new'))]);
    const CF = new LuaTable(); G.set('CFrame', CF); CF.set('new', a => { if (a.length === 0) return [new LuaCFrame({ x: 0, y: 0, z: 0 })]; if (a[0] instanceof LuaVec3 && a.length === 1) return [new LuaCFrame({ x: a[0].x, y: a[0].y, z: a[0].z })]; if (a.length === 3) return [new LuaCFrame({ x: num(a[0], 1, 'new'), y: num(a[1], 2, 'new'), z: num(a[2], 3, 'new') })]; H._unsupported('CFrame.new overload (look-at / quaternion / 12-number)'); throw new LuaError('this CFrame.new overload is not supported in the sandbox'); });
    const C3 = new LuaTable(); G.set('Color3', C3); C3.set('new', a => [new LuaColor3(num(a[0] ?? 0, 1, 'new'), num(a[1] ?? 0, 2, 'new'), num(a[2] ?? 0, 3, 'new'))]);
    const BC = new LuaTable(); G.set('BrickColor', BC); BC.set('new', a => { if (typeof a[0] === 'number') return [new LuaBrickColor(a[0])]; const name = String(a[0]).toLowerCase(); const id = Object.keys(BRICK_NAMES).find(k => BRICK_NAMES[k].toLowerCase() === name); if (id === undefined) { H._unsupported('BrickColor.new(name) for names outside the partial 22-colour table (MISSING table)'); throw new LuaError(`BrickColor name "${a[0]}" is not in this sandbox's partial table`); } return [new LuaBrickColor(+id)]; });
    const Enum = { luaGet(k) { const items = H.enumItems(String(k)); if (!items) throw new LuaError(`${k} is not a valid Enum`); return { luaGet(n) { if (items[n] === undefined) throw new LuaError(`${n} is not a valid member of Enum.${k}`); return new LuaEnumItem(String(k), String(n), items[n]); } }; } };
    G.set('Enum', Enum);
    G.set('printidentity', () => []); G.set('LoadLibrary', () => { H._unsupported('LoadLibrary'); throw new LuaError('LoadLibrary is not available in the sandbox'); });
  }
  // TeleportService (BLOXEN-safe). Scripts name a HISTORICAL Roblox PlaceId; the host-supplied resolver maps it to an approved LOCAL place or refuses.
  // Nothing here contacts Roblox. A result is always logged in teleportLog: {placeId, userId, result, reason|local}. Without a resolver every call is refused.
  _teleportService() {
    const H = this; const asPlayer = v => { if (!(v instanceof LuaInstance) || v.w.className !== 'Player') throw new LuaError('TeleportService: argument is not a Player'); for (const r of H.world.players.values()) if (r.player === v.w) return r; return null; };
    const attempt = (placeId, playerArg, spawn) => { const rec = asPlayer(playerArg); let r; try { r = H.teleportResolver ? H.teleportResolver(placeId, rec && rec.userId) : { ok: false, result: 'unsupported-local-destination', reason: 'no teleport resolver configured on this host', robloxPlaceId: placeId }; } catch (e) { r = { ok: false, result: 'unsupported-local-destination', reason: 'resolver error: ' + e.message, robloxPlaceId: placeId }; }
      const entry = { at: H.scheduler.now !== undefined ? H.scheduler.now : 0, robloxPlaceId: Number(placeId) || String(placeId).slice(0, 24), userId: rec ? rec.userId : null, spawn: spawn ? String(spawn).slice(0, 40) : null, result: r.result, ok: !!r.ok, local: r.local || null, reason: r.reason || null };
      H.teleportLog.push(entry); if (H.teleportLog.length > 200) H.teleportLog.shift(); if (!r.ok) H._unsupported(`TeleportService: ${r.reason || r.result} (PlaceId ${entry.robloxPlaceId})`); H.log('teleport ' + JSON.stringify(entry)); return entry; };
    const ts = { luaGet(k) { k = String(k); const l = k.toLowerCase();
      if (l === 'teleport') return a => { attempt(a[1], a[2]); return []; };
      if (l === 'teleporttospawnbyname') return a => { attempt(a[1], a[3], a[2]); return []; };
      if (l === 'teleporttoplaceinstance') return a => { H._unsupported('TeleportService:TeleportToPlaceInstance: specific server instances are not supported'); H.teleportLog.push({ robloxPlaceId: Number(a[1]) || null, userId: null, result: 'unsupported-local-destination', ok: false, local: null, reason: 'instance-targeted teleport unsupported' }); return []; };
      if (l === 'teleportcancel' || l === 'setteleportgui') return () => [];
      H._unsupported('TeleportService.' + k + ': not implemented'); throw new LuaError(`${k} is not a valid member of TeleportService`); }, luaToString() { return 'TeleportService'; } };
    return ts;
  }
  _service(name) { const n = String(name); if (n === 'TeleportService') return [this._teleportService()]; if (BLOCKED_SERVICES.has(n)) { this._unsupported(`service ${n} blocked by the sandbox`); throw new LuaError(`${n} is not available in the BLOXEN script sandbox`); } const w = this.world.roots.find(r => r.className === n); if (!w || classStatus(n, this.world.schema) !== 'replicable') throw new LuaError(`Service ${n} is not available`); return [this.wrap(w)]; }
}
class LuaScriptObject {
  constructor(host, src) { this.host = host; this.src = src; }
  luaGet(k) { k = String(k); if (k === 'Parent' || k === 'parent') return this.host.wrap(this.src.parent); if (k === 'Name' || k === 'name') return this.src.name; if (k === 'ClassName' || k === 'className') return this.src.className; if (k === 'Disabled') return this.src.disabled;
    const kids = (this.src.shadow && this.src.shadow.children) || []; const l = k.toLowerCase();
    if (l === 'findfirstchild' || l === 'waitforchild') return a => [this.host.wrap(kids.find(c => c.name === String(a[1])))];
    if (l === 'getchildren' || l === 'children') return () => [this.host.wrapList(kids)];
    const c = kids.find(c => c.name === k); if (c) return this.host.wrap(c); throw new LuaError(`${k} is not a valid member of Script`); }
  luaSet(k) { throw new LuaError(`script.${k} cannot be set in this sandbox`); }
  luaToString() { return this.src.name; }
}
module.exports = { ScriptHost, LuaInstance, LuaVec3, LuaCFrame, obbOverlap, boxOf };
