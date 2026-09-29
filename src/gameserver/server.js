'use strict';
// BLOXEN game server: RakNet-style transport + bloxen-profile-v0 protocol + World. Loopback-only by default.
// STATUS: SIMULATOR-TESTED. NOT REAL-CLIENT-TESTED (see docs/WINDOWS-REAL-CLIENT-VALIDATION.md).
const fs = require('fs'); const { RakServer, REL } = require('./raknet'); const P = require('./protocol'); const { World } = require('./world'); const schemaMod = require('./schema');
const MAX_PKT_PER_SEC = 400; const BATCH_INSTANCES = 150;

class GameServer {
  constructor({ world, place, placeInfo, port = 53640, host = '127.0.0.1', allowRemote = false, verifyTicket, logger = () => {}, capturePath = null, respawnSeconds = 5, loadAsset = null, maxPlayers = 12, scripts = false } = {}) {
    if (typeof verifyTicket !== 'function') throw new Error('GameServer requires verifyTicket(ticket) -> {userId, username, avatar, placeId} | null');
    this.world = world || new World({ place, placeInfo, respawnSeconds, loadAsset }); this.log = logger; this.verifyTicket = verifyTicket; this.maxPlayers = maxPlayers;
    let cap = null; if (capturePath) { const fd = fs.openSync(capturePath, 'a'); cap = (r, m) => fs.writeSync(fd, JSON.stringify({ t: new Date().toISOString(), from: r.address + ':' + r.port, len: m.length, hex: m.subarray(0, 512).toString('hex') }) + '\n'); } // raw UDP capture for profile calibration
    this.rak = new RakServer({ host, port, allowRemote, capture: cap }); this.peers = new Map(); this.stats = { connections: 0, refused: 0, protocolMismatch: 0, badPackets: 0, moves: 0, movesRejected: 0 };
    this.rak.on('data', (p, pk) => this._onData(p, pk)); this.rak.on('disconnect', p => this._drop(p));
    this.unsub = this.world.onChange(ev => this._feed(ev)); this.flushTimer = setInterval(() => this._flush(), 40); this.flushTimer.unref();
    this.scriptHost = null; if (scripts) { const { ScriptHost } = require('../script/engine'); this.scriptHost = new ScriptHost(this.world, { logger: m => this.log(m) }); }
    this.descriptors = P.encodeDescriptors(this.world.schema); this.globalsBuf = null;
  }
  async listen() { await this.rak.listen(); this.port = this.rak.port; if (this.scriptHost) { this.scriptHost.start(); /* scripts run to their first wait() before any client can join */ this.scriptHost.step(0); let last = Date.now(); this.scriptTimer = setInterval(() => { const n = Date.now(); this.scriptHost.step(Math.min((n - last) / 1000, 1)); last = n; }, 50); this.scriptTimer.unref(); } this.log('game server listening on ' + this.rak.host + ':' + this.port); return this; }
  close() { clearInterval(this.flushTimer); clearInterval(this.scriptTimer); if (this.scriptHost) this.scriptHost.close(); this.unsub(); for (const s of this.peers.values()) if (s.rec) this.world.removePlayer(s.rec.userId); this.rak.close(); }
  _state(p) { let s = this.peers.get(p.key); if (!s) { s = { p, stage: 'new', queue: [], errors: 0, winStart: Date.now(), count: 0 }; this.peers.set(p.key, s); this.stats.connections++; } return s; }
  _drop(p) { const s = this.peers.get(p.key); if (s && s.rec) this.world.removePlayer(s.rec.userId); this.peers.delete(p.key); }
  _kick(s, why) { this.log('kick ' + s.p.key + ': ' + why); try { this.rak.send(s.p, Buffer.from([0x15])); } catch {} s.p.session.close(); this.rak.peers.delete(s.p.key); this._drop(s.p); }
  _onData(p, pk) {
    const s = this._state(p); const now = Date.now(); if (now - s.winStart > 1000) { s.winStart = now; s.count = 0; } if (++s.count > MAX_PKT_PER_SEC) return this._kick(s, 'flood');
    try { this._handle(s, pk); } catch (e) { this.stats.badPackets++; this.log('bad packet from ' + p.key + ': ' + e.message); if (++s.errors > 5) this._kick(s, 'too many malformed packets'); }
  }
  _handle(s, pk) {
    const id = pk[0];
    if (s.stage === 'new') {
      if (id !== P.PKT.PROTOCOL_SYNC) throw new Error('expected PROTOCOL_SYNC');
      const { version, ticket } = P.decodeProtocolSync(pk);
      if (version !== P.PROTOCOL_VERSION) { this.stats.protocolMismatch++; this.rak.send(s.p, P.encodeMismatch()); return this._kick(s, 'protocol mismatch ' + version); }
      if (this.world.players.size >= this.maxPlayers) { this.rak.send(s.p, P.encodeRefused('Server is full')); return this._kick(s, 'full'); }
      const who = this.verifyTicket(ticket); if (!who) { this.stats.refused++; this.rak.send(s.p, P.encodeRefused('Invalid or expired join ticket')); return this._kick(s, 'bad ticket'); }
      if (this.world.players.has(who.userId)) { this.rak.send(s.p, P.encodeRefused('Already connected')); return this._kick(s, 'duplicate user'); }
      s.who = who; s.stage = 'globals'; this.rak.send(s.p, this.descriptors); this.rak.send(s.p, P.encodeGlobals(this.world)); return;
    }
    if (id !== P.PKT.ID_DATA) throw new Error('unexpected packet 0x' + id.toString(16));
    const ops = P.decodeBatch(pk, this.world.schema, iid => this.world.byId.get(iid)?.className);
    for (const op of ops) {
      if (op.op === 'ready' && s.stage === 'globals') { this._join(s); continue; }
      if (s.stage !== 'ingame') continue;
      if (op.op === 'move') { const mine = s.rec.parts.HumanoidRootPart; if (op.id !== mine.id) { this.stats.movesRejected++; continue; } const r = this.world.applyMove(s.rec, op.cf); this.stats.moves++; if (!r.ok) { this.stats.movesRejected++; this._corrective(s, r); } }
      else if (op.op === 'reset') this.world.kill(s.rec);
      // anything else from a client is ignored (clients are not authoritative over instances)
    }
  }
  _corrective(s, r) { const b = P.newBatch(); const root = s.rec.parts.HumanoidRootPart; P.writeProp(b, this.world, root, 'CFrame'); this.rak.send(s.p, P.endBatch(b)); }
  _join(s) {
    const { userId, username, avatar } = s.who; const rec = this.world.addPlayer({ userId, username, avatar }); s.rec = rec; // emits change events to existing ingame peers only
    const w = this.world; const order = []; for (const t of w.topContainers()) for (const i of w.subtree(t)) order.push(i);
    for (let i = 0; i < order.length; i += BATCH_INSTANCES) { const b = P.newBatch(); for (const inst of order.slice(i, i + BATCH_INSTANCES)) P.writeNew(b, w, inst); this.rak.send(s.p, P.endBatch(b)); }
    const b = P.newBatch(); b.writeU8(P.OP.LOCAL_PLAYER); b.writeCompressed(rec.player.id, 4); this.rak.send(s.p, P.endBatch(b)); s.stage = 'ingame'; this.log(`${username} (${userId}) joined; ${order.length} instances sent; missing assets: ${rec.missingAssets.length}`);
  }
  _feed(ev) { for (const s of this.peers.values()) if (s.stage === 'ingame') { s.queue.push(ev); } }
  _flush() {
    for (const s of this.peers.values()) {
      if (!s.queue.length) continue; const q = s.queue; s.queue = []; let b = P.newBatch(); let n = 0;
      const send = () => { if (n) this.rak.send(s.p, P.endBatch(b)); b = P.newBatch(); n = 0; };
      for (const ev of q) {
        if (ev.op === 'new') { if (!this.world.byId.has(ev.inst.id)) continue; P.writeNew(b, this.world, ev.inst); n++; }
        else if (ev.op === 'prop') { if (!this.world.byId.has(ev.inst.id)) continue; if (P.writeProp(b, this.world, ev.inst, ev.name)) n++; }
        else if (ev.op === 'del') { P.writeDel(b, ev.id); n++; }
        if (n >= 60) send();
      }
      send();
    }
  }
}
module.exports = { GameServer };
