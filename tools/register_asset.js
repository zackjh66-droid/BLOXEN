#!/usr/bin/env node
'use strict';
// Register a real archived asset file in the BLOXEN asset store. Nothing is executed or parsed; the file is hashed and copied read-only by SHA-256.
// usage: node tools/register_asset.js <file> --id <assetId> --type <AssetTypeId> --name <name> --source <https-url | repo@commit:path> --date <YYYY-MM-DD> --grade <PRESERVED|ARCHIVED-EXACT|ARCHIVED-NEAR-DATE|RECONSTRUCTED-FROM-EVIDENCE> [--note ..]
const { register } = require('../src/lib/assetstore');
const a = process.argv.slice(2); const file = a[0]; const opt = {}; for (let i = 1; i < a.length; i += 2) opt[a[i].replace(/^--/, '')] = a[i + 1];
if (!file || !opt.id) { console.error(require('fs').readFileSync(__filename, 'utf8').split('\n').slice(3, 4).join('\n').replace('// ', '')); process.exit(2); }
try { const e = register(file, { id: opt.id, typeId: +opt.type, name: opt.name, source: opt.source, sourceDate: opt.date, grade: opt.grade, note: opt.note }); console.log('registered', e.id, e.sha256, e.size + ' B'); }
catch (e) { console.error('REFUSED:', e.message); process.exit(1); }
