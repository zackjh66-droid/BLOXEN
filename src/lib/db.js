'use strict';
const fs = require('fs'); const path = require('path'); const { DatabaseSync } = require('node:sqlite'); const cfg = require('./config'); const assetstore = require('./assetstore');

const SCHEMA = `
PRAGMA foreign_keys = ON; PRAGMA journal_mode = WAL;
CREATE TABLE IF NOT EXISTS users(id INTEGER PRIMARY KEY, username TEXT NOT NULL UNIQUE COLLATE NOCASE, pass_hash TEXT NOT NULL, created_at INTEGER NOT NULL, birthday TEXT, gender TEXT, blurb TEXT NOT NULL DEFAULT '', robux INTEGER NOT NULL DEFAULT 0, tickets INTEGER NOT NULL DEFAULT 0, last_online INTEGER);
CREATE TABLE IF NOT EXISTS sessions(token_hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, csrf TEXT NOT NULL, created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS friend_requests(from_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, to_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, created_at INTEGER NOT NULL, PRIMARY KEY(from_id,to_id));
CREATE TABLE IF NOT EXISTS friends(a INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, b INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, since INTEGER NOT NULL, PRIMARY KEY(a,b), CHECK(a<b));
CREATE TABLE IF NOT EXISTS assets(id INTEGER PRIMARY KEY, type_id INTEGER, type TEXT, name TEXT, source TEXT, source_date TEXT, sha256 TEXT, local_path TEXT, provenance TEXT NOT NULL DEFAULT 'UNKNOWN', availability TEXT NOT NULL DEFAULT 'MISSING', note TEXT);
CREATE TABLE IF NOT EXISTS asset_uses(asset_id INTEGER NOT NULL, place_id TEXT NOT NULL, PRIMARY KEY(asset_id,place_id));
CREATE TABLE IF NOT EXISTS catalog_items(asset_id INTEGER PRIMARY KEY, name TEXT NOT NULL, description TEXT, creator TEXT, creator_id INTEGER, type_id INTEGER NOT NULL, type TEXT NOT NULL, price_robux INTEGER, price_tickets INTEGER, created_ms INTEGER, updated_ms INTEGER, sales TEXT, favorited TEXT, limited_unique INTEGER NOT NULL DEFAULT 0, item_url TEXT, source_capture TEXT, provenance TEXT, thumbnail TEXT NOT NULL DEFAULT 'MISSING', min_membership INTEGER NOT NULL DEFAULT 0, resale_only INTEGER NOT NULL DEFAULT 0);
CREATE TABLE IF NOT EXISTS inventory(user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, asset_id INTEGER NOT NULL, acquired_at INTEGER NOT NULL, PRIMARY KEY(user_id,asset_id));
CREATE TABLE IF NOT EXISTS avatars(user_id INTEGER PRIMARY KEY REFERENCES users(id) ON DELETE CASCADE, head_color INTEGER NOT NULL DEFAULT 194, torso_color INTEGER NOT NULL DEFAULT 194, left_arm_color INTEGER NOT NULL DEFAULT 194, right_arm_color INTEGER NOT NULL DEFAULT 194, left_leg_color INTEGER NOT NULL DEFAULT 194, right_leg_color INTEGER NOT NULL DEFAULT 194, updated_at INTEGER);
CREATE TABLE IF NOT EXISTS avatar_items(user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, asset_id INTEGER NOT NULL, PRIMARY KEY(user_id,asset_id));
CREATE TABLE IF NOT EXISTS places(id TEXT PRIMARY KEY, title TEXT NOT NULL, creator TEXT, roblox_place_id INTEGER, tier TEXT, grade TEXT, status TEXT, sha256 TEXT, size INTEGER, format TEXT, stats TEXT, evidence TEXT, date_note TEXT, method TEXT, public_evidence TEXT, source TEXT, notes TEXT, compat TEXT, playable TEXT NOT NULL DEFAULT 'GEOMETRY-ONLY', sort INTEGER);
CREATE TABLE IF NOT EXISTS favorites(user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, place_id TEXT NOT NULL, PRIMARY KEY(user_id,place_id));
CREATE TABLE IF NOT EXISTS recent_plays(user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, place_id TEXT NOT NULL, at INTEGER NOT NULL, PRIMARY KEY(user_id,place_id));
CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY, from_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, to_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, subject TEXT NOT NULL, body TEXT NOT NULL, created_at INTEGER NOT NULL, read INTEGER NOT NULL DEFAULT 0);
CREATE TABLE IF NOT EXISTS groups(id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE COLLATE NOCASE, description TEXT NOT NULL DEFAULT '', owner_id INTEGER NOT NULL REFERENCES users(id), created_at INTEGER NOT NULL);
CREATE TABLE IF NOT EXISTS group_members(group_id INTEGER NOT NULL REFERENCES groups(id) ON DELETE CASCADE, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, role TEXT NOT NULL DEFAULT 'Member', PRIMARY KEY(group_id,user_id));
CREATE TABLE IF NOT EXISTS launch_tickets(hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL, place_id TEXT NOT NULL, server_id TEXT NOT NULL, created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, used_at INTEGER);
CREATE TABLE IF NOT EXISTS join_tokens(hash TEXT PRIMARY KEY, user_id INTEGER NOT NULL, place_id TEXT NOT NULL, server_id TEXT NOT NULL, created_at INTEGER NOT NULL, expires_at INTEGER NOT NULL, used_at INTEGER);
CREATE TABLE IF NOT EXISTS teleports(id INTEGER PRIMARY KEY, user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, from_place TEXT NOT NULL, to_place TEXT NOT NULL, roblox_place_id INTEGER, created_at INTEGER NOT NULL, consumed_at INTEGER);
CREATE TABLE IF NOT EXISTS item_favorites(user_id INTEGER NOT NULL REFERENCES users(id) ON DELETE CASCADE, asset_id INTEGER NOT NULL, PRIMARY KEY(user_id,asset_id));
CREATE TABLE IF NOT EXISTS missing_asset_log(asset_id TEXT NOT NULL, requested_at INTEGER NOT NULL, context TEXT);
CREATE TABLE IF NOT EXISTS compat_log(at INTEGER NOT NULL, path TEXT, note TEXT);
`;
function open(file = cfg.dbPath) {
  if (file !== ':memory:') fs.mkdirSync(path.dirname(file), { recursive: true });
  const db = new DatabaseSync(file); db.exec(SCHEMA);
  for (const col of ['min_membership INTEGER NOT NULL DEFAULT 0', 'resale_only INTEGER NOT NULL DEFAULT 0']) { try { db.exec('ALTER TABLE catalog_items ADD COLUMN ' + col); } catch { /* already present */ } } // migrate databases created before these columns existed
  return db;
}
const TYPE_NAMES = { 2: 'T-Shirt', 8: 'Hat', 11: 'Shirt', 12: 'Pants', 17: 'Head', 18: 'Face', 19: 'Gear', 32: 'Package' };

