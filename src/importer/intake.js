#!/usr/bin/env node
'use strict';
// Intake pipeline:  candidate -> quarantine copy -> SHA-256 -> static parse -> inventory -> compat analysis -> report.
// Usage: node src/importer/intake.js <file> [--record provenance.json] [--store] [--out reports/dir]
// Never executes scripts. Originals are immutable: --store writes a read-only content-addressed copy under preservation/store/.
const fs = require('fs'); const path = require('path');
const { parsePlace } = require('./parse'); const { analyze, sha256 } = require('./analyze');

function intakeFile(file, opts = {}) {
  const buf = fs.readFileSync(file); const digest = sha256(buf);
  const place = parsePlace(buf); const a = analyze(place, { fileName: path.basename(file) });
  const report = { schema: 'bloxen.place-intake/1', file: path.basename(file), size: buf.length, sha256: digest, format: place.format, intakeAt: new Date().toISOString(), ...a };
  if (opts.store) {
    const dir = typeof opts.store === 'string' ? opts.store : path.join(__dirname, '../../preservation/store'); fs.mkdirSync(dir, { recursive: true });
    const dst = path.join(dir, digest + '.rbx'); if (!fs.existsSync(dst)) { fs.writeFileSync(dst, buf); fs.chmodSync(dst, 0o444); }
    report.storedAs = 'preservation/store/' + digest + '.rbx (gitignored, read-only)';
  }
  return { report, place };
}
module.exports = { intakeFile };
if (require.main === module) {
  const args = process.argv.slice(2); const file = args.find(a => !a.startsWith('--'));
  if (!file) { console.error('usage: intake.js <file> [--store] [--out dir]'); process.exit(2); }
  const { report } = intakeFile(file, { store: args.includes('--store') });
  const oi = args.indexOf('--out');
  if (oi >= 0) { fs.mkdirSync(args[oi + 1], { recursive: true }); fs.writeFileSync(path.join(args[oi + 1], report.sha256.slice(0, 16) + '.json'), JSON.stringify(report, null, 1)); }
  const { scripts, assets, classCounts, ...brief } = report; console.log(JSON.stringify({ ...brief, topClasses: Object.entries(classCounts).slice(0, 8), scripts: scripts.length, assetRefs: assets.length }, null, 1));
}
