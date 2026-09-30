'use strict';
// Starts historical-place game servers on demand (loopback only). The place is loaded from the content-addressed store and re-verified by SHA-256.
const fs = require('fs'); const path = require('path'); const crypto = require('crypto');
const cfg = require('./config'); const { parsePlace } = require('../importer/parse'); const { GameServer } = require('../gameserver/server'); const auth = require('./auth'); const services = require('./services');

class GameServerManager {
  constructor(db, { logger = () => {}, storeDir = cfg.storeDir } = {}) { this.db = db; this.servers = new Map(); this.log = logger; this.storeDir = storeDir; }
  storePathFor(place) { return path.join(this.storeDir, place.sha256 + '.rbx'); }
  available(place) { return fs.existsSync(this.storePathFor(place)); }
  async ensure(place) {
    const existing = this.servers.get(place.id); if (existing) return existing;
    if (!/^ACCEPTED/.test(place.status)) throw Object.assign(new Error('place is not accepted'), { code: 'NOT_ACCEPTED' });
    const file = this.storePathFor(place); if (!fs.existsSync(file)) throw Object.assign(new Error(`place file is not in the local store; run: node tools/fetch_places.js ${place.id}`), { code: 'NOT_IN_STORE' });
    const buf = fs.readFileSync(file); const sha = crypto.createHash('sha256').update(buf).digest('hex'); if (sha !== place.sha256) throw Object.assign(new Error('stored place file failed SHA-256 verification (quarantined)'), { code: 'HASH_MISMATCH' });
    const parsed = parsePlace(buf); const placemap = require('./placemap'); const serverId = crypto.randomBytes(8).toString('hex');
    const gs = new GameServer({ place: parsed, port: 0, logger: this.log, verifyTicket: t => this._verify(serverId, place.id, t), maxPlayers: 12, scripts: require('./config').runScripts,
      teleport: pid => placemap.resolve(pid, { storeHas: r => this.available({ sha256: r.sha256 }) }),
      onTeleport: (uid, r) => { try { this.db.prepare('INSERT INTO teleports(user_id,from_place,to_place,roblox_place_id,created_at) VALUES(?,?,?,?,?)').run(uid, place.id, r.local, r.robloxPlaceId, Date.now()); } catch (e) { this.log('teleport record failed: ' + e.message); } } });
    await gs.listen(); const rec = { serverId, place: place.id, gs, port: gs.port, startedAt: Date.now(), worldStats: gs.world.stats }; this.servers.set(place.id, rec); return rec;
  }
  _verify(serverId, placeId, ticket) {
    const r = auth.redeemTicket(this.db, 'join_tokens', ticket, { serverId, placeId }); if (!r) return null; const u = services.userById(this.db, r.user_id); if (!u) return null;
    return { userId: u.id, username: u.username, avatar: services.avatarForGame(this.db, u.id), placeId };
  }
  closeAll() { for (const s of this.servers.values()) s.gs.close(); this.servers.clear(); }
}
module.exports = { GameServerManager };
