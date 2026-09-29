'use strict';
// RakNet-style transport, profile "raknet-framing-v0".
// STATUS: SIMULATOR-TESTED / UNIT-TESTED only. The genuine 0.205.0.61876 client embeds RakNet 3.x (see research/PROTOCOL.md);
// the datagram/frame bit layout below follows the widely-documented RakNet 4.x framing and has NOT been validated against that client.
// Everything above (protocol.js, schema.js, world.js) is independent of this layer so it can be re-profiled after a Windows capture.
const dgram = require('dgram'); const crypto = require('crypto'); const EventEmitter = require('events');

const MAGIC = Buffer.from('00ffff00fefefefefdfdfdfd12345678', 'hex');
const ID = { PING: 0x01, PONG: 0x1c, REQ1: 0x05, REP1: 0x06, REQ2: 0x07, REP2: 0x08, CONN_REQ: 0x09, CONN_ACC: 0x10, NEW_INC: 0x13, C_PING: 0x00, C_PONG: 0x03, DISC: 0x15, ACK: 0xc0, NACK: 0xa0 };
const REL = { UNRELIABLE: 0, RELIABLE: 2, RELIABLE_ORDERED: 3 };
const RAK_PROTOCOL = 5; const DEFAULT_MTU = 1200; const MAX_SPLIT = 4096;
const u24 = { read: (b, o) => b[o] | (b[o + 1] << 8) | (b[o + 2] << 16), write: (b, v, o) => { b[o] = v & 255; b[o + 1] = (v >> 8) & 255; b[o + 2] = (v >> 16) & 255; } };

