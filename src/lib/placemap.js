'use strict';
// Historical PlaceId -> approved local BLOXEN place. See preservation/manifests/place-mapping.json for the rules.
// resolve() NEVER performs network I/O and never returns an address: only a local registry id that the caller may turn into a normal Play ticket.
const fs = require('fs'); const path = require('path'); const cfg = require('./config'); const registry = require('./registry');
function load(file = path.join(cfg.manifests, 'place-mapping.json')) { return JSON.parse(fs.readFileSync(file, 'utf8')); }
function resolve(robloxPlaceId, { storeHas = () => true, mapping } = {}) {
  const id = Number(robloxPlaceId); if (!Number.isSafeInteger(id) || id <= 0) return { ok: false, result: 'unsupported-local-destination', reason: 'invalid PlaceId', robloxPlaceId: String(robloxPlaceId).slice(0, 24) };
  const m = mapping || load(); const e = m.entries.find(x => x.robloxPlaceId === id);
  if (!e) return { ok: false, result: 'unsupported-local-destination', reason: 'UNMAPPED: no approved local place for this historical PlaceId', robloxPlaceId: id };
  if (!e.approved) return { ok: false, result: 'unsupported-local-destination', reason: 'mapping exists but is not approved', robloxPlaceId: id };
  const r = registry.byId(e.local);
  if (!r || r.status !== 'PRESERVED') return { ok: false, result: 'unsupported-local-destination', reason: `target ${e.local} is not registry-PRESERVED`, robloxPlaceId: id };
  if (e.sha256 && r.sha256 !== e.sha256) return { ok: false, result: 'unsupported-local-destination', reason: 'pinned SHA-256 differs from the registry entry', robloxPlaceId: id };
  if (!storeHas(r)) return { ok: false, result: 'unsupported-local-destination', reason: `UNAVAILABLE: ${e.local} is not in the local store`, robloxPlaceId: id };
  return { ok: true, result: 'local-destination', local: r.id, title: r.title, sha256: r.sha256, robloxPlaceId: id };
}
module.exports = { load, resolve };
