'use strict';
// Asset store: real archived asset files (never invented ones). A file counts as AVAILABLE only if it is in the asset directory under its SHA-256
// AND the bytes still hash to that value AND a manifest entry records where it came from. Anything else stays MISSING.
// Files are addressed by hash, so a request can never name a path (no traversal), and nothing is ever fetched from Roblox at serve time.
const fs = require('fs'); const path = require('path'); const crypto = require('crypto'); const cfg = require('./config');
const MANIFEST = path.join(cfg.manifests, 'assets.json');
const GRADES = ['PRESERVED', 'ARCHIVED-EXACT', 'ARCHIVED-NEAR-DATE', 'RECONSTRUCTED-FROM-EVIDENCE'];   // INFERRED/UNKNOWN content is not an asset file
const MAX_BYTES = 64 * 1024 * 1024;
const sha256 = buf => crypto.createHash('sha256').update(buf).digest('hex');
function loadManifest(file = MANIFEST) { return fs.existsSync(file) ? JSON.parse(fs.readFileSync(file, 'utf8')) : { schema: 'bloxen.assets/1', items: [] }; }
function validate(meta) {
  const errs = [];
  if (!/^\d{1,12}$/.test(String(meta.id))) errs.push('id must be a numeric asset id');
  if (!Number.isInteger(meta.typeId) || meta.typeId < 1 || meta.typeId > 100) errs.push('typeId must be a Roblox AssetTypeId (1-100)');
  if (!meta.name || String(meta.name).length > 120) errs.push('name required (<=120 chars)');
  if (!/^https:\/\/[^\s]+$/.test(meta.source || '') && !/^[\w.-]+\/[\w.-]+@[0-9a-f]{7,40}:.+/.test(meta.source || '')) errs.push('source must be an https URL or repo@commit:path');
  if (!/^\d{4}-\d{2}-\d{2}$/.test(meta.sourceDate || '')) errs.push('sourceDate must be YYYY-MM-DD (the date of the archive copy)');
  if (!GRADES.includes(meta.grade)) errs.push('grade must be one of ' + GRADES.join(', '));
  return errs;
}
// Copy a file into the store (read-only, by hash) and record it in the manifest. The original is never modified.
function register(file, meta, { assetDir = cfg.assetDir, manifest = MANIFEST } = {}) {
  const errs = validate(meta); if (errs.length) throw new Error(errs.join('; '));
  const st = fs.statSync(file); if (!st.isFile()) throw new Error('not a regular file'); if (st.size === 0 || st.size > MAX_BYTES) throw new Error('size out of range (1..' + MAX_BYTES + ')');
  const buf = fs.readFileSync(file); const sha = sha256(buf);
  fs.mkdirSync(assetDir, { recursive: true }); const dest = path.join(assetDir, sha);
  if (!fs.existsSync(dest)) fs.writeFileSync(dest, buf, { mode: 0o444 });
  const m = loadManifest(manifest); const entry = { id: String(meta.id), typeId: meta.typeId, name: meta.name, source: meta.source, sourceDate: meta.sourceDate, grade: meta.grade, sha256: sha, size: buf.length, note: meta.note || '' };
  m.items = m.items.filter(i => i.id !== entry.id).concat(entry).sort((a, b) => +a.id - +b.id);
  fs.mkdirSync(path.dirname(manifest), { recursive: true }); fs.writeFileSync(manifest, JSON.stringify(m, null, 1)); return entry;
}
// true only if the stored bytes still match the recorded hash
function verify(entry, assetDir = cfg.assetDir) {
  if (!/^[0-9a-f]{64}$/.test(entry.sha256 || '')) return false; const f = path.join(assetDir, entry.sha256);
  try { return fs.statSync(f).isFile() && sha256(fs.readFileSync(f)) === entry.sha256; } catch { return false; }
}
function read(sha, assetDir = cfg.assetDir) { if (!/^[0-9a-f]{64}$/.test(sha || '')) return null; try { const b = fs.readFileSync(path.join(assetDir, sha)); return sha256(b) === sha ? b : null; } catch { return null; } }
module.exports = { MANIFEST, GRADES, MAX_BYTES, sha256, loadManifest, validate, register, verify, read };