class Session extends EventEmitter {
  constructor(send, mtu = DEFAULT_MTU) {
    super(); this.send = send; this.mtu = mtu; this.seq = 0; this.relIdx = 0; this.ordIdx = 0; this.splitId = 0;
    this.unacked = new Map(); this.recvSeen = new Set(); this.ackQueue = []; this.nackQueue = []; this.expectSeq = 0;
    this.orderExpect = 0; this.orderBuf = new Map(); this.splits = new Map(); this.relSeen = new Set(); this.lastRecv = Date.now(); this.closed = false;
    this.timer = setInterval(() => this.tick(), 50); this.timer.unref();
  }
  close() { this.closed = true; clearInterval(this.timer); }
  sendPacket(payload, reliability = REL.RELIABLE_ORDERED) {
    const maxFrame = this.mtu - 60;
    if (payload.length <= maxFrame) return this._queueFrame(payload, reliability, null);
    const n = Math.ceil(payload.length / maxFrame); if (n > MAX_SPLIT) throw new Error('packet too large'); const id = this.splitId++ & 0xffff;
    const ord = reliability === REL.RELIABLE_ORDERED ? this.ordIdx++ : 0;
    for (let i = 0; i < n; i++) this._queueFrame(payload.subarray(i * maxFrame, (i + 1) * maxFrame), reliability === REL.UNRELIABLE ? REL.RELIABLE : reliability, { count: n, id, index: i }, ord);
  }
  _queueFrame(payload, reliability, split, forcedOrd) {
    const hdr = []; const reliable = reliability >= 2; const ordered = reliability === REL.RELIABLE_ORDERED;
    const flags = (reliability << 5) | (split ? 0x10 : 0);
    const parts = [Buffer.from([flags]), Buffer.from([(payload.length * 8) >> 8, (payload.length * 8) & 255])];
    if (reliable) { const b = Buffer.alloc(3); u24.write(b, this.relIdx++, 0); parts.push(b); }
    if (ordered) { const b = Buffer.alloc(4); u24.write(b, split ? forcedOrd : this.ordIdx++, 0); b[3] = 0; parts.push(b); }
    if (split) { const b = Buffer.alloc(10); b.writeUInt32BE(split.count, 0); b.writeUInt16BE(split.id, 4); b.writeUInt32BE(split.index, 6); parts.push(b); }
    parts.push(payload); const frame = Buffer.concat(parts);
    const dg = this._dgram(frame); this._transmit(dg, reliable);
  }
  _dgram(frames) { const seq = this.seq++; const h = Buffer.alloc(4); h[0] = 0x84; u24.write(h, seq, 1); return { seq, buf: Buffer.concat([h, frames]) }; }
  _transmit(dg, track) { if (track) this.unacked.set(dg.seq, { dg, at: Date.now(), tries: 0 }); this.send(dg.buf); }
  tick() {
    if (this.closed) return; const now = Date.now();
    if (this.ackQueue.length) { this.send(this._ackPacket(ID.ACK, this.ackQueue)); this.ackQueue = []; }
    if (this.nackQueue.length) { this.send(this._ackPacket(ID.NACK, this.nackQueue)); this.nackQueue = []; }
    for (const [seq, u] of this.unacked) if (now - u.at > 300) { if (++u.tries > 40) { this.close(); this.emit('timeout'); return; } u.at = now; this.send(u.dg.buf); }
    if (now - this.lastRecv > 15000) { this.close(); this.emit('timeout'); }
  }
  _ackPacket(type, seqs) {
    const sorted = [...new Set(seqs)].sort((a, b) => a - b); const ranges = []; let s = sorted[0], p = s;
    for (let i = 1; i <= sorted.length; i++) { if (i < sorted.length && sorted[i] === p + 1) { p = sorted[i]; continue; } ranges.push([s, p]); s = sorted[i]; p = s; }
    const parts = [Buffer.from([type, (ranges.length >> 8) & 255, ranges.length & 255])];
    for (const [a, b] of ranges) { const r = Buffer.alloc(a === b ? 4 : 7); r[0] = a === b ? 1 : 0; u24.write(r, a, 1); if (a !== b) u24.write(r, b, 4); parts.push(r); }
    return Buffer.concat(parts);
  }
  receive(buf) {
    this.lastRecv = Date.now(); const t = buf[0];
    if (t === ID.ACK || t === ID.NACK) {
      const n = buf.readUInt16BE(1); let o = 3;
      for (let i = 0; i < n; i++) { const single = buf[o]; const a = u24.read(buf, o + 1); const b = single ? a : u24.read(buf, o + 4); o += single ? 4 : 7;
        if (b - a > 100000) return;
        for (let s = a; s <= b; s++) { if (t === ID.ACK) this.unacked.delete(s); else { const u = this.unacked.get(s); if (u) this.send(u.dg.buf); } } }
      return;
    }
    if (!(t & 0x80)) return;
    const seq = u24.read(buf, 1); this.ackQueue.push(seq);
    if (this.recvSeen.has(seq)) return; this.recvSeen.add(seq); if (this.recvSeen.size > 8192) this.recvSeen.delete(this.recvSeen.values().next().value);
    for (let m = this.expectSeq; m < seq; m++) if (!this.recvSeen.has(m)) this.nackQueue.push(m); if (seq >= this.expectSeq) this.expectSeq = seq + 1;
    let o = 4;
    while (o < buf.length) {
      const flags = buf[o++]; const rel = flags >> 5; const isSplit = !!(flags & 0x10); const len = Math.ceil(buf.readUInt16BE(o) / 8); o += 2;
      let relIdx = -1, ordIdx = -1; if (rel >= 2) { relIdx = u24.read(buf, o); o += 3; } if (rel === 3) { ordIdx = u24.read(buf, o); o += 4; }
      let split = null; if (isSplit) { split = { count: buf.readUInt32BE(o), id: buf.readUInt16BE(o + 4), index: buf.readUInt32BE(o + 6) }; o += 10; }
      if (o + len > buf.length) return; const payload = buf.subarray(o, o + len); o += len;
      if (relIdx >= 0) { if (this.relSeen.has(relIdx)) continue; this.relSeen.add(relIdx); if (this.relSeen.size > 8192) this.relSeen.delete(this.relSeen.values().next().value); }
      let full = payload;
      if (split) { if (split.count > MAX_SPLIT || split.index >= split.count) continue; let e = this.splits.get(split.id); if (!e) { e = { parts: new Array(split.count), got: 0 }; this.splits.set(split.id, e); }
        if (!e.parts[split.index]) { e.parts[split.index] = Buffer.from(payload); e.got++; } if (e.got < split.count) continue; full = Buffer.concat(e.parts); this.splits.delete(split.id); }
      if (rel === 3) { if (ordIdx < this.orderExpect) continue; this.orderBuf.set(ordIdx, full); while (this.orderBuf.has(this.orderExpect)) { const p = this.orderBuf.get(this.orderExpect); this.orderBuf.delete(this.orderExpect++); this.emit('packet', p); } }
      else this.emit('packet', full);
    }
  }
}

