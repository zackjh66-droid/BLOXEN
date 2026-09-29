'use strict';
// Descriptor tables derived from the 0.205.0.61876 API dump (preservation/reference/api-0.205.0.61876.json, RobloxAPI/build-archive).
//  * classId  = index of class name in the alphabetical (sorted) list of the 332 classes. Corroborated by ONE real-client observation:
//               ReplicatedFirst -> 231 (see research/PROTOCOL.md).  Other ids: INFERRED from that rule.
//  * propertyId (per class) = INFERRED. The client has 968 properties vs 851 in the public dump; hidden members are unknown, so ids are NOT
//               expected to match the client. `OBSERVED_CLIENT_COUNTS` is used to report the gap explicitly rather than hide it.
const fs = require('fs'); const path = require('path');
const DUMP = path.join(__dirname, '..', '..', 'preservation', 'reference', 'api-0.205.0.61876.json');
const OBSERVED_CLIENT_COUNTS = Object.freeze({ classes: 332, properties: 968, events: 320, types: 182 }); // previous independent real-client run
const OBSERVED_REPLICATED_FIRST_ID = 231;

const HIDDEN_SERIALIZED = { BasePart: [['size', 'Vector3', 'Vector3'], ['BrickColor', 'BrickColor', 'BrickColor']], FormFactorPart: [['formFactor', 'FormFactor', 'enum'], ['FormFactor', 'FormFactor', 'enum']], Part: [['shape', 'PartType', 'enum']] };
let cache = null;
function load(file = DUMP) {
  if (cache && file === DUMP) return cache;
  const d = JSON.parse(fs.readFileSync(file, 'utf8'));
  const names = Object.keys(d.classes).sort();
  const classId = new Map(names.map((n, i) => [n, i]));
  const enums = Object.keys(d.enums || {}).sort(); const enumNames = new Set(enums);
  const primitive = new Set(['bool', 'int', 'float', 'double', 'string', 'Object', 'Content', 'Color3', 'BrickColor', 'Vector3', 'Vector2', 'CFrame', 'CoordinateFrame', 'UDim2', 'ProtectedString', 'BinaryString', 'Vector2int16']);
  const chain = n => { const out = []; for (let c = n; c && d.classes[c]; c = d.classes[c].super) out.unshift(c); return out; };
  const classes = new Map(); const types = new Set();
  for (const n of names) {
    const props = []; // replicated properties, super-most class first, alphabetical within class (INFERRED ordering)
    for (const c of chain(n)) for (const p of [...d.classes[c].props].sort((a, b) => a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0)) {
      const [pn, pt, , tags, canSave] = p; types.add(pt); if (tags.includes('NotReplicated') && !canSave) continue; // canSave props (Friction, Elasticity) appear in place files, so they must travel
      props.push({ name: pn, type: pt, kind: primitive.has(pt) ? pt : enumNames.has(pt) ? 'enum' : 'unknown', declaredIn: c, tags });
    }
    // INFERRED: internal serialization-name properties that the public dump marks NotReplicated or omits (size, shape, formFactor, BrickColor).
    // Hypothesis: these are part of the client's 968-property table (117 more than the dump). UNVERIFIED.
    for (const c of chain(n)) for (const [pn, pt, kind] of HIDDEN_SERIALIZED[c] || []) props.push({ name: pn, type: pt, kind, declaredIn: c, tags: ['InferredHidden'], inferred: true });
    const seen = new Set(); for (let i = props.length - 1; i >= 0; i--) { if (seen.has(props[i].name)) props.splice(i, 1); else seen.add(props[i].name); } // inferred alias wins only if the dump has no same-named entry
    props.forEach((p, i) => { p.id = i; });
    classes.set(n, { name: n, id: classId.get(n), super: d.classes[n].super === '<<<ROOT>>>' ? null : d.classes[n].super, tags: d.classes[n].tags, props, byName: new Map(props.map(p => [p.name, p])) });
  }
  const total = { classes: names.length, properties: names.reduce((s, n) => s + d.classes[n].props.length, 0), events: names.reduce((s, n) => s + d.classes[n].events, 0), types: types.size, enums: enums.length };
  const s = { names, classId, classes, enums, total, byId: names };
  if (file === DUMP) cache = s; return s;
}
function isA(schema, cls, base) { for (let c = cls; c; c = schema.classes.get(c)?.super) if (c === base) return true; return false; }
function report(s = load()) {
  const gaps = {}; for (const k of Object.keys(OBSERVED_CLIENT_COUNTS)) gaps[k] = { dump: s.total[k], client: OBSERVED_CLIENT_COUNTS[k], match: s.total[k] === OBSERVED_CLIENT_COUNTS[k] };
  return { replicatedFirstId: s.classId.get('ReplicatedFirst'), replicatedFirstMatchesClient: s.classId.get('ReplicatedFirst') === OBSERVED_REPLICATED_FIRST_ID, gaps };
}
module.exports = { load, isA, report, OBSERVED_CLIENT_COUNTS, OBSERVED_REPLICATED_FIRST_ID };
