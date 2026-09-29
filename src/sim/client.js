'use strict';
// Protocol SIMULATOR client. It speaks bloxen-profile-v0 to the BLOXEN game server. A simulator success is SIMULATOR-TESTED only and says
// NOTHING about the real Roblox client. It is a test harness, never a substitute for the real-client gate.
const { RakClient } = require('../gameserver/raknet'); const P = require('../gameserver/protocol'); const schemaMod = require('../gameserver/schema'); const EventEmitter = require('events');

class SimClient extends EventEmitter {
  constructor({ port, host = '127.0.0.1', ticket, protocolVersion = P.PROTOCOL_VERSION, schema = schemaMod.load() }) {
    super(); this.port = port; this.host = host; this.ticket = ticket; this.version = protocolVersion; this.schema = schema;
    this.instances = new Map(); this.localPlayerId = null; this.globals = null; this.descriptors = null; this.log = []; this.state = 'new'; this.refusal = null; this.mismatch = false;
  }
  async connect() {
    this.rak = new RakClient({ host: this.host, port: this.port }); await this.rak.connect(); this.state = 'connected';
    this.rak.on('data', pk => { try { this._data(pk); } catch (e) { this.emit('error', e); this.log.push('error ' + e.message); } });
    this.rak.send(P.encodeProtocolSync(this.ticket, this.version));
  }
  _data(pk) {
    switch (pk[0]) {
      case P.PKT.TEACH_DESCRIPTOR_DICTIONARIES: this.descriptors = P.decodeDescriptors(pk); this.state = 'descriptors'; break;
      case P.PKT.SET_GLOBALS: this.globals = P.decodeGlobals(pk, this.schema); this.state = 'globals';
        for (const c of this.globals) this.instances.set(c.id, { id: c.id, className: c.className, parent: 0, props: { Name: c.name } });
        this.rak.send(P.encodeSimple(P.OP.CLIENT_READY)); break; // client-emitted ID_DATA (OBSERVED behaviour with the real client: it emits ID_DATA after SET_GLOBALS)
      case P.PKT.PROTOCOL_MISMATCH: this.mismatch = true; this.state = 'refused'; break;
      case P.PKT.JOIN_REFUSED: { const { BitStream } = require('../gameserver/bitstream'); const { readLegacyString } = require('../gameserver/huffman'); const bs = new BitStream(pk); bs.readU8(); this.refusal = readLegacyString(bs).toString(); this.state = 'refused'; break; }
      case P.PKT.ID_DATA: for (const op of P.decodeBatch(pk, this.schema, id => this.instances.get(id)?.className)) this._apply(op); break;
    }
  }
  _apply(op) {
    if (op.op === 'new') this.instances.set(op.id, { id: op.id, className: op.className, parent: op.parent, props: op.props });
    else if (op.op === 'prop') { const i = this.instances.get(op.id); if (i) i.props[op.name] = op.value; }
    else if (op.op === 'del') { const drop = id => { this.instances.delete(id); for (const i of [...this.instances.values()]) if (i.parent === id) drop(i.id); }; drop(op.id); }
    else if (op.op === 'localPlayer') { this.localPlayerId = op.id; this.state = 'ingame'; this.emit('ingame'); }
    this.emit('op', op);
  }
  get player() { return this.instances.get(this.localPlayerId); }
  get character() { const p = this.player; return p && this.instances.get(p.props.Character); }
  part(name) { const c = this.character; if (!c) return null; return [...this.instances.values()].find(i => i.parent === c.id && i.props.Name === name) || null; }
  move(cf) { const root = this.part('HumanoidRootPart'); this.rak.send(P.encodeMove(root.id, cf)); }
  reset() { this.rak.send(P.encodeSimple(P.OP.RESET)); }
  waitFor(pred, ms = 5000) { return new Promise((res, rej) => { const t0 = Date.now(); const iv = setInterval(() => { try { if (pred()) { clearInterval(iv); res(true); } else if (Date.now() - t0 > ms) { clearInterval(iv); rej(new Error('waitFor timeout; state=' + this.state)); } } catch (e) { clearInterval(iv); rej(e); } }, 20); }); }
  close() { this.rak.close(); }
}
module.exports = { SimClient };