function rand64() { return crypto.randomBytes(8); }
function writeAddr(ip, port) { const b = Buffer.alloc(7); b[0] = 4; ip.split('.').forEach((x, i) => b[1 + i] = (~Number(x)) & 255); b.writeUInt16BE(port, 5); return b; }

class RakServer extends EventEmitter {
  constructor({ host = '127.0.0.1', port = 53640, allowRemote = false, maxPeers = 64, capture = null } = {}) {
    super(); if (host !== '127.0.0.1' && host !== '::1' && !allowRemote) throw new Error('refusing to bind game server to a non-loopback address without allowRemote');
    this.host = host; this.port = port; this.guid = rand64(); this.peers = new Map(); this.maxPeers = maxPeers; this.capture = capture;
    this.sock = dgram.createSocket('udp4'); this.sock.on('message', (m, r) => this._onMsg(m, r)); this.sock.on('error', e => this.emit('error', e));
  }
  listen() { return new Promise(res => this.sock.bind(this.port, this.host, () => { this.port = this.sock.address().port; res(this); })); }
  close() { for (const p of this.peers.values()) p.session.close(); this.sock.close(); }
  _onMsg(m, r) {
    if (this.capture) this.capture(r, m);
    const key = r.address + ':' + r.port; const peer = this.peers.get(key);
    if (peer && peer.connected !== undefined && !(m[0] === ID.REQ1 || m[0] === ID.REQ2 || m[0] === ID.PING)) return this._connected(peer, m);
    const t = m[0];
    if (t === ID.PING && m.length >= 25 && m.subarray(9, 25).equals(MAGIC)) { const name = Buffer.from('BLOXEN'); const b = Buffer.alloc(1 + 8 + 8 + 16 + 2 + name.length); b[0] = ID.PONG; m.copy(b, 1, 1, 9); this.guid.copy(b, 9); MAGIC.copy(b, 17); b.writeUInt16BE(name.length, 33); name.copy(b, 35); this.sock.send(b, r.port, r.address); return; }
    if (t === ID.REQ1 && m.subarray(1, 17).equals(MAGIC)) { if (m[17] !== RAK_PROTOCOL) return; const mtu = Math.min(m.length + 28, 1400); const b = Buffer.alloc(1 + 16 + 8 + 1 + 2); b[0] = ID.REP1; MAGIC.copy(b, 1); this.guid.copy(b, 17); b[25] = 0; b.writeUInt16BE(mtu, 26); this.sock.send(b, r.port, r.address); return; }
    if (t === ID.REQ2 && m.subarray(1, 17).equals(MAGIC)) {
      if (this.peers.size >= this.maxPeers) return; const mtu = Math.min(m.readUInt16BE(24), 1400); const clientGuid = m.subarray(26, 34);
      const b = Buffer.alloc(1 + 16 + 8 + 7 + 2 + 1); b[0] = ID.REP2; MAGIC.copy(b, 1); this.guid.copy(b, 17); writeAddr(r.address, r.port).copy(b, 25); b.writeUInt16BE(mtu, 32); b[34] = 0;
      const session = new Session(buf => this.sock.send(buf, r.port, r.address), mtu); const p = { key, addr: r, session, guid: clientGuid, connected: false };
      session.on('packet', pk => this._packet(p, pk)); session.on('timeout', () => { this.peers.delete(key); this.emit('disconnect', p); });
      this.peers.set(key, p); this.sock.send(b, r.port, r.address); return;
    }
  }
  _connected(p, m) { p.session.receive(m); }
  _packet(p, pk) {
    const t = pk[0];
    if (t === ID.CONN_REQ) { const b = Buffer.alloc(1 + 7 + 2 + 7 * 10 + 16); b[0] = ID.CONN_ACC; writeAddr(p.addr.address, p.addr.port).copy(b, 1); b.writeUInt16BE(0, 8); for (let i = 0; i < 10; i++) writeAddr('127.0.0.1', this.port).copy(b, 10 + i * 7); pk.copy(b, 80, 9, 17); p.session.sendPacket(b.subarray(0, 88)); return; }
    if (t === ID.NEW_INC) { p.connected = true; this.emit('connection', p); return; }
    if (t === ID.C_PING) { const b = Buffer.alloc(17); b[0] = ID.C_PONG; pk.copy(b, 1, 1, 9); b.writeBigUInt64BE(BigInt(Date.now()), 9); p.session.sendPacket(b, REL.UNRELIABLE); return; }
    if (t === ID.DISC) { p.session.close(); this.peers.delete(p.key); this.emit('disconnect', p); return; }
    if (t >= 0x80) this.emit('data', p, pk);
  }
  send(p, payload, rel) { p.session.sendPacket(payload, rel); }
}

