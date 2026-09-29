#!/usr/bin/env node
'use strict';
// Merge hand-curated provenance records with the automated intake manifest -> preservation/manifests/places.json
// Usage: node tools/build_places_manifest.js
const fs = require('fs'); const path = require('path');
const root = path.join(__dirname, '..', 'preservation', 'manifests');
const cur = JSON.parse(fs.readFileSync(path.join(root, 'places-curation.json'), 'utf8'));
const intake = JSON.parse(fs.readFileSync(path.join(root, 'places-archive-beagleded-intake.json'), 'utf8'));
const out = { schema: 'bloxen.places/1', generatedAt: new Date().toISOString(), note: cur.note, grades: cur.grades, source: { ...cur.source, commit: intake.sourceCommit }, items: [], notFound: cur.notFound };
for (const c of cur.items) {
  let i = intake.items.find(x => x.file === c.file);
  if (c.intakeExtra) i = { ...c.intakeExtra, addedInCommit: c.sourceCommit, addedInMessage: 'see curation evidence' }; // version not at HEAD: intake report supplied by hand from the pinned commit
  if (!i) throw new Error('not in intake manifest: ' + c.file);
  const { intakeExtra, ...rest } = c;
  out.items.push({ ...rest, id: c.id || c.file.replace(/\.rbxlx?$/i, '').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, ''),
    source: { repo: cur.source.repo, commit: c.sourceCommit || i.addedInCommit, path: 'Games/' + c.file, addedInMessage: i.addedInMessage }, sha256: i.sha256, size: i.size, format: i.format,
    stats: { instances: i.instances, parts: i.parts, scripts: i.scripts, assetRefs: i.assetRefs, terrain: i.terrain, guis: i.guis }, eraVerdict: i.eraVerdict, importerCompat: i.compat, dependencies: { externalAssetIds: i.assetRefs } });
}
fs.writeFileSync(path.join(root, 'places.json'), JSON.stringify(out, null, 2) + '\n'); console.log('wrote places.json with', out.items.length, 'items');
