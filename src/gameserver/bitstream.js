'use strict';
// RakNet-style BitStream: bits are written MSB-first within each byte; multi-byte integers are big-endian ("network order").
// Semantics follow RakNet BitStream.cpp (BSD). Used by the BLOXEN protocol layers; see research/PROTOCOL.md for verification status.
class BitStream {
  constructor(buf) { this.buf = buf ? Buffer.from(buf) : Buffer.alloc(64); this.bitsUsed = buf ? buf.length * 8 : 0; this.readPos = 0; }
  _ensure(bits) { const need = (bits + 7) >> 3; if (need > this.buf.length) { const nb = Buffer.alloc(Math.max(need, this.buf.length * 2)); this.buf.copy(nb); this.buf = nb; } }
  get length() { return this.bitsUsed; }
  get unread() { return this.bitsUsed - this.readPos; }
  toBuffer() { return Buffer.from(this.buf.subarray(0, (this.bitsUsed + 7) >> 3)); }
  // ---- bit level
  writeBit(b) { this._ensure(this.bitsUsed + 1); const i = this.bitsUsed >> 3, m = 0x80 >> (this.bitsUsed & 7); if (b) this.buf[i] |= m; else this.buf[i] &= ~m; this.bitsUsed++; return this; }
  readBit() { if (this.readPos >= this.bitsUsed) throw new RangeError('BitStream: read past end'); const i = this.readPos >> 3, m = 0x80 >> (this.readPos & 7); this.readPos++; return (this.buf[i] & m) ? 1 : 0; }
  // Write n bits taken from `data` (Buffer). rightAligned: for the final partial byte use its LOW bits (how integers are stored); else its HIGH bits (Huffman codes).
  writeBits(data, n, rightAligned = true) {
    const full = n >> 3, rem = n & 7;
    for (let i = 0; i < full; i++) for (let k = 7; k >= 0; k--) this.writeBit((data[i] >> k) & 1);
    if (rem) { const byte = data[full]; for (let k = 0; k < rem; k++) { const bit = rightAligned ? (byte >> (rem - 1 - k)) & 1 : (byte >> (7 - k)) & 1; this.writeBit(bit); } }
    return this;
  }
  readBits(n) { const out = Buffer.alloc((n + 7) >> 3); for (let i = 0; i < n; i++) if (this.readBit()) out[i >> 3] |= 0x80 >> (i & 7); return out; } // left-aligned
  // ---- aligned / integer helpers (big-endian)
  writeUInt(v, bytes) { let x = BigInt(v); for (let i = bytes - 1; i >= 0; i--) { const byte = Number((x >> BigInt(8 * i)) & 0xffn); for (let k = 7; k >= 0; k--) this.writeBit((byte >> k) & 1); } return this; }
  readUInt(bytes) { let x = 0n; for (let i = 0; i < bytes; i++) { let b = 0; for (let k = 0; k < 8; k++) b = (b << 1) | this.readBit(); x = (x << 8n) | BigInt(b); } return bytes <= 6 ? Number(x) : x; }
  writeU8(v) { return this.writeUInt(v, 1); } readU8() { return this.readUInt(1); }
  writeU16(v) { return this.writeUInt(v, 2); } readU16() { return this.readUInt(2); }
  writeU32(v) { return this.writeUInt(v >>> 0, 4); } readU32() { return this.readUInt(4); }
  writeI32(v) { return this.writeUInt(v >>> 0, 4); } readI32() { return this.readUInt(4) | 0; }
  writeF32(v) { const b = Buffer.alloc(4); b.writeFloatBE(v); return this.writeBytes(b); } readF32() { return this.readBytes(4).readFloatBE(0); }
  writeF64(v) { const b = Buffer.alloc(8); b.writeDoubleBE(v); return this.writeBytes(b); } readF64() { return this.readBytes(8).readDoubleBE(0); }
  writeBool(v) { return this.writeBit(v ? 1 : 0); } readBool() { return !!this.readBit(); }
  writeBytes(buf) { for (const byte of buf) for (let k = 7; k >= 0; k--) this.writeBit((byte >> k) & 1); return this; }
  readBytes(n) { const out = Buffer.alloc(n); for (let i = 0; i < n; i++) { let b = 0; for (let k = 0; k < 8; k++) b = (b << 1) | this.readBit(); out[i] = b; } return out; }
  alignWrite() { const r = this.bitsUsed & 7; if (r) { this._ensure(this.bitsUsed + 8 - r); this.bitsUsed += 8 - r; } return this; }
  alignRead() { const r = this.readPos & 7; if (r) this.readPos += 8 - r; return this; }
  // ---- RakNet "compressed" unsigned ints: leading zero-bytes (from the high end) collapse to single 1-bits.
  writeCompressed(v, bytes) { const b = Buffer.alloc(bytes); let x = BigInt(v); for (let i = bytes - 1; i >= 0; i--) { b[i] = Number(x & 0xffn); x >>= 8n; } // b big-endian
    let cur = 0; while (cur < bytes - 1) { if (b[cur] === 0) { this.writeBit(1); cur++; } else { this.writeBit(0); this.writeBytes(b.subarray(cur)); return this; } }
    if (b[cur] === 0) this.writeBit(1); else { this.writeBit(0); this.writeBytes(b.subarray(cur)); } return this; }
  readCompressed(bytes) { const b = Buffer.alloc(bytes); let cur = 0; while (cur < bytes - 1) { if (this.readBit()) { b[cur++] = 0; } else { this.readBytes(bytes - cur).copy(b, cur); return bytes <= 6 ? b.readUIntBE(0, bytes) : b.readBigUInt64BE(0); } }
    if (this.readBit()) b[cur] = 0; else b[cur] = this.readBytes(1)[0]; return bytes <= 6 ? b.readUIntBE(0, bytes) : b.readBigUInt64BE(0); }
}
module.exports = { BitStream };
