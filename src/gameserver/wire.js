'use strict';
// Value encoders for replicated properties. STATUS: UNIT-TESTED round trips only. Every encoding here is INFERRED from RakNet conventions
// and client strings ("BitStream >> BrickColor failed", "CoordinateFrame orientId failed"); none are verified against the real client.
const { writeLegacyString, readLegacyString } = require('./huffman');
const { ROT } = require('../importer/rbxl');
const ROT_KEYS = Object.entries(ROT).map(([id, m]) => [Number(id), m]);
function orientId(m) { for (const [id, r] of ROT_KEYS) { let ok = true; for (let i = 0; i < 9; i++) if (Math.abs(r[i] - m[i]) > 1e-4) { ok = false; break; } if (ok) return id; } return 0; }
const IDENT = [1, 0, 0, 0, 1, 0, 0, 0, 1];
const SUPPORTED = new Set(['bool', 'int', 'float', 'double', 'string', 'Content', 'ProtectedString', 'BinaryString', 'BrickColor', 'Color3', 'Vector3', 'Vector2', 'CFrame', 'CoordinateFrame', 'UDim2', 'Vector2int16', 'Object', 'enum']);
const num = (v, d = 0) => (typeof v === 'number' && Number.isFinite(v)) ? v : d;
function write(bs, kind, v, ctx = {}) {
  switch (kind) {
    case 'bool': bs.writeBool(!!v); break;
    case 'int': bs.writeI32(num(v) | 0); break;
    case 'float': bs.writeF32(num(v)); break;
    case 'double': bs.writeF64(num(v)); break;
    case 'string': case 'Content': case 'ProtectedString': case 'BinaryString': writeLegacyString(bs, v == null ? '' : v); break;
    case 'BrickColor': bs.writeU16(num(v, 194) & 0xffff); break;
    case 'Color3': bs.writeF32(num(v?.r)); bs.writeF32(num(v?.g)); bs.writeF32(num(v?.b)); break;
    case 'Vector3': bs.writeF32(num(v?.x)); bs.writeF32(num(v?.y)); bs.writeF32(num(v?.z)); break;
    case 'Vector2': bs.writeF32(num(v?.x)); bs.writeF32(num(v?.y)); break;
    case 'Vector2int16': bs.writeU16(num(v?.x) & 0xffff); bs.writeU16(num(v?.y) & 0xffff); break;
    case 'UDim2': bs.writeF32(num(v?.xs)); bs.writeI32(num(v?.xo) | 0); bs.writeF32(num(v?.ys)); bs.writeI32(num(v?.yo) | 0); break;
    case 'CFrame': case 'CoordinateFrame': {
      bs.writeF32(num(v?.pos?.x)); bs.writeF32(num(v?.pos?.y)); bs.writeF32(num(v?.pos?.z));
      const m = Array.isArray(v?.rot) ? v.rot : IDENT; const id = orientId(m); bs.writeU8(id);
      if (id === 0) for (let i = 0; i < 9; i++) bs.writeF32(num(m[i]));
      break; }
    case 'Object': bs.writeCompressed(typeof v === 'number' && v > 0 ? v : 0, 4); break; // world instance id, 0 = nil
    case 'enum': bs.writeCompressed(num(v) >>> 0, 4); break;
    default: throw new Error('wire: unsupported kind ' + kind);
  }
}
function read(bs, kind, ctx = {}) {
  switch (kind) {
    case 'bool': return bs.readBool(); case 'int': return bs.readI32(); case 'float': return bs.readF32(); case 'double': return bs.readF64();
    case 'string': case 'Content': case 'ProtectedString': case 'BinaryString': return readLegacyString(bs).toString('utf8');
    case 'BrickColor': return bs.readU16();
    case 'Color3': return { r: bs.readF32(), g: bs.readF32(), b: bs.readF32() };
    case 'Vector3': return { x: bs.readF32(), y: bs.readF32(), z: bs.readF32() };
    case 'Vector2': return { x: bs.readF32(), y: bs.readF32() };
    case 'Vector2int16': { const x = bs.readU16(), y = bs.readU16(); return { x: x << 16 >> 16, y: y << 16 >> 16 }; }
    case 'UDim2': return { xs: bs.readF32(), xo: bs.readI32(), ys: bs.readF32(), yo: bs.readI32() };
    case 'CFrame': case 'CoordinateFrame': { const pos = { x: bs.readF32(), y: bs.readF32(), z: bs.readF32() }; const id = bs.readU8(); let rot;
      if (id === 0) { rot = []; for (let i = 0; i < 9; i++) rot.push(bs.readF32()); } else { if (!ROT[id]) throw new Error('CoordinateFrame orientId failed'); rot = ROT[id].slice(); } return { pos, rot }; }
    case 'Object': { const id = bs.readCompressed(4); return id || null; }
    case 'enum': return bs.readCompressed(4);
    default: throw new Error('wire: unsupported kind ' + kind);
  }
}
// Coerce an importer value ({type,value}) to the schema kind. Returns undefined when not representable.
function coerce(kind, pv) {
  if (!pv) return undefined; const v = pv.value;
  switch (kind) {
    case 'bool': return typeof v === 'boolean' ? v : undefined;
    case 'int': case 'BrickColor': case 'enum': return (typeof v === 'number') ? v : undefined;
    case 'float': case 'double': return typeof v === 'number' ? v : undefined;
    case 'string': case 'Content': case 'ProtectedString': case 'BinaryString': return typeof v === 'string' ? v : undefined;
    case 'Color3': return v && typeof v.r === 'number' ? v : undefined;
    case 'Vector3': case 'Vector2': case 'Vector2int16': return v && typeof v.x === 'number' ? v : undefined;
    case 'UDim2': return v && 'xs' in v ? v : undefined;
    case 'CFrame': case 'CoordinateFrame': return v && v.pos && Array.isArray(v.rot) ? v : undefined;
    case 'Object': return typeof v === 'string' ? v : undefined; // referent, resolved by world
    default: return undefined;
  }
}
module.exports = { write, read, coerce, SUPPORTED, orientId };
