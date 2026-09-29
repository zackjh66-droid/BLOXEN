'use strict';
// BLOXEN game-server message profile "bloxen-profile-v0".
// EVERY packet id, field order and width below is INFERRED/UNVERIFIED except the items listed in OBSERVED. The profile exists so the stack can be
// driven end-to-end by the simulator and then re-aligned with a capture from the genuine client (docs/WINDOWS-REAL-CLIENT-VALIDATION.md).
const { BitStream } = require('./bitstream'); const { writeLegacyString, readLegacyString } = require('./huffman'); const wire = require('./wire');

const PROTOCOL_VERSION = 31; // OBSERVED: the real client reached protocol 31
const PKT = Object.freeze({ PROTOCOL_SYNC: 0x80, SET_GLOBALS: 0x81, TEACH_DESCRIPTOR_DICTIONARIES: 0x82, ID_DATA: 0x83, PROTOCOL_MISMATCH: 0x85, JOIN_REFUSED: 0x86 }); // values UNVERIFIED
const OP = Object.freeze({ END: 0, NEW: 1, PROP: 2, DEL: 3, CLIENT_READY: 0x10, MOVE: 0x11, RESET: 0x12, LOCAL_PLAYER: 0x21 });
const GLOBALS_PREAMBLE_BITS = 121; // OBSERVED length (Workspace preamble before the container count). CONTENT is UNKNOWN; we write zero bits.
const OBSERVED = Object.freeze({ protocol: 31, preambleBits: 121, topContainers: 22, firstContainerClass: 231, legacyStringFormat: 'bit0+u32be-bitlen+huffman' });

function start(id) { const bs = new BitStream(); bs.writeU8(id); return bs; }
function encodeProtocolSync(ticket, version = PROTOCOL_VERSION) { const bs = start(PKT.PROTOCOL_SYNC); bs.writeU32(version); writeLegacyString(bs, ticket); return bs.toBuffer(); }
function decodeProtocolSync(buf) { const bs = new BitStream(buf); if (bs.readU8() !== PKT.PROTOCOL_SYNC) throw new Error('not PROTOCOL_SYNC'); const version = bs.readU32(); const ticket = readLegacyString(bs, 4096).toString('utf8'); return { version, ticket }; }
function encodeMismatch(serverVersion = PROTOCOL_VERSION) { const bs = start(PKT.PROTOCOL_MISMATCH); bs.writeU32(serverVersion); writeLegacyString(bs, 'Network protocol mismatch. Please upgrade.'); return bs.toBuffer(); }
function encodeRefused(reason) { const bs = start(PKT.JOIN_REFUSED); writeLegacyString(bs, reason); return bs.toBuffer(); }

function encodeDescriptors(schema) {
  const bs = start(PKT.TEACH_DESCRIPTOR_DICTIONARIES);
  bs.writeU32(schema.names.length); for (const n of schema.names) writeLegacyString(bs, n);
  for (const n of schema.names) { const c = schema.classes.get(n); bs.writeU32(c.props.length); for (const p of c.props) { writeLegacyString(bs, p.name); writeLegacyString(bs, p.type); bs.writeU8(wire.SUPPORTED.has(p.kind) ? 1 : 0); } }
  bs.writeU32(0); // events: names are not available in the public dump (only counts) -> UNKNOWN, none sent
  return bs.toBuffer();
}
function decodeDescriptors(buf) {
  const bs = new BitStream(buf); if (bs.readU8() !== PKT.TEACH_DESCRIPTOR_DICTIONARIES) throw new Error('bad descriptor packet');
  const n = bs.readU32(); if (n > 5000) throw new Error('bad class count'); const names = []; for (let i = 0; i < n; i++) names.push(readLegacyString(bs, 256).toString());
  const classes = names.map(() => []); for (let i = 0; i < n; i++) { const k = bs.readU32(); if (k > 5000) throw new Error('bad prop count'); for (let j = 0; j < k; j++) classes[i].push({ name: readLegacyString(bs, 256).toString(), type: readLegacyString(bs, 256).toString(), supported: bs.readU8() === 1 }); }
  return { names, classes, events: bs.readU32() };
}

function encodeGlobals(world, extra = {}) {
  const bs = start(PKT.SET_GLOBALS); for (let i = 0; i < GLOBALS_PREAMBLE_BITS; i++) bs.writeBit(0);
  const tops = world.topContainers(); bs.writeCompressed(tops.length, 4);
  for (const t of tops) { bs.writeU16(world.schema.classId.get(t.className)); bs.writeCompressed(t.id, 4); writeLegacyString(bs, t.name); }
  return bs.toBuffer();
}
function decodeGlobals(buf, schema) {
  const bs = new BitStream(buf); if (bs.readU8() !== PKT.SET_GLOBALS) throw new Error('bad globals packet'); bs.readBits(GLOBALS_PREAMBLE_BITS);
  const n = bs.readCompressed(4); if (n > 1000) throw new Error('bad container count'); const out = [];
  for (let i = 0; i < n; i++) { const classId = bs.readU16(); const id = bs.readCompressed(4); const name = readLegacyString(bs, 256).toString(); out.push({ classId, className: schema.names[classId], id, name }); }
  return out;
}

