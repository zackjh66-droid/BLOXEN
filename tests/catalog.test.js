'use strict';
// Catalog seed integrity + purchase rules. The data under test is the archived 2015 capture (ARCHIVED-NEAR-DATE), not invented content.
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const path = require('path');
const dbm = require('../src/lib/db'); const services = require('../src/lib/services'); const cfg = require('../src/lib/config');
const db = dbm.open(':memory:'); const seeded = dbm.seed(db); const uid = services.createUser(db, { username: 'Buyer', hash: 'x' });
const files = fs.readdirSync(cfg.catalogDir).filter(f => f.endsWith('.json')).map(f => JSON.parse(fs.readFileSync(path.join(cfg.catalogDir, f), 'utf8')));

test('every catalog file loads; ids are unique across files; every item has an archive URL and a provenance grade; nothing is marked as having a thumbnail', () => {
  const all = files.flatMap(f => f.items); for (const f of files) assert.equal(new Set(f.items.map(i => i.assetId)).size, f.items.length, 'asset ids unique within a capture');
  const dupes = all.length - new Set(all.map(i => i.assetId)).size; assert.ok(dupes <= 1, 'only the documented Blackvalk cross-capture overlap is allowed'); // later capture (name order) supersedes
  assert.equal(db.prepare('SELECT COUNT(*) n FROM catalog_items').get().n, all.length - dupes);
  assert.equal(db.prepare('SELECT source_capture c FROM catalog_items WHERE asset_id=124730194').get().c, '20150616223239', 'later capture wins');
  for (const f of files) { assert.match(f.source.archiveUrl, /^https:\/\/web\.archive\.org\/web\/\d{14}id_\//); assert.match(f.source.grade, /^ARCHIVED-/); }
  assert.equal(db.prepare("SELECT COUNT(*) n FROM catalog_items WHERE thumbnail <> 'MISSING'").get().n, 0);
  assert.equal(db.prepare("SELECT COUNT(*) n FROM assets WHERE availability <> 'MISSING'").get().n, 0, 'no asset content was retrieved, so none may claim to be available');
});
test('type coverage is reported honestly (no shirts/pants/heads exist in the archived pages read so far)', () => {
  const t = Object.fromEntries(db.prepare('SELECT type, COUNT(*) n FROM catalog_items GROUP BY type').all().map(r => [r.type, r.n])); assert.ok(t.Hat > 20 && t.Gear > 5 && t.Package >= 5 && t['T-Shirt'] >= 2 && t.Face >= 1, JSON.stringify(t));
  for (const absent of ['Shirt', 'Pants', 'Head']) assert.equal(t[absent], undefined, absent + ' must not be invented');
});
test('purchase rules: normal item is bought once; Limited-U / Limited resale-only / Builders Club items are refused with the archive-based reason', () => {
  const before = services.userById(db, uid).robux; const r = services.acquire(db, uid, 13745548); assert.equal(r.ok, true, JSON.stringify(r)); assert.equal(services.userById(db, uid).robux, before - 80);
  assert.equal(services.acquire(db, uid, 13745548).ok, false, 'cannot buy twice');
  for (const [id, re] of [[51245885, /Limited U/], [14462955, /Limited item/], [30331986, /Builders Club/], [63043890, /Limited U/]]) { const x = services.acquire(db, uid, id); assert.equal(x.ok, false, id); assert.match(x.error, re, id); }
  assert.equal(services.userById(db, uid).robux, before - 80, 'refused purchases cost nothing');
});
test('ticket-priced item (Blackvalk) has no Robux price and cannot be bought with R$', () => { const x = services.acquire(db, uid, 124730194); assert.equal(x.ok, false); });

test('post-2015 itemisation manifest agrees with the batch intake and adopts nothing', () => {
  const root = path.join(__dirname, '..', 'preservation', 'manifests'); const it = JSON.parse(fs.readFileSync(path.join(root, 'places-post2015-itemised.json'), 'utf8')).items;
  const intake = JSON.parse(fs.readFileSync(path.join(root, 'places-archive-beagleded-intake.json'), 'utf8')).items; const places = JSON.parse(fs.readFileSync(path.join(root, 'places.json'), 'utf8')).items;
  assert.equal(it.length, 4);
  for (const i of it) { const m = intake.find(x => x.file === i.file); assert.ok(m, i.file); assert.equal(m.sha256, i.sha256); assert.ok(['REJECTED-AS-HISTORICAL', 'QUARANTINED'].includes(i.disposition)); assert.ok(!places.some(p => p.sha256 === i.sha256 && /^ACCEPTED/.test(p.status)), 'not adopted: ' + i.file); }
});
