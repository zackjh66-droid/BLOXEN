#!/usr/bin/env node
'use strict';
// Builds preservation/manifests/place-compat-reports.json: a per-place compatibility report (static analysis + replication stats + sandbox script status) for every
// registry-PRESERVED place whose file is in the local store. The website's Game Details page reads this file, so the report is visible even where the big place files are not.
// Nothing is executed outside the sandboxed script host (virtual time). usage: node --disable-warning=ExperimentalWarning tools/build_compat_reports.js
const fs = require('fs'); const path = require('path');
const { parsePlace } = require('../src/importer/parse'); const { analyze } = require('../src/importer/analyze'); const { World } = require('../src/gameserver/world'); const { ScriptHost } = require('../src/script/engine');
const reg = require('../src/lib/registry'); const root = path.join(__dirname, '..'); const out = { schema: 'bloxen.place-compat-reports/1', generated: new Date().toISOString().slice(0, 10), note: 'Static analysis + BLOXEN sandbox results. UNIT/SIMULATOR evidence only; REAL-CLIENT-TESTED is "no" for every place.', reports: {} };
const idx = (() => { try { return JSON.parse(fs.readFileSync(path.join(root, 'research', 'script-index.json'), 'utf8')); } catch { return null; } })();
const teleportsFor = id => { const m = new Map(); for (const row of (idx && idx.places && idx.places[id] ? idx.places[id].rows : [])) for (const t of row.teleportTargets || []) m.set(t.robloxPlaceId, t); return [...m.values()].map(t => ({ robloxPlaceId: t.robloxPlaceId, result: t.result, local: t.local || null, reason: t.reason || null })); };
const old = (() => { try { return JSON.parse(fs.readFileSync(path.join(root, 'preservation', 'manifests', 'place-compat-reports.json'), 'utf8')).reports; } catch { return {}; } })();
for (const r of reg.registry().filter(x => x.status === 'PRESERVED')) {
  const file = path.join(root, 'preservation', 'store', r.sha256 + '.rbx'); if (!fs.existsSync(file)) { if (old[r.id] && old[r.id].sha256 === r.sha256) out.reports[r.id] = old[r.id]; continue; }
  const parsed = parsePlace(fs.readFileSync(file)); const a = analyze(parsed); const world = new World({ place: parsePlace(fs.readFileSync(file)), respawnSeconds: 0.01 }); const host = new ScriptHost(world, {}).start(); for (let t = 0; t < 30; t++) host.step(1); const rep = host.report();
  const st = world.stats; const dropped = Object.entries(st.droppedClasses || {}).sort((x, y) => y[1] - x[1]).slice(0, 12);
  out.reports[r.id] = { sha256: r.sha256, registryStatus: r.status, overall: a.overall, importerCompat: a.compat, era: { verdict: a.era.verdict, post2015Score: a.era.post2015Score, legacyScore: a.era.legacyScore },
    instances: a.instanceCount, parts: a.parts, meshes: a.meshes, sounds: a.sounds, guis: a.guis, terrain: a.terrain,
    replication: { imported: st.imported, serverOnlyDropped: st.serverOnlyDropped, unknownClassDropped: st.unknownClassDropped, propsDropped: st.propsDropped, topDroppedClasses: dropped.map(([c, n]) => `${c} x${n}`) },
    scripts: { total: a.scriptSummary.total, byClass: a.scriptSummary.byClass, markerCounts: a.scriptSummary.markerCounts, flagged: a.scriptSummary.flagged, sandboxStatusCounts: rep.statusCounts, touchedConnections: rep.touchedConnections, unsupportedApis: Object.keys(rep.unsupported).slice(0, 12), runtimeOnByDefault: false },
    assets: { distinctReferences: a.assets.filter(x => x.kind === 'asset-id').length, clientBuiltin: a.assets.filter(x => x.kind === 'client-builtin').length, availableInStore: 0, note: 'Every referenced asset is MISSING unless the asset manifest lists it (it is empty); missing assets are never substituted.' },
    teleportDestinations: teleportsFor(r.id),
    blockers: [ ...(a.terrain ? ['Terrain voxel data is not replicated'] : []), ...(a.guis ? [`${a.guis} GUI instance(s) (GUI replication unverified)`] : []), ...(a.scriptSummary.total ? ['Place logic is Lua: sandbox runtime is OFF by default and is not a faithful engine; no server-side physics (Touched = simplified player-contact model)'] : []), 'No real-client test (Windows required)' ],
    tested: { UNIT: true, SIMULATOR: 'BLOXEN simulator client joins, receives the place and spawns (tests/e2e-flow.test.js)', REAL_CLIENT: 'NOT TESTED: REAL CLIENT EXECUTION = BLOCKED: WINDOWS REQUIRED' } };
}
fs.writeFileSync(path.join(root, 'preservation', 'manifests', 'place-compat-reports.json'), JSON.stringify(out, null, 1) + '\n'); for (const [id, r] of Object.entries(out.reports)) console.log(id.padEnd(34), r.overall, r.instances, 'inst', r.scripts.total, 'scripts', JSON.stringify(r.scripts.sandboxStatusCounts));