function seed(db, opts = {}) {
  // ---- catalog (real archived items only)
  const ins = db.prepare(`INSERT OR REPLACE INTO catalog_items(asset_id,name,description,creator,creator_id,type_id,type,price_robux,price_tickets,created_ms,updated_ms,sales,favorited,limited_unique,item_url,source_capture,provenance,min_membership,resale_only) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`);
  const insA = db.prepare(`INSERT OR IGNORE INTO assets(id,type_id,type,name,source,source_date,provenance,availability,note) VALUES(?,?,?,?,?,?,?,'MISSING',?)`);
  let items = 0;
  if (fs.existsSync(cfg.catalogDir)) for (const f of fs.readdirSync(cfg.catalogDir).filter(x => x.endsWith('.json')).sort()) {
    const j = JSON.parse(fs.readFileSync(path.join(cfg.catalogDir, f), 'utf8'));
    for (const i of j.items) {
      const name = i.name.replace(/&amp;/g, '&');
      ins.run(i.assetId, name, i.description, i.creator, i.creatorId, i.assetTypeId, i.assetType, i.priceRobux, i.priceTickets, i.createdMs, i.updatedMs, i.sales, i.favorited, i.isLimitedUnique ? 1 : 0, i.itemUrl, j.source.capture, j.source.grade, i.minimumMembershipLevel || 0, (i.priceView === 1 || i.isLimited) ? 1 : 0); items++;
      insA.run(i.assetId, i.assetTypeId, i.assetType, name, j.source.archiveUrl, j.source.capture, j.source.grade, 'Asset content (mesh/texture/model) was not retrieved; only catalog metadata is archived.');
    }
  }
  // ---- real asset files (manifest): AVAILABLE only when the stored bytes still match the recorded SHA-256; otherwise the row says why it is not.
  const am = assetstore.loadManifest(opts.assetManifest); let assets = 0;
  for (const e of am.items) {
    const ok = assetstore.verify(e, opts.assetDir); const note = ok ? e.note || '' : 'Manifest lists this file but it is absent from (or no longer matches) the local asset store; treated as MISSING.';
    db.prepare(`INSERT INTO assets(id,type_id,type,name,source,source_date,sha256,local_path,provenance,availability,note) VALUES(?,?,?,?,?,?,?,?,?,?,?)
      ON CONFLICT(id) DO UPDATE SET type_id=excluded.type_id, name=excluded.name, source=excluded.source, source_date=excluded.source_date, sha256=excluded.sha256, local_path=excluded.local_path, provenance=excluded.provenance, availability=excluded.availability, note=excluded.note`)
      .run(+e.id, e.typeId, TYPE_NAMES[e.typeId] || 'Type ' + e.typeId, e.name, e.source, e.sourceDate, e.sha256, ok ? e.sha256 : null, e.grade, ok ? 'AVAILABLE' : 'MISSING', note); assets++;
  }
  // ---- places (manifest)
  const mp = path.join(cfg.manifests, 'places.json'); let places = 0;
  if (fs.existsSync(mp)) {
    const m = JSON.parse(fs.readFileSync(mp, 'utf8')); const rank = { P1: 1, P2: 2, Q: 9 };
    const insP = db.prepare(`INSERT OR REPLACE INTO places(id,title,creator,roblox_place_id,tier,grade,status,sha256,size,format,stats,evidence,date_note,method,public_evidence,source,notes,compat,playable,sort) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)`);
    for (const p of m.items) { insP.run(p.id, p.title, p.creator, p.placeId || null, p.tier, p.grade, p.status, p.sha256, p.size, p.format, JSON.stringify(p.stats), JSON.stringify(p.evidence), p.date, p.method, p.publicEvidence, JSON.stringify(p.source), p.notes || p.reason || '', p.importerCompat, 'GEOMETRY-ONLY', rank[p.tier] || 5); places++; }
  }
  return { items, places, assets };
}
module.exports = { open, seed, TYPE_NAMES };
