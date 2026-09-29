'use strict';
// Binary Roblox place/model (.rbxl/.rbxm) STATIC parser.
// Implements RobloxAPI/spec formats/rbxl.md (CC-BY-SA-4.0 spec, https://github.com/RobloxAPI/spec) v0.
// Nothing is executed; Script sources are kept as inert strings.
const { lz4Decompress } = require('./lz4');
const { Instance, Place } = require('./model');

const SIG = Buffer.from([0x3c,0x72,0x6f,0x62,0x6c,0x6f,0x78,0x21,0x89,0xff,0x0d,0x0a,0x1a,0x0a]); // "<roblox!" 89 FF 0D 0A 1A 0A
const MAX_CHUNK = 1 << 30;

const T = { String:1, Bool:2, Int:3, Float:4, Double:5, UDim:6, UDim2:7, Ray:8, Faces:9, Axes:10, BrickColor:11, Color3:12, Vector2:13, Vector3:14,
  Vector2int16:15, CFrame:16, CFrameQuat:17, Token:18, Reference:19, Vector3int16:20, NumberSequence:21, ColorSequence:22, NumberRange:23, Rect:24,
  PhysicalProperties:25, Color3uint8:26, Int64:27, SharedString:28, Optional:30, UniqueId:31, Font:32 };
const TNAME = Object.fromEntries(Object.entries(T).map(([k, v]) => [v, k]));

const ROT = { 0x02:[1,0,0,0,1,0,0,0,1],0x03:[1,0,0,0,0,-1,0,1,0],0x05:[1,0,0,0,-1,0,0,0,-1],0x06:[1,0,0,0,0,1,0,-1,0],0x07:[0,1,0,1,0,0,0,0,-1],
 0x09:[0,0,1,1,0,0,0,1,0],0x0A:[0,-1,0,1,0,0,0,0,1],0x0C:[0,0,-1,1,0,0,0,-1,0],0x0D:[0,1,0,0,0,1,1,0,0],0x0E:[0,0,-1,0,1,0,1,0,0],
 0x10:[0,-1,0,0,0,-1,1,0,0],0x11:[0,0,1,0,-1,0,1,0,0],0x14:[-1,0,0,0,1,0,0,0,-1],0x15:[-1,0,0,0,0,1,0,1,0],0x17:[-1,0,0,0,-1,0,0,0,1],
 0x18:[-1,0,0,0,0,-1,0,-1,0],0x19:[0,1,0,-1,0,0,0,0,1],0x1B:[0,0,-1,-1,0,0,0,1,0],0x1C:[0,-1,0,-1,0,0,0,0,-1],0x1E:[0,0,1,-1,0,0,0,-1,0],
 0x1F:[0,1,0,0,0,-1,-1,0,0],0x20:[0,0,1,0,1,0,-1,0,0],0x22:[0,-1,0,0,0,1,-1,0,0],0x23:[0,0,-1,0,-1,0,-1,0,0] };

class Reader {
  constructor(buf) { this.b = buf; this.p = 0; }
  need(n) { if (this.p + n > this.b.length) throw new Error('rbxl: read past end of chunk'); }
  u8() { this.need(1); return this.b[this.p++]; }
  u32() { this.need(4); const v = this.b.readUInt32LE(this.p); this.p += 4; return v; }
  i32() { this.need(4); const v = this.b.readInt32LE(this.p); this.p += 4; return v; }
  f32() { this.need(4); const v = this.b.readFloatLE(this.p); this.p += 4; return v; }
  f64() { this.need(8); const v = this.b.readDoubleLE(this.p); this.p += 8; return v; }
  i16() { this.need(2); const v = this.b.readInt16LE(this.p); this.p += 2; return v; }
  str() { const n = this.u32(); this.need(n); const s = this.b.subarray(this.p, this.p + n); this.p += n; return s; }
  bytes(n) { this.need(n); const s = this.b.subarray(this.p, this.p + n); this.p += n; return s; }
  // byte-interleaved arrays of 4-byte words (big-endian within the interleave)
  words(n) { const raw = this.bytes(n * 4); const out = new Array(n);
    for (let i = 0; i < n; i++) out[i] = ((raw[i] << 24) | (raw[n + i] << 16) | (raw[2 * n + i] << 8) | raw[3 * n + i]) >>> 0; return out; }
  zint32s(n) { return this.words(n).map(v => ((v >>> 1) ^ -(v & 1)) | 0); }
  rfloats(n) { const buf = Buffer.alloc(4); return this.words(n).map(v => { const r = ((v >>> 1) | (v << 31)) >>> 0; buf.writeUInt32LE(r); return buf.readFloatLE(0); }); }
  refs(n) { const a = this.zint32s(n); for (let i = 1; i < n; i++) a[i] += a[i - 1]; return a; }
  zint64s(n) { const raw = this.bytes(n * 8); const out = [];
    for (let i = 0; i < n; i++) { let v = 0n; for (let j = 0; j < 8; j++) v = (v << 8n) | BigInt(raw[j * n + i]); out.push(((v >> 1n) ^ -(v & 1n)).toString()); } return out; }
}

