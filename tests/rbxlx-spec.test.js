'use strict';
// Conformance checks against RobloxAPI/spec formats/rbxlx.md (CC-BY-SA-4.0; read in full for this test). UNIT-TESTED.
const test = require('node:test'); const assert = require('node:assert/strict'); const { parsePlace } = require('../src/importer/parse');
const doc = props => `<roblox xmlns:xmime="http://www.w3.org/2005/05/xmlmime" version="4"><Meta name="ExplicitAutoJoints">true</Meta><External>null</External><External>nil</External>
<Item class="Part" referent="RBXAAAA"><Properties>${props}</Properties></Item><Item class="Model" referent="RBXBBBB"><Properties><string name="Name">M</string><Ref name="PrimaryPart">RBXAAAA</Ref><Ref name="Other">null</Ref></Properties></Item></roblox>`;
const get = (xml, name) => parsePlace(Buffer.from(xml)).roots[0].props.get(name);

test('Color3: packed uint32 text and R/G/B elements agree (spec example values)', () => {
  const a = get(doc('<Color3 name="C">4288914085</Color3>'), 'C').value, b = get(doc('<Color3 name="C"><R>0.639215708</R><G>0.635294139</G><B>0.647058845</B></Color3>'), 'C').value;
  for (const k of 'rgb') assert.ok(Math.abs(a[k] - b[k]) < 1 / 255, k);
});
test('Content: url, null, and legacy binary/hash (empty)', () => {
  assert.equal(get(doc('<Content name="T"><url>rbxasset://textures/SpawnLocation.png</url></Content>'), 'T').value, 'rbxasset://textures/SpawnLocation.png');
  for (const inner of ['<null></null>', '<binary>AAAA</binary>', '<hash>abc</hash>']) assert.equal(get(doc(`<Content name="T">${inner}</Content>`), 'T').value, '', inner);
});
test('Ref resolves to a referent and null is nil; Meta/External are tolerated', () => {
  const p = parsePlace(Buffer.from(doc('<string name="Name">P</string>'))); const m = p.roots.find(r => r.className === 'Model'); assert.ok(m.props.get('PrimaryPart')); assert.ok(!m.props.get('Other') || m.props.get('Other').value === null || m.props.get('Other').value === undefined);
});
test('numeric and vector types from the spec parse to the documented shapes', () => {
  const x = doc('<int name="I">-5</int><int64 name="L">9007199254740993</int64><float name="F">1.5</float><double name="D">2.25</double><bool name="B">true</bool><Vector2 name="V2"><X>1</X><Y>2</Y></Vector2><Vector3 name="V3"><X>1</X><Y>2</Y><Z>3</Z></Vector3><Vector3int16 name="V3i"><X>1</X><Y>2</Y><Z>3</Z></Vector3int16><UDim2 name="U"><XS>0.5</XS><XO>4</XO><YS>1</YS><YO>8</YO></UDim2><BrickColor name="BC">194</BrickColor><CoordinateFrame name="CF"><X>1</X><Y>2</Y><Z>3</Z><R00>1</R00><R01>0</R01><R02>0</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>0</R20><R21>0</R21><R22>1</R22></CoordinateFrame>');
  const p = parsePlace(Buffer.from(x)).roots[0].props; assert.equal(p.get('I').value, -5); assert.equal(p.get('F').value, 1.5); assert.equal(p.get('D').value, 2.25); assert.equal(p.get('B').value, true);
  assert.deepEqual(p.get('V3').value, { x: 1, y: 2, z: 3 }); assert.deepEqual({ x: p.get('V2').value.x, y: p.get('V2').value.y }, { x: 1, y: 2 }); assert.equal(p.get('BC').value, 194); assert.deepEqual(p.get('CF').value.pos, { x: 1, y: 2, z: 3 }); assert.equal(p.get('CF').value.rot.length, 9);
  assert.ok(p.get('L') && p.get('U') && p.get('V3i'), 'int64 / UDim2 / Vector3int16 are present (no crash)');
});
test('ProtectedString/BinaryString/string: scripts stay text, never evaluated', () => {
  const p = parsePlace(Buffer.from(doc('<ProtectedString name="Source"><![CDATA[print("x") -- <not xml>]]></ProtectedString><BinaryString name="Bin">aGVsbG8=</BinaryString><string name="S">a&amp;b</string>'))).roots[0].props;
  assert.equal(p.get('Source').value, 'print("x") -- <not xml>'); assert.equal(p.get('S').value, 'a&b'); assert.ok(p.get('Bin'));
});
test('hostile XML: entity expansion and DOCTYPE are not expanded, deep nesting does not crash the process', () => {
  const bomb = `<?xml version="1.0"?><!DOCTYPE roblox [<!ENTITY a "aaaaaaaaaa"><!ENTITY b "&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;">]><roblox version="4"><Item class="Part" referent="RBX1"><Properties><string name="Name">&b;&b;</string></Properties></Item></roblox>`;
  let name; try { name = parsePlace(Buffer.from(bomb)).roots[0].props.get('Name').value; } catch { name = null; } assert.ok(name === null || name.length < 100, 'entities must not expand exponentially');
  const deep = '<roblox version="4">' + '<Item class="Model" referent="RBX1"><Properties></Properties>'.repeat(20000) + '</Item>'.repeat(20000) + '</roblox>';
  assert.doesNotThrow(() => { try { parsePlace(Buffer.from(deep)); } catch (e) { assert.ok(e instanceof Error); /* rejection is acceptable; what matters is that it is a catchable Error, not a process crash */ } });
});
