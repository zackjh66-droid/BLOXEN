'use strict';
// Minimal LZ4 *block* decompressor (https://github.com/lz4/lz4/blob/dev/doc/lz4_Block_format.md).
// Used for Roblox binary place (.rbxl) chunks. Pure data decoding; bounds-checked.
function lz4Decompress(src, uncompressedSize) {
  const dst = Buffer.allocUnsafe(uncompressedSize);
  let s = 0, d = 0;
  while (s < src.length) {
    const token = src[s++];
    let litLen = token >> 4;
    if (litLen === 15) { let b; do { if (s >= src.length) throw new Error('lz4: truncated literal length'); b = src[s++]; litLen += b; } while (b === 255); }
    if (s + litLen > src.length || d + litLen > dst.length) throw new Error('lz4: literal overrun');
    src.copy(dst, d, s, s + litLen); s += litLen; d += litLen;
    if (s >= src.length) break; // last sequence has no match
    if (s + 2 > src.length) throw new Error('lz4: truncated offset');
    const offset = src[s] | (src[s + 1] << 8); s += 2;
    if (offset === 0 || offset > d) throw new Error('lz4: bad offset');
    let matchLen = token & 15;
    if (matchLen === 15) { let b; do { if (s >= src.length) throw new Error('lz4: truncated match length'); b = src[s++]; matchLen += b; } while (b === 255); }
    matchLen += 4;
    if (d + matchLen > dst.length) throw new Error('lz4: match overrun');
    for (let i = 0; i < matchLen; i++, d++) dst[d] = dst[d - offset];
  }
  if (d !== uncompressedSize) throw new Error(`lz4: size mismatch ${d} != ${uncompressedSize}`);
  return dst;
}
// Test-support: trivial (literal-only) LZ4 block compressor so tests can build fixtures without deps.
function lz4CompressLiteralOnly(buf) {
  const out = [];
  let n = buf.length;
  const tok = Math.min(n, 15) << 4;
  out.push(tok);
  if (n >= 15) { n -= 15; while (n >= 255) { out.push(255); n -= 255; } out.push(n); }
  return Buffer.concat([Buffer.from(out), buf]);
}
module.exports = { lz4Decompress, lz4CompressLiteralOnly };
