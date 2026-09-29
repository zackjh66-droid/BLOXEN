#!/usr/bin/env node
'use strict';
// Batch static intake of a cloned preservation repo of place files (read-only). Writes a summary manifest.
// usage: node tools/batch_intake.js <clone-dir>/Games <out.json> <source-repo-url>
const fs = require('fs'); const path = require('path'); const { execFileSync } = require('child_process');
const { intakeFile } = require('../src/importer/intake');
const [dir, out, repo] = process.argv.slice(2);
const cloneRoot = path.dirname(dir);
const addedIn = {};
const log = execFileSync('git', ['-C', cloneRoot, 'log', '--reverse', '--name-only', '--format=@@%H|%ad|%s', '--date=short'], { maxBuffer: 1 << 26 }).toString();
let cur = null; for (const line of log.split('\n')) { if (line.startsWith('@@')) { const [h, d, ...s] = line.slice(2).split('|'); cur = { commit: h, date: d, message: s.join('|') }; } else if (line.trim() && !addedIn[line.trim()]) addedIn[line.trim()] = cur; }
const head = execFileSync('git', ['-C', cloneRoot, 'rev-parse', 'HEAD']).toString().trim();
const items = [];
for (const f of fs.readdirSync(dir).sort()) {
  try {
    const { report } = intakeFile(path.join(dir, f));
    const ai = addedIn['Games/' + f] || null;
    items.push({ file: f, sha256: report.sha256, size: report.size, format: report.format, instances: report.instanceCount, parts: report.parts, scripts: report.scriptSummary.total, scriptMarkers: report.scriptSummary.markerCounts,
      assetRefs: report.assets.length, eraVerdict: report.era.verdict, post2015Score: report.era.post2015Score, legacyScore: report.era.legacyScore, compat: report.overall, terrain: report.terrain, guis: report.guis,
      addedInCommit: ai && ai.commit.slice(0, 10), addedInDate: ai && ai.date, addedInMessage: ai && ai.message, parseWarnings: report.parseWarningCount });
  } catch (e) { items.push({ file: f, error: e.message }); }
}
fs.writeFileSync(out, JSON.stringify({ schema: 'bloxen.batch-intake/1', sourceRepo: repo, sourceCommit: head, generatedAt: new Date().toISOString(), count: items.length, items }, null, 1));
console.log('wrote', out, items.length);
