'use strict';
// Game registry invariants. UNIT-TESTED. Status = provenance; it is never a playability claim.
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const path = require('path'); const crypto = require('crypto');
const registry = require('../src/lib/registry'); const cfg = require('../src/lib/config'); const db = require('../src/lib/db'); const S = require('../src/lib/services');
const rows = registry.registry();

test('counts and identity: 125 unique rows; PRESERVED 7, QUARANTINED 3, REJECTED 3, UNKNOWN 112', () => {
  assert.deepEqual(registry.counts(), { PRESERVED: 7, QUARANTINED: 3, REJECTED: 3, UNKNOWN: 112 }); assert.equal(rows.length, 125); assert.equal(new Set(rows.map(r => r.id)).size, rows.length, 'ids unique');
  for (const r of rows) { assert.ok(registry.STATUSES.includes(r.status), r.id); assert.ok(/^[0-9a-f]{64}$/.test(r.sha256 || ''), r.id + ' has a sha256'); assert.ok(r.basis && r.basis.length > 10, r.id + ' records why'); }
});
test('named decisions are pinned: bad-provenance places are never PRESERVED', () => {
  const by = t => rows.find(r => r.title.toLowerCase().includes(t)); assert.equal(by('natural').status, 'QUARANTINED'); assert.equal(by('stolen').status, 'REJECTED');
  for (const r of rows.filter(r => /^unknown:/.test(r.id))) assert.equal(r.status, 'UNKNOWN', 'a clean parse never promotes ' + r.id);
  for (const r of rows.filter(r => r.status === 'PRESERVED')) { assert.ok(!/stolen|leak/i.test(r.title + r.basis), r.id); assert.ok(r.source && r.source.repo && r.source.commit, r.id + ' source+commit'); }
});
test('registry PRESERVED set == the places offered by the website; nothing else is accepted', () => {
  const d = db.open(':memory:'); db.seed(d); const offered = S.placeList(d).map(p => p.id).sort(); assert.deepEqual(offered, rows.filter(r => r.status === 'PRESERVED').map(r => r.id).sort());
  for (const p of S.placeList(d)) assert.equal(p.sha256, registry.byId(p.id).sha256);
});
test('playability is a separate axis: only PRESERVED + in-store is eligible, and even then only GEOMETRY-ONLY', () => {
  for (const r of rows) { const p = registry.playability(r, { storeHas: true }); if (r.status === 'PRESERVED') { assert.equal(p.eligible, true); assert.equal(p.level, 'GEOMETRY-ONLY'); assert.match(p.reason, /REAL-CLIENT-TESTED: no/); } else { assert.equal(p.eligible, false, r.id); assert.equal(p.level, 'NONE'); } }
  assert.equal(registry.playability(rows.find(r => r.status === 'PRESERVED'), { storeHas: false }).eligible, false); assert.equal(registry.playability(null, { storeHas: true }).eligible, false);
});
test('store integrity: every stored place file re-hashes to its registry SHA-256; absent files are simply unavailable', () => {
  let present = 0; for (const r of rows.filter(r => r.status === 'PRESERVED')) { const f = path.join(cfg.storeDir, r.sha256 + '.rbx'); if (!fs.existsSync(f)) continue; present++; assert.equal(crypto.createHash('sha256').update(fs.readFileSync(f)).digest('hex'), r.sha256, r.id); }
  assert.ok(present >= 0);
});
test('compat reports: one per PRESERVED place, pinned to the same SHA-256, and never claim a real-client test', () => {
  for (const r of rows.filter(r => r.status === 'PRESERVED')) { const c = registry.compatReport(r.id); assert.ok(c, 'report for ' + r.id); assert.equal(c.sha256, r.sha256); assert.match(c.tested.REAL_CLIENT, /NOT TESTED/); assert.ok(['SUPPORTED', 'PARTIAL', 'UNSUPPORTED', 'UNKNOWN'].includes(c.overall)); assert.equal(c.scripts.runtimeOnByDefault, false); assert.ok(c.instances > 0); }
  assert.equal(registry.compatReport('unknown:nope'), null);
});
test('the game-server manager refuses a non-accepted place even when asked directly', async () => {
  const { GameServerManager } = require('../src/lib/gameservers'); const m = new GameServerManager(db.open(':memory:')); await assert.rejects(() => m.ensure({ id: 'x', status: 'QUARANTINED', sha256: '0'.repeat(64) }), /not accepted/);
});