class RakClient extends EventEmitter {
  constructor({ host = '127.0.0.1', port }) { super(); this.host = host; this.port = port; this.sock = dgram.createSocket('udp4'); this.guid = rand64(); this.state = 'idle'; this.sock.on('message', m => this._onMsg(m)); }
  connect() {
    return new Promise((resolve, reject) => {
      this.sock.bind(0, '127.0.0.1', () => { this.sock.unref();
        const req1 = Buffer.alloc(1 + 16 + 1 + 1100); req1[0] = ID.REQ1; MAGIC.copy(req1, 1); req1[17] = RAK_PROTOCOL; this.sock.send(req1, this.port, this.host); this.state = 'req1'; this._resolve = resolve;
        this._to = setTimeout(() => reject(new Error('connect timeout')), 5000);
      });
    });
  }
  _onMsg(m) {
    if (this.state === 'req1' && m[0] === ID.REP1) { const mtu = m.readUInt16BE(26); const b = Buffer.alloc(1 + 16 + 7 + 2 + 8); b[0] = ID.REQ2; MAGIC.copy(b, 1); writeAddr(this.host, this.port).copy(b, 17); b.writeUInt16BE(mtu, 24); this.guid.copy(b, 26); this.sock.send(b, this.port, this.host); this.state = 'req2'; this.mtu = mtu; return; }
    if (this.state === 'req2' && m[0] === ID.REP2) {
      this.session = new Session(buf => this.sock.send(buf, this.port, this.host), m.readUInt16BE(32)); this.session.on('packet', pk => this._packet(pk)); this.session.on('timeout', () => this.emit('close'));
      const b = Buffer.alloc(1 + 8 + 8 + 1); b[0] = ID.CONN_REQ; this.guid.copy(b, 1); b.writeBigUInt64BE(BigInt(Date.now()), 9); this.session.sendPacket(b); this.state = 'conn'; return;
    }
    if (this.session) this.session.receive(m);
  }
  _packet(pk) {
    if (pk[0] === ID.CONN_ACC) { const b = Buffer.alloc(1 + 7 + 7 * 10 + 16); b[0] = ID.NEW_INC; this.session.sendPacket(b.subarray(0, 8)); this.state = 'connected'; clearTimeout(this._to); this._resolve && this._resolve(this); return; }
    if (pk[0] >= 0x80) this.emit('data', pk);
  }
  send(payload, rel) { this.session.sendPacket(payload, rel); }
  close() { try { this.session && this.session.sendPacket(Buffer.from([ID.DISC])); } catch {} setTimeout(() => { this.session && this.session.close(); this.sock.close(); }, 100); }
}
module.exports = { RakServer, RakClient, Session, REL, ID, MAGIC };
