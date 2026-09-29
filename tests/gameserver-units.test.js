'use strict';
const test = require('node:test'); const assert = require('node:assert/strict');
const { BitStream } = require('../src/gameserver/bitstream'); const { writeLegacyString, readLegacyString } = require('../src/gameserver/huffman');
const wire = require('../src/gameserver/wire'); const schema = require('../src/gameserver/schema'); const { Session, REL } = require('../src/gameserver/raknet');

test('bitstream: MSB-first bits and big-endian ints', () => {
  const b = new BitStream(); b.writeBit(1); b.writeBit(0); b.writeBit(1); b.writeU32(0x01020304); const buf = b.toBuffer();
  assert.equal(buf[0] >> 5, 0b101); const r = new BitStream(buf); assert.equal(r.readBit(), 1); assert.equal(r.readBit(), 0); assert.equal(r.readBit(), 1); assert.equal(r.readU32(), 0x01020304);
});
test('bitstream: compressed ints round trip incl. boundaries', () => {
  for (const v of [0, 1, 255, 256, 65535, 70000, 0xffffffff]) { const b = new BitStream(); b.writeCompressed(v, 4); assert.equal(new BitStream(b.toBuffer()).readCompressed(4), v); }
});
test('bitstream: read past end throws', () => { assert.throws(() => new BitStream(Buffer.alloc(1)).readU32(), RangeError); });
test('huffman: legacy string layout and round trip', () => {
  for (const s of ['Workspace', '', 'héllo ✓', 'x'.repeat(2000)]) { const b = new BitStream(); writeLegacyString(b, s); const buf = b.toBuffer(); assert.equal(buf[0] & 0x80, 0, 'leading zero bit'); assert.equal(new BitStream(buf).readBit(), 0); assert.equal(readLegacyString(new BitStream(buf)).toString(), s); }
  const b = new BitStream(); writeLegacyString(b, 'e'); const r = new BitStream(b.toBuffer()); r.readBit(); assert.equal(r.readU32(), 3, "'e' has a 3-bit code in the RakNet English tree");
});
test('huffman: hostile length prefix is rejected', () => { const b = new BitStream(); b.writeBit(0); b.writeU32(0xffffffff); assert.throws(() => readLegacyString(new BitStream(b.toBuffer()))); });
test('huffman: nonzero leading bit rejected', () => { const b = new BitStream(); b.writeBit(1); b.writeU32(0); assert.throws(() => readLegacyString(new BitStream(b.toBuffer()))); });
test('schema: 332 classes, ReplicatedFirst id 231 (matches real-client observation)', () => {
  const s = schema.load(); assert.equal(s.names.length, 332); assert.equal(s.classId.get('ReplicatedFirst'), 231); const r = schema.report(s); assert.ok(r.replicatedFirstMatchesClient);
  assert.equal(r.gaps.classes.match, true); assert.equal(r.gaps.properties.match, false, 'known gap: dump has fewer properties than the client (851 in the dump vs 968 in the client)');
});
test('schema: Part exposes the serialization properties used by place files', () => { const c = schema.load().classes.get('Part'); for (const n of ['size', 'CFrame', 'BrickColor', 'Anchored', 'Friction']) assert.ok(c.byName.has(n), n); assert.ok(c.byName.get('size').inferred); });
test('wire: every supported kind round trips', () => {
  const cases = [['bool', true], ['int', -7], ['float', 1.5], ['double', 2.25], ['string', 'a b'], ['BrickColor', 194], ['Color3', { r: 0.5, g: 0.25, b: 1 }], ['Vector3', { x: 1, y: 2, z: 3 }], ['Vector2', { x: 1, y: 2 }], ['UDim2', { xs: 0.5, xo: 3, ys: 0, yo: -4 }], ['Vector2int16', { x: -3, y: 7 }], ['enum', 5], ['Object', null], ['CFrame', { pos: { x: 1, y: 2, z: 3 }, rot: [1, 0, 0, 0, 0, -1, 0, 1, 0] }]];
  const b = new BitStream(); for (const [k, v] of cases) wire.write(b, k, v === null ? 0 : v); const r = new BitStream(b.toBuffer());
  for (const [k, v] of cases) { const got = wire.read(r, k); assert.deepEqual(got, v, k); }
});
test('wire: non-axis-aligned CFrame is preserved as explicit matrix', () => {
  const cf = { pos: { x: 0, y: 0, z: 0 }, rot: [0.5, 0, 0.5, 0, 1, 0, -0.5, 0, 0.5] }; const b = new BitStream(); wire.write(b, 'CFrame', cf); const got = wire.read(new BitStream(b.toBuffer()), 'CFrame'); got.rot.forEach((x, i) => assert.ok(Math.abs(x - cf.rot[i]) < 1e-6));
});
test('wire: bad orientId is rejected', () => { const b = new BitStream(); for (let i = 0; i < 3; i++) b.writeF32(0); b.writeU8(0x99); assert.throws(() => wire.read(new BitStream(b.toBuffer()), 'CFrame'), /orientId/); });

// lossy / reordering link between two Sessions
function link({ loss = 0.3, seed = 7 } = {}) {
  let s = seed; const rnd = () => (s = (s * 1664525 + 1013904223) % 4294967296) / 4294967296; let a, b; const q = [];
  const mk = other => buf => { if (rnd() < loss) return; q.push(() => other().receive(Buffer.from(buf))); if (rnd() < 0.3 && q.length > 1) q.push(q.shift()); };
  a = new Session(mk(() => b)); b = new Session(mk(() => a)); const pump = setInterval(() => { while (q.length) q.shift()(); }, 5); return { a, b, stop: () => { clearInterval(pump); a.close(); b.close(); } };
}
test('raknet: reliable ordered delivery over a lossy, reordering link (incl. split packets)', async () => {
  const { a, b, stop } = link(); const got = []; b.on('packet', p => got.push(p.length > 10 ? 'big' + p.length : p[1]));
  for (let i = 0; i < 20; i++) a.sendPacket(Buffer.from([0x83, i])); a.sendPacket(Buffer.alloc(5000, 1)); for (let i = 20; i < 30; i++) a.sendPacket(Buffer.from([0x83, i]));
  const t0 = Date.now(); while (got.length < 31 && Date.now() - t0 < 12000) await new Promise(r => setTimeout(r, 25)); stop();
  assert.deepEqual(got, [...Array(20).keys(), 'big5000', ...Array.from({ length: 10 }, (_, i) => 20 + i)]);
});
test('raknet: duplicate datagrams are not delivered twice', () => {
  const a = new Session(() => {}); const out = []; a.on('packet', p => out.push(p)); const sender = new Session(buf => { a.receive(buf); a.receive(buf); }); sender.sendPacket(Buffer.from([0x83, 1])); sender.close(); a.close(); assert.equal(out.length, 1);
});
test('raknet: malformed datagram never throws past receive boundaries', () => {
  const a = new Session(() => {}); for (const junk of [Buffer.from([0x84, 0, 0, 0, 0x60, 0xff, 0xff]), Buffer.from([0xc0, 0xff, 0xff]), Buffer.from([0x84, 0, 0, 0, 0x70, 0, 8, 1, 2, 3])]) { try { a.receive(junk); } catch (e) { assert.ok(e instanceof RangeError); } } a.close();
});
