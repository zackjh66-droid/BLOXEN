'use strict';
// Asset store contract. The fixture files are throwaway test bytes in a temp dir - they are NOT historical content and never enter the real store/manifest.
const os = require('os'); const fs = require('fs'); const path = require('path');
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-assets-')); process.env.BLOXEN_ASSETS = path.join(tmp, 'store'); // must be set before config is first loaded
const test = require('node:test'); const assert = require('node:assert/strict');
const A = require('../src/lib/assetstore'); const dbm = require('../src/lib/db'); const { start } = require('../src/web/main');
const manifest = path.join(tmp, 'assets.json'); const assetDir = process.env.BLOXEN_ASSETS;
const META = { id: '1028606', typeId: 8, name: 'Test fixture', source: 'https://web.archive.org/web/20150916010724/http://www.roblox.com/item?id=1028606', sourceDate: '2015-09-16', grade: 'ARCHIVED-NEAR-DATE' };
const fixture = path.join(tmp, 'fixture.bin'); fs.writeFileSync(fixture, Buffer.from('bloxen test bytes - not a historical asset'));

test('the committed manifest is empty: no real asset content has been retrieved, so nothing may claim to be available', () => {
  const m = A.loadManifest(); assert.deepEqual(m.items, []);
  const db = dbm.open(':memory:'); dbm.seed(db); assert.equal(db.prepare("SELECT COUNT(*) n FROM assets WHERE availability='AVAILABLE'").get().n, 0);
});
test('register validates provenance fields and refuses incomplete or unsafe metadata', () => {
  for (const bad of [{ id: '12a' }, { typeId: 0 }, { name: '' }, { source: 'file:///etc/passwd' }, { source: 'trust me' }, { sourceDate: 'yesterday' }, { grade: 'INFERRED' }, { grade: 'UNKNOWN' }])
    assert.throws(() => A.register(fixture, { ...META, ...bad }, { assetDir, manifest }), /id must|typeId|name required|source must|sourceDate|grade must/, JSON.stringify(bad));
  assert.throws(() => A.register(tmp, META, { assetDir, manifest }), /not a regular file/);
  const empty = path.join(tmp, 'empty'); fs.writeFileSync(empty, ''); assert.throws(() => A.register(empty, META, { assetDir, manifest }), /size out of range/);
  assert.equal(fs.existsSync(manifest), false, 'nothing recorded on refusal');
});
test('register stores by SHA-256 (read-only), records the manifest; seed marks it AVAILABLE only while the hash verifies', () => {
  const e = A.register(fixture, META, { assetDir, manifest }); assert.equal(e.sha256, A.sha256(fs.readFileSync(fixture))); assert.equal(fs.statSync(path.join(assetDir, e.sha256)).mode & 0o222, 0, 'read-only');
  assert.equal(A.loadManifest(manifest).items[0].grade, 'ARCHIVED-NEAR-DATE');
  const db = dbm.open(':memory:'); dbm.seed(db, { assetManifest: manifest, assetDir });
  const row = db.prepare('SELECT * FROM assets WHERE id=1028606').get(); assert.equal(row.availability, 'AVAILABLE'); assert.equal(row.sha256, e.sha256); assert.equal(row.source_date, '2015-09-16'); assert.equal(row.provenance, 'ARCHIVED-NEAR-DATE');
  fs.chmodSync(path.join(assetDir, e.sha256), 0o644); fs.writeFileSync(path.join(assetDir, e.sha256), 'tampered'); // tamper
  const db2 = dbm.open(':memory:'); dbm.seed(db2, { assetManifest: manifest, assetDir }); const r2 = db2.prepare('SELECT * FROM assets WHERE id=1028606').get(); assert.equal(r2.availability, 'MISSING'); assert.match(r2.note, /no longer matches/);
  assert.equal(A.read(e.sha256, assetDir), null); assert.equal(A.read('../../etc/passwd', assetDir), null, 'only 64-hex names are ever opened');
  fs.writeFileSync(path.join(assetDir, e.sha256), fs.readFileSync(fixture)); // restore for the next test
});
test('compat /asset/?id= serves verified bytes, 404s + logs MISSING ids, and re-checks the hash on every serve', async t => {
  const app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true, storeDir: path.join(tmp, 'noplaces') }); t.after(() => app.close());
  dbm.seed(app.db, { assetManifest: manifest, assetDir }); const base = app.compatUrl;
  const ok = await fetch(base + '/asset/?id=1028606'); assert.equal(ok.status, 200); assert.equal(Buffer.from(await ok.arrayBuffer()).toString(), 'bloxen test bytes - not a historical asset');
  const miss = await fetch(base + '/asset/?id=13745548'); assert.equal(miss.status, 404); assert.match(await miss.text(), /MISSING/); assert.ok(app.db.prepare('SELECT COUNT(*) n FROM missing_asset_log WHERE asset_id=13745548').get().n >= 1);
  const sha = A.loadManifest(manifest).items[0].sha256; fs.chmodSync(path.join(assetDir, sha), 0o644); fs.writeFileSync(path.join(assetDir, sha), 'swapped after registration');
  const bad = await fetch(base + '/asset/?id=1028606'); assert.equal(bad.status, 404); assert.match(await bad.text(), /SHA-256/);
  assert.equal((await fetch(base + '/asset/?id=../../etc/passwd')).status, 400);
});
