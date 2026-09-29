'use strict';
const test = require('node:test'); const assert = require('node:assert/strict');
const { lz4Decompress, lz4CompressLiteralOnly } = require('../src/importer/lz4'); const { parsePlace } = require('../src/importer/parse'); const { parseRbxlx } = require('../src/importer/rbxlx');
const { SIG } = require('../src/importer/rbxl'); const { analyze } = require('../src/importer/analyze'); const { intakeFile } = require('../src/importer/intake');
const fs = require('fs'); const os = require('os'); const path = require('path'); const crypto = require('crypto');

// ---- binary fixture builder (mirrors RobloxAPI/spec formats/rbxl.md)
const u32 = n => { const b = Buffer.alloc(4); b.writeUInt32LE(n >>> 0); return b; };
const str = s => { const d = Buffer.from(s); return Buffer.concat([u32(d.length), d]); };
const zig = n => ((n << 1) ^ (n >> 31)) >>> 0;
function interleave(ints) { const n = ints.length; const out = Buffer.alloc(n * 4); ints.forEach((v, i) => { const x = zig(v); out[i] = (x >>> 24) & 255; out[n + i] = (x >>> 16) & 255; out[2 * n + i] = (x >>> 8) & 255; out[3 * n + i] = x & 255; }); return out; }
const delta = arr => arr.map((v, i) => v - (i ? arr[i - 1] : 0));
function chunk(name, data, compress = true) { const hdr = Buffer.alloc(16); hdr.write(name.padEnd(4, '\0'), 0, 'latin1'); if (compress) { const c = lz4CompressLiteralOnly(data); hdr.writeUInt32LE(c.length, 4); hdr.writeUInt32LE(data.length, 8); return Buffer.concat([hdr, c]); } hdr.writeUInt32LE(0, 4); hdr.writeUInt32LE(data.length, 8); return Buffer.concat([hdr, data]); }
function buildPlace({ compress = true, source = 'print("hi")' } = {}) {
  const head = Buffer.concat([SIG, Buffer.from([0, 0]), u32(2), u32(3), Buffer.alloc(8)]);
  const inst = (id, name, refs) => chunk('INST', Buffer.concat([u32(id), str(name), Buffer.from([0]), u32(refs.length), interleave(delta(refs))]), compress);
  const prop = (id, name, type, body) => chunk('PROP', Buffer.concat([u32(id), str(name), Buffer.from([type]), body]), compress);
  const parts = [head, inst(0, 'Workspace', [0]), inst(1, 'Script', [1, 2]),
    prop(0, 'Name', 1, str('Workspace')), prop(1, 'Name', 1, Buffer.concat([str('A'), str('B')])), prop(1, 'Source', 1, Buffer.concat([str(source), str('x = 1')])),
    chunk('PRNT', Buffer.concat([Buffer.from([0]), u32(3), interleave(delta([0, 1, 2])), interleave(delta([-1, 0, 0]))]), compress), chunk('END', Buffer.from('</roblox>'), false)];
  return Buffer.concat(parts);
}
test('lz4: literal-only round trip and malformed input', () => {
  const data = crypto.randomBytes(3000); assert.deepEqual(lz4Decompress(lz4CompressLiteralOnly(data), data.length), data);
  assert.throws(() => lz4Decompress(Buffer.from([0xf0, 0xff]), 100));
});
test('lz4: back-reference (overlapping match) decodes', () => { // token: 1 literal, match len 4+? ; literal 'a', offset 1, matchlen 8
  const comp = Buffer.from([0x14, 0x61, 0x01, 0x00]); assert.equal(lz4Decompress(comp, 9).toString(), 'aaaaaaaaa');
});
for (const compress of [true, false]) test(`rbxl binary fixture parses (${compress ? 'lz4' : 'raw'} chunks)`, () => {
  const place = parsePlace(buildPlace({ compress })); assert.equal(place.format, 'rbxl-binary'); assert.equal(place.count(), 3);
  const ws = place.roots[0]; assert.equal(ws.className, 'Workspace'); assert.deepEqual(ws.children.map(c => c.name), ['A', 'B']);
  assert.equal(ws.children[0].props.get('Source').value, 'print("hi")');
});
test('rbxl: truncated and corrupt files throw, never hang', () => {
  const good = buildPlace(); for (const n of [5, 20, 40, 100, good.length - 20]) assert.throws(() => parsePlace(good.subarray(0, n)), Error, 'cut at ' + n);
  const huge = Buffer.from(good); huge.writeUInt32LE(0xfffffff0, 32 + 8); assert.throws(() => parsePlace(huge), /too large|truncated|overrun|implausible/);
});
test('rbxlx: tolerant XML, no entity expansion', () => {
  const xml = `<roblox><Item class="Workspace" referent="RBX0"><Properties><string name="Name">Workspace</string></Properties><Item class="Script" referent="RBX1"><Properties><string name="Name">S</string><ProtectedString name="Source"><![CDATA[print("&lt;")]]></ProtectedString></Properties></Item></Item></roblox>`;
  const p = parseRbxlx(Buffer.from(xml)); assert.equal(p.count(), 2); assert.equal(p.roots[0].children[0].props.get('Source').value, 'print("&lt;")');
  const bomb = `<!DOCTYPE r [<!ENTITY a "aaaa"><!ENTITY b "&a;&a;&a;&a;">]><roblox><Item class="Part" referent="RBX0"><Properties><string name="Name">&b;</string></Properties></Item></roblox>`;
  const q = parseRbxlx(Buffer.from(bomb)); assert.ok(q.roots[0].props.get('Name').value.length < 50, 'entities are not expanded');
});
test('parsePlace: content sniffing, garbage rejected', () => { assert.throws(() => parsePlace(Buffer.from('not a place'))); assert.throws(() => parsePlace(Buffer.alloc(0))); });
test('analyze: scripts are inventoried as inert data with markers, and asset ids extracted', () => {
  const src = 'local http = game:GetService("HttpService")\nrequire(123456)\nInsertService:LoadAsset(987654)'; const r = analyze(parsePlace(buildPlace({ source: src })));
  assert.equal(r.scriptSummary.total, 2); assert.ok(r.scripts[0].sha256.length === 64); assert.ok(Object.keys(r.scriptSummary.markerCounts).length >= 1, JSON.stringify(r.scriptSummary));
});
test('intake: hashes, stores read-only content-addressed copy, never executes', () => {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-')); const f = path.join(dir, 'x.rbxl'); const buf = buildPlace(); fs.writeFileSync(f, buf);
  const { report } = intakeFile(f, { store: path.join(dir, 'store') }); assert.equal(report.sha256, crypto.createHash('sha256').update(buf).digest('hex')); assert.equal(report.format, 'rbxl-binary');
  const stored = path.join(dir, 'store', report.sha256 + '.rbx'); assert.ok(fs.existsSync(stored)); assert.equal(fs.statSync(stored).mode & 0o222, 0, 'store copy is read-only');
});