// ---- ID_DATA batches
function newBatch() { return start(PKT.ID_DATA); }
function writeNew(bs, world, inst) {
  const cls = world.schema.classes.get(inst.className); bs.writeU8(OP.NEW); bs.writeU16(cls.id); bs.writeCompressed(inst.id, 4); bs.writeCompressed(inst.parent ? inst.parent.id : 0, 4);
  const list = []; for (const [k, v] of inst.props) { const pd = cls.byName.get(k); if (pd && wire.SUPPORTED.has(pd.kind)) list.push([pd, v]); }
  bs.writeCompressed(list.length, 2); for (const [pd, v] of list) { bs.writeU16(pd.id); wire.write(bs, pd.kind, v); }
}
function writeProp(bs, world, inst, name) {
  const cls = world.schema.classes.get(inst.className); const pd = cls.byName.get(name); if (!pd || !wire.SUPPORTED.has(pd.kind)) return false;
  bs.writeU8(OP.PROP); bs.writeCompressed(inst.id, 4); bs.writeU16(pd.id); wire.write(bs, pd.kind, inst.props.get(name)); return true;
}
function writeDel(bs, id) { bs.writeU8(OP.DEL); bs.writeCompressed(id, 4); }
function endBatch(bs) { bs.writeU8(OP.END); return bs.toBuffer(); }
function encodeMove(id, cf) { const bs = newBatch(); bs.writeU8(OP.MOVE); bs.writeCompressed(id, 4); wire.write(bs, 'CFrame', cf); return endBatch(bs); }
function encodeSimple(op, id) { const bs = newBatch(); bs.writeU8(op); if (id !== undefined) bs.writeCompressed(id, 4); return endBatch(bs); }

// Parse any ID_DATA batch. `lookup(classId)` → {name, byId:Map(propId→{name,kind})}; `kindOfInstance(id)` → class info for PROP ops.
function decodeBatch(buf, schema, getInstanceClass) {
  const bs = new BitStream(buf); if (bs.readU8() !== PKT.ID_DATA) throw new Error('not ID_DATA'); const ops = []; const local = new Map();
  for (let guard = 0; guard < 100000; guard++) {
    const op = bs.readU8(); if (op === OP.END) return ops;
    switch (op) {
      case OP.NEW: { const classId = bs.readU16(); const className = schema.names[classId]; const cls = schema.classes.get(className); if (!cls) throw new Error('unknown class id ' + classId);
        const id = bs.readCompressed(4), parent = bs.readCompressed(4); const n = bs.readCompressed(2); const props = {};
        for (let i = 0; i < n; i++) { const pd = cls.props[bs.readU16()]; if (!pd) throw new Error('unknown property id'); props[pd.name] = wire.read(bs, pd.kind); }
        local.set(id, className); ops.push({ op: 'new', className, id, parent, props }); break; }
      case OP.PROP: { const id = bs.readCompressed(4); const cn = local.get(id) || getInstanceClass(id); const cls = cn && schema.classes.get(cn); if (!cls) throw new Error('prop for unknown instance'); const pd = cls.props[bs.readU16()]; if (!pd) throw new Error('unknown property id');
        ops.push({ op: 'prop', id, name: pd.name, value: wire.read(bs, pd.kind) }); break; }
      case OP.DEL: ops.push({ op: 'del', id: bs.readCompressed(4) }); break;
      case OP.CLIENT_READY: ops.push({ op: 'ready' }); break;
      case OP.RESET: ops.push({ op: 'reset' }); break;
      case OP.MOVE: { const id = bs.readCompressed(4); ops.push({ op: 'move', id, cf: wire.read(bs, 'CFrame') }); break; }
      case OP.LOCAL_PLAYER: ops.push({ op: 'localPlayer', id: bs.readCompressed(4) }); break;
      default: throw new Error('unknown ID_DATA op ' + op);
    }
  }
  throw new Error('batch not terminated');
}
module.exports = { PROTOCOL_VERSION, PKT, OP, OBSERVED, GLOBALS_PREAMBLE_BITS, encodeProtocolSync, decodeProtocolSync, encodeMismatch, encodeRefused, encodeDescriptors, decodeDescriptors, encodeGlobals, decodeGlobals,
  newBatch, writeNew, writeProp, writeDel, endBatch, encodeMove, encodeSimple, decodeBatch };
