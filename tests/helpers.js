'use strict';
const { Place, Instance } = require('../src/importer/model');
// Synthetic minimal place built in code (NOT a historical place; used only to exercise the pipeline in unit tests).
function syntheticPlace() {
  const place = new Place(); let n = 0; const mk = (cls, props = {}) => { const i = new Instance(cls, 'RBX' + n++); for (const [k, v] of Object.entries(props)) i.props.set(k, v); place.byReferent.set(i.referent, i); return i; };
  const ws = mk('Workspace', { Name: { type: 'string', value: 'Workspace' } });
  const part = mk('Part', { Name: { type: 'string', value: 'Baseplate' }, size: { type: 'Vector3', value: { x: 512, y: 4, z: 512 } }, CFrame: { type: 'CFrame', value: { pos: { x: 0, y: -2, z: 0 }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] } }, Anchored: { type: 'bool', value: true }, BrickColor: { type: 'int', value: 28 } });
  const spawn = mk('SpawnLocation', { Name: { type: 'string', value: 'Spawn' }, size: { type: 'Vector3', value: { x: 6, y: 1, z: 6 } }, CFrame: { type: 'CFrame', value: { pos: { x: 10, y: 0.5, z: 20 }, rot: [1, 0, 0, 0, 1, 0, 0, 0, 1] } } });
  const script = mk('Script', { Name: { type: 'string', value: 'Die' }, Source: { type: 'ProtectedString', value: 'error("must never run")' } });
  ws.addChild(part); ws.addChild(spawn); ws.addChild(script); place.roots.push(ws); place.roots.push(mk('Lighting', {}));
  return place;
}
module.exports = { syntheticPlace };
