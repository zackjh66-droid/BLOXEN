'use strict';
const { parseRbxl, SIG } = require('./rbxl');
const { parseRbxlx } = require('./rbxlx');
// Sniff by content (extension is unreliable: archived ".rbxl" files are often XML).
function parsePlace(buf) {
  if (buf.length >= 14 && buf.subarray(0, 14).equals(SIG)) return parseRbxl(buf);
  const head = buf.subarray(0, 256).toString('utf8').trimStart();
  if (head.startsWith('<roblox') || head.startsWith('<?xml')) return parseRbxlx(buf);
  throw new Error('unrecognised place file signature');
}
module.exports = { parsePlace };