function readChunks(buf, place) {
  let p = 14; // after signature
  const version = buf.readUInt16LE(p); p += 2;
  if (version !== 0) place.warnings.push(`unexpected binary format version ${version}`);
  const classCount = buf.readUInt32LE(p), instCount = buf.readUInt32LE(p + 4); p += 16;
  if (classCount > 100000 || instCount > 20000000) throw new Error('rbxl: implausible header counts');
  const chunks = [];
  while (p + 16 <= buf.length) {
    const name = buf.toString('latin1', p, p + 4).replace(/\0+$/, '');
    const clen = buf.readUInt32LE(p + 4), ulen = buf.readUInt32LE(p + 8); p += 16;
    if (ulen > MAX_CHUNK) throw new Error('rbxl: chunk too large');
    let data;
    if (clen === 0) { if (p + ulen > buf.length) throw new Error('rbxl: truncated chunk'); data = buf.subarray(p, p + ulen); p += ulen; }
    else { if (p + clen > buf.length) throw new Error('rbxl: truncated chunk'); data = lz4Decompress(buf.subarray(p, p + clen), ulen); p += clen; }
    chunks.push({ name, data });
    if (name === 'END') break;
  }
  if (!chunks.length || chunks[chunks.length - 1].name !== 'END') throw new Error('rbxl: missing END chunk (truncated file?)');
  return { version, classCount, instCount, chunks };
}

function readValues(r, typeId, n, ctx) {
  const out = new Array(n);
  switch (typeId) {
    case T.String: for (let i = 0; i < n; i++) out[i] = r.str(); return { type: 'String', values: out };
    case T.Bool: for (let i = 0; i < n; i++) out[i] = r.u8() !== 0; return { type: 'bool', values: out };
    case T.Int: return { type: 'int', values: r.zint32s(n) };
    case T.Float: return { type: 'float', values: r.rfloats(n) };
    case T.Double: for (let i = 0; i < n; i++) out[i] = r.f64(); return { type: 'double', values: out };
    case T.UDim: { const s = r.rfloats(n), o = r.zint32s(n); return { type: 'UDim', values: s.map((x, i) => ({ scale: x, offset: o[i] })) }; }
    case T.UDim2: { const sx = r.rfloats(n), sy = r.rfloats(n), ox = r.zint32s(n), oy = r.zint32s(n);
      return { type: 'UDim2', values: sx.map((x, i) => ({ xs: x, xo: ox[i], ys: sy[i], yo: oy[i] })) }; }
    case T.BrickColor: case T.Token: return { type: typeId === T.Token ? 'token' : 'BrickColor', values: r.words(n) };
    case T.Color3: { const a = r.rfloats(n), b = r.rfloats(n), c = r.rfloats(n); return { type: 'Color3', values: a.map((x, i) => ({ r: x, g: b[i], b: c[i] })) }; }
    case T.Vector2: { const a = r.rfloats(n), b = r.rfloats(n); return { type: 'Vector2', values: a.map((x, i) => ({ x, y: b[i] })) }; }
    case T.Vector3: { const a = r.rfloats(n), b = r.rfloats(n), c = r.rfloats(n); return { type: 'Vector3', values: a.map((x, i) => ({ x, y: b[i], z: c[i] })) }; }
    case T.Vector2int16: for (let i = 0; i < n; i++) out[i] = { x: r.i16(), y: r.i16() }; return { type: 'Vector2int16', values: out };
    case T.Vector3int16: for (let i = 0; i < n; i++) out[i] = { x: r.i16(), y: r.i16(), z: r.i16() }; return { type: 'Vector3int16', values: out };
    case T.CFrame: case T.CFrameQuat: {
      const rots = [];
      for (let i = 0; i < n; i++) {
        const id = r.u8();
        if (id === 0) {
          if (typeId === T.CFrame) { const m = []; for (let k = 0; k < 9; k++) m.push(r.f32()); rots.push(m); }
          else { const q = [r.f32(), r.f32(), r.f32(), r.f32()]; rots.push({ quat: q }); }
        } else { if (!ROT[id]) throw new Error('rbxl: unknown rotation id ' + id); rots.push(ROT[id]); }
      }
      const a = r.rfloats(n), b = r.rfloats(n), c = r.rfloats(n);
      return { type: 'CFrame', values: rots.map((m, i) => ({ pos: { x: a[i], y: b[i], z: c[i] }, rot: m })) };
    }
    case T.Reference: return { type: 'Reference', values: r.refs(n) };
    case T.Color3uint8: { const raw = r.bytes(n * 3); for (let i = 0; i < n; i++) out[i] = { r: raw[i], g: raw[n + i], b: raw[2 * n + i] }; return { type: 'Color3uint8', values: out }; }
    case T.Int64: return { type: 'int64', values: r.zint64s(n) };
    case T.SharedString: return { type: 'SharedString', values: r.words(n) };
    case T.NumberRange: for (let i = 0; i < n; i++) out[i] = { min: r.f32(), max: r.f32() }; return { type: 'NumberRange', values: out };
    case T.Rect: { const a = r.rfloats(n), b = r.rfloats(n), c = r.rfloats(n), d = r.rfloats(n); return { type: 'Rect', values: a.map((x, i) => ({ x0: x, y0: b[i], x1: c[i], y1: d[i] })) }; }
    case T.Faces: case T.Axes: for (let i = 0; i < n; i++) out[i] = { bits: r.u8() }; return { type: TNAME[typeId], values: out };
    case T.Ray: for (let i = 0; i < n; i++) out[i] = { o: [r.f32(), r.f32(), r.f32()], d: [r.f32(), r.f32(), r.f32()] }; return { type: 'Ray', values: out };
    case T.NumberSequence: for (let i = 0; i < n; i++) { const k = r.u32(); const kp = []; for (let j = 0; j < k; j++) kp.push({ t: r.f32(), v: r.f32(), e: r.f32() }); out[i] = kp; } return { type: 'NumberSequence', values: out };
    case T.ColorSequence: for (let i = 0; i < n; i++) { const k = r.u32(); const kp = []; for (let j = 0; j < k; j++) { const t = r.f32(); kp.push({ t, c: [r.f32(), r.f32(), r.f32()] }); r.f32(); } out[i] = kp; } return { type: 'ColorSequence', values: out };
    case T.PhysicalProperties: for (let i = 0; i < n; i++) { const c = r.u8(); out[i] = c ? { density: r.f32(), friction: r.f32(), elasticity: r.f32(), frictionWeight: r.f32(), elasticityWeight: r.f32() } : null; } return { type: 'PhysicalProperties', values: out };
    default: return null; // unsupported -> caller records warning
  }
}

