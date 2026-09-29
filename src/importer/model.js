'use strict';
// Format-neutral in-memory representation of a parsed place. Scripts are INERT DATA here.
class Instance {
  constructor(className, referent) {
    this.className = className; this.referent = referent;
    this.props = new Map();       // name -> {type, value}
    this.children = []; this.parent = null;
  }
  get name() { const p = this.props.get('Name'); return p ? String(p.value) : this.className; }
  addChild(c) { c.parent = this; this.children.push(c); }
  *descendants() { for (const c of this.children) { yield c; yield* c.descendants(); } }
}
class Place {
  constructor() { this.roots = []; this.byReferent = new Map(); this.format = null; this.meta = {}; this.warnings = []; }
  *all() { for (const r of this.roots) { yield r; yield* r.descendants(); } }
  count() { let n = 0; for (const _ of this.all()) n++; return n; }
}
module.exports = { Instance, Place };
