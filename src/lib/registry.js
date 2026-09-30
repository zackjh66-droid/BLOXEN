'use strict';
// Game registry: one row per candidate place file that BLOXEN has ever looked at, with an explicit status.
//   PRESERVED   - bytes held (by SHA-256), intake passed, evidence that the place was publicly released, era not contradicted.
//                 This says NOTHING about playability: see `playability` and the compatibility report.
//   QUARANTINED - bytes seen but provenance/era is unresolved. Never offered as a game. Kept so it is not re-litigated.
//   REJECTED    - must not be used (leak, post-2015 content presented as 2015, etc.). Never offered.
//   UNKNOWN     - listed in the source archive but never evaluated for provenance. Never offered.
// Parsing successfully is NOT a promotion criterion: an UNKNOWN place that parses cleanly stays UNKNOWN.
const fs = require('fs'); const path = require('path'); const cfg = require('./config');
const STATUSES = ['PRESERVED', 'QUARANTINED', 'REJECTED', 'UNKNOWN'];
const readJson = f => { const p = path.join(cfg.manifests, f); return fs.existsSync(p) ? JSON.parse(fs.readFileSync(p, 'utf8')) : null; };

function build() {
  const out = []; const seen = new Set();
  const places = readJson('places.json'); const post = readJson('places-post2015-itemised.json'); const intake = readJson('places-archive-beagleded-intake.json');
  for (const p of (places && places.items) || []) {
    const st = /^ACCEPTED/.test(p.status) ? 'PRESERVED' : p.status === 'REJECTED' ? 'REJECTED' : 'QUARANTINED'; seen.add(p.sha256);
    out.push({ id: p.id, title: p.title, creator: p.creator, robloxPlaceId: p.placeId || null, status: st, basis: st === 'PRESERVED' ? `${p.status}; grade ${p.grade}; ${p.publicEvidence}` : (p.reason || p.status), sha256: p.sha256, grade: p.grade, tier: p.tier, compatibility: p.importerCompat || null, stats: p.stats || null, source: p.source });
  }
  for (const p of (post && post.items) || []) { if (seen.has(p.sha256)) continue; seen.add(p.sha256);
    const st = /reject/i.test(p.disposition || '') ? 'REJECTED' : 'QUARANTINED'; out.push({ id: 'post2015:' + p.file.replace(/\.rbxl$/i, '').toLowerCase().replace(/[^a-z0-9]+/g, '-'), title: p.file.replace(/\.rbxl$/i, ''), creator: 'UNKNOWN', robloxPlaceId: null, status: st, basis: p.reason || p.disposition, sha256: p.sha256, grade: 'UNKNOWN', tier: 'Q', compatibility: null, stats: { instances: p.instances, scripts: p.scripts }, source: { repo: p.sourceRepo, commit: p.sourceCommit, path: 'Games/' + p.file } }); }
  for (const p of (intake && intake.items) || []) { if (seen.has(p.sha256)) continue; seen.add(p.sha256);
    out.push({ id: 'unknown:' + p.sha256.slice(0, 12), title: p.file.replace(/\.rbxl$/i, ''), creator: 'UNKNOWN', robloxPlaceId: null, status: 'UNKNOWN', basis: 'Present in the source archive; file name is not provenance; creator, place ID and public-release evidence have NOT been established. Parsing cleanly does not promote it.', sha256: p.sha256, grade: 'UNKNOWN', tier: null, compatibility: p.compat || null, stats: { instances: p.instances, parts: p.parts, scripts: p.scripts }, source: { repo: intake.sourceRepo, commit: intake.sourceCommit, path: 'Games/' + p.file } }); }
  return out;
}
let cache = null; const registry = () => cache || (cache = build());
const byId = id => registry().find(r => r.id === id) || null;
const counts = () => Object.fromEntries(STATUSES.map(s => [s, registry().filter(r => r.status === s).length]));
// Playability is a separate axis and is never inferred from the registry status.
//   NONE           - not offered
//   GEOMETRY-ONLY  - map geometry + spawn replicate; the place's own logic is not (reliably) running; no real-client test
function playability(entry, { storeHas }) {
  if (!entry || entry.status !== 'PRESERVED') return { level: 'NONE', eligible: false, reason: `registry status is ${entry ? entry.status : 'absent'}` };
  if (!storeHas) return { level: 'GEOMETRY-ONLY', eligible: false, reason: 'place file is not in the local store (run tools/fetch_places.js)' };
  return { level: 'GEOMETRY-ONLY', eligible: true, reason: 'geometry and spawn replicate (SIMULATOR-TESTED); the place\'s scripts are off by default and never claimed to reproduce original gameplay; REAL-CLIENT-TESTED: no' };
}
module.exports = { STATUSES, registry, byId, counts, playability, _reset: () => { cache = null; } };