function parseRbxl(buf) {
  if (buf.length < 32 || !buf.subarray(0, 14).equals(SIG)) throw new Error('rbxl: bad signature');
  const place = new Place(); place.format = 'rbxl-binary';
  const { version, classCount, instCount, chunks } = readChunks(buf, place);
  place.meta.binaryVersion = version; place.meta.headerClassCount = classCount; place.meta.headerInstanceCount = instCount;
  const classes = new Map(); const byId = new Map(); const sstr = [];
  for (const { name, data } of chunks) {
    const r = new Reader(data);
    if (name === 'META') { const n = r.u32(); for (let i = 0; i < n; i++) { const k = r.str().toString('utf8'); place.meta[k] = r.str().toString('utf8'); } }
    else if (name === 'SSTR') { r.u32(); const n = r.u32(); for (let i = 0; i < n; i++) { r.bytes(16); sstr.push(r.str()); } }
    else if (name === 'INST') {
      const id = r.i32(); const cname = r.str().toString('utf8'); const hasService = r.u8(); const n = r.u32(); const ids = r.refs(n);
      if (hasService) for (let i = 0; i < n; i++) r.u8();
      const insts = ids.map(rid => { const inst = new Instance(cname, rid); byId.set(rid, inst); place.byReferent.set(rid, inst); return inst; });
      classes.set(id, { name: cname, insts });
    } else if (name === 'PROP') {
      const cid = r.i32(); const pname = r.str().toString('utf8'); const cls = classes.get(cid);
      if (!cls) { place.warnings.push(`PROP for unknown class id ${cid}`); continue; }
      const tid = r.u8(); let res;
      try { res = readValues(r, tid, cls.insts.length); } catch (e) { place.warnings.push(`PROP ${cls.name}.${pname}: ${e.message}`); continue; }
      if (!res) { place.warnings.push(`unsupported property type 0x${tid.toString(16)} for ${cls.name}.${pname}`); continue; }
      cls.insts.forEach((inst, i) => {
        let v = res.values[i]; let type = res.type;
        if (type === 'String') { const s = v.toString('utf8'); v = s; type = /^(Source)$/.test(pname) ? 'ProtectedString' : (/(Id|Url|URL|Texture|Image|Mesh|Content|Sound)/.test(pname) && /(:\/\/|rbxasset|rbxassetid|^\d+$)/.test(s) ? 'Content' : 'string'); }
        else if (type === 'SharedString') { v = sstr[v] ? sstr[v].toString('latin1') : ''; type = 'SharedString'; }
        inst.props.set(pname, { type, value: v });
      });
    } else if (name === 'PRNT') {
      r.u8(); const n = r.u32(); const kids = r.refs(n), pars = r.refs(n);
      for (let i = 0; i < n; i++) { const c = byId.get(kids[i]); if (!c) continue; if (pars[i] === -1) place.roots.push(c); else { const p = byId.get(pars[i]); if (p) p.addChild(c); else place.warnings.push(`PRNT dangling parent ${pars[i]}`); } }
    }
  }
  return place;
}
module.exports = { parseRbxl, SIG, ROT };
