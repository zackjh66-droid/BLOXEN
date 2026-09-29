'use strict';
// Runs every stored place's server Scripts inside the sandboxed runtime for a fixed span of VIRTUAL time (no real waiting) and writes
// research/script-runtime-report.json. This is a compatibility report for the BLOXEN runtime, not evidence about real Roblox behaviour.
// usage: node --disable-warning=ExperimentalWarning tools/script_report.js [virtualSeconds=600]
const fs = require('fs'); const path = require('path');
const { parsePlace } = require('../src/importer/parse'); const { World } = require('../src/gameserver/world'); const { ScriptHost } = require('../src/script/engine');
const root = path.join(__dirname, '..'); const secs = +(process.argv[2] || 600); const man = require('../preservation/manifests/places.json');
const out = { generated: new Date().toISOString(), virtualSeconds: secs, note: 'Virtual time, no players and no physics in the main run (status counts are per script). contactProbe: a second run where one player is placed on each Touched-listening part in turn (box-overlap model only; Velocity/BodyMover effects and joints are not simulated).', places: {} };
for (const it of man.items) {
  const file = path.join(root, 'preservation', 'store', it.sha256 + '.rbx'); if (!it.sha256 || !fs.existsSync(file)) continue;
  const world = new World({ place: parsePlace(fs.readFileSync(file)) }); if (!world.scriptSources.length) { out.places[it.id] = { scripts: 0 }; continue; }
  const lines = []; const host = new ScriptHost(world, { print: s => lines.push(s.slice(0, 200)) }).start(); let events = 0; world.onChange(() => events++);
  for (let t = 0; t < secs; t++) host.step(1); const r = host.report();
  out.places[it.id] = { scripts: r.scripts.length, statusCounts: r.statusCounts, unsupported: r.unsupported, touchedConnections: r.touchedConnections, worldEventsEmitted: events, printedLines: lines.length, firstPrints: lines.slice(0, 5), errors: r.errors.map(e => ({ thread: e.thread, message: e.message })) };

  // ---- contact probe: drop one player onto each Touched-listening part in turn and see what the place's own scripts do
  const pw = new World({ place: parsePlace(fs.readFileSync(file)), respawnSeconds: 0.01 }); const plines = []; const ph = new ScriptHost(pw, { print: s => plines.push(s.slice(0, 160)) }).start(); ph.step(1);
  const rec = pw.addPlayer({ userId: 1, username: 'Probe', avatar: {} }); ph.step(1); const listeners = []; for (const [w, m] of ph.signals) { const t = m.get('Touched'); if (t && t.conns.some(c => c.connected) && ph.isA(w, 'BasePart') && ph._touchBox(w)) listeners.push(w); }
  const errBefore = ph.errors.length; let fired = 0; const ev0 = { n: 0 }; pw.onChange(() => ev0.n++);
  for (const w of listeners.slice(0, 80)) { const b = ph._touchBox(w); const top = { pos: { x: b.c[0], y: b.c[1] + b.h[1] + 2.5, z: b.c[2] }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] }; if (rec.dead) { ph.step(0.5); continue; } pw._moveCharacter(rec, top); ph.step(0.5); if ((ph.touching.get(w) || new Set()).size) fired++; pw._moveCharacter(rec, { pos: { x: 0, y: 5000, z: 0 }, rot: top.rot }); ph.step(0.25); }
  const newErr = ph.errors.slice(errBefore).map(e => e.thread + ': ' + e.message); const r2 = ph.report();
  out.places[it.id].contactProbe = { listeners: listeners.length, probed: Math.min(listeners.length, 80), listenersContacted: fired, scriptErrorsDuringProbe: [...new Set(newErr)].slice(0, 10), newPrints: plines.slice(-5), worldEventsDuringProbe: ev0.n, statusCounts: r2.statusCounts, unsupported: r2.unsupported };
}
fs.writeFileSync(path.join(root, 'research', 'script-runtime-report.json'), JSON.stringify(out, null, 2) + '\n'); console.log(JSON.stringify(out.places, null, 1).slice(0, 3000));
