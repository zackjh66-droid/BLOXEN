'use strict';
// Port of RakNet DS_HuffmanEncodingTree.cpp (BSD, Oculus VR) + StringCompressor semantics, with the LEGACY length prefix
// observed from the real 0.205.0.61876 client: [bit 0][u32 network-order bit length][huffman bits].
const fs = require('fs'); const path = require('path');
const { BitStream } = require('./bitstream');
const FREQ = JSON.parse(fs.readFileSync(path.join(__dirname, '../../preservation/reference/raknet-english-freq.json'), 'utf8')).freq;

function buildTree(freq) {
  const leaves = []; let list = [];
  const insert = (node) => { let i = 0; while (i < list.length && list[i].weight < node.weight) i++; list.splice(i, 0, node); }; // before first node with weight >= new
  for (let c = 0; c < 256; c++) { const n = { left: null, right: null, parent: null, value: c, weight: freq[c] || 1 }; leaves.push(n); insert(n); }
  let root;
  for (;;) {
    const lesser = list.shift(), greater = list.shift();
    const node = { left: lesser, right: greater, parent: null, weight: lesser.weight + greater.weight, value: -1 };
    lesser.parent = node; greater.parent = node;
    if (list.length === 0) { root = node; break; }
    insert(node);
  }
  const table = leaves.map(leaf => { const path = []; let cur = leaf; do { path.push(cur.parent.left === cur ? 0 : 1); cur = cur.parent; } while (cur !== root); path.reverse(); return path; });
  return { root, table };
}
const TREE = buildTree(FREQ);

function encodeBits(buf) { const bs = new BitStream(); for (const b of buf) for (const bit of TREE.table[b]) bs.writeBit(bit); return bs; }
// Legacy string encoding: 0 bit, u32 BE bit length, code bits. (Empty string => length 0, no bits.)
function writeLegacyString(bs, str) {
  const data = Buffer.isBuffer(str) ? str : Buffer.from(String(str), 'utf8');
  const enc = encodeBits(data);
  bs.writeBit(0); bs.writeU32(enc.length);
  for (let i = 0; i < enc.length; i++) bs.writeBit(enc.buf[i >> 3] & (0x80 >> (i & 7)) ? 1 : 0);
  return bs;
}
function readLegacyString(bs, maxLen = 1 << 20) {
  if (bs.readBit() !== 0) throw new Error('legacy string: expected leading zero bit');
  const nbits = bs.readU32(); if (nbits > bs.unread) throw new RangeError('legacy string: bit length exceeds stream');
  const out = []; let node = TREE.root;
  for (let i = 0; i < nbits; i++) { node = bs.readBit() ? node.right : node.left; if (!node.left && !node.right) { if (out.length >= maxLen) throw new RangeError('legacy string too long'); out.push(node.value); node = TREE.root; } }
  return Buffer.from(out);
}
module.exports = { writeLegacyString, readLegacyString, TREE, buildTree };
