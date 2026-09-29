'use strict';
// Runs every stored place's server Scripts inside the sandboxed runtime for a fixed span of VIRTUAL time (no real waiting) and writes
// research/script-runtime-report.json. This is a compatibility report for the BLOXEN runtime, not evidence about real Roblox behaviour.
// usage: node --disable-warning=ExperimentalWarning tools/script_report.js [virtualSeconds=600]
const fs = require('fs'); const path = require('path');
const { parsePlace } = require('../src/importer/parse'); const { World } = require('../src/gameserver/world'); const { ScriptHost } = require('../src/script/engine');
const root = path.join(__dirname, '..'); const secs = +(process.argv[2] || 600); const man = require('../preservation/manifests/places.json');
const out = { generated: new Date().toISOString(), virtualSeconds: secs, note: 'Virtual time; no players; no physics (Touched never fires). Status counts are per script.', places: {} };
for (const it of man.items) {
  const file = path.join(root, 'preservation', 'store', it.sha256 + '.rbx'); if (!it.sha256 || !fs.existsSync(file)) continue;
  const world = new World({ place: parsePlace(fs.readFileSync(file)) }); if (!world.scriptSources.length) { out.places[it.id] = { scripts: 0 }; continue; }
  const lines = []; const host = new ScriptHost(world, { print: s => lines.push(s.slice(0, 200)) }).start(); let events = 0; world.onChange(() => events++);
  for (let t = 0; t < secs; t++) host.step(1); const r = host.report();
  out.places[it.id] = { scripts: r.scripts.length, statusCounts: r.statusCounts, unsupported: r.unsupported, touchedConnections: r.touchedConnections, worldEventsEmitted: events, printedLines: lines.length, firstPrints: lines.slice(0, 5), errors: r.errors.map(e => ({ thread: e.thread, message: e.message })) };
}
fs.writeFileSync(path.join(root, 'research', 'script-runtime-report.json'), JSON.stringify(out, null, 2) + '\n'); console.log(JSON.stringify(out.places, null, 1).slice(0, 3000));
