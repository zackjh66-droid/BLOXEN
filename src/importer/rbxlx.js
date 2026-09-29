'use strict';
// Static XML Roblox place parser (.rbxlx, and the XML flavour historically saved as .rbxl).
// Hand-written, dependency-free, tolerant tokenizer. No DTD/entity expansion beyond the 5 XML entities + numeric refs
// (no external entities => no XXE). Scripts are kept as inert strings.
const { Instance, Place } = require('./model');

const ENT = { lt: '<', gt: '>', amp: '&', quot: '"', apos: "'" };
function unescapeXml(s) {
  if (s.indexOf('&') < 0) return s;
  return s.replace(/&(#x[0-9a-fA-F]+|#[0-9]+|[a-zA-Z]+);/g, (m, e) => {
    if (e[0] === '#') { const cp = e[1] === 'x' ? parseInt(e.slice(2), 16) : parseInt(e.slice(1), 10); try { return String.fromCodePoint(cp); } catch { return ''; } }
    return ENT[e] !== undefined ? ENT[e] : m;
  });
}
// Streaming tokenizer -> callbacks
function tokenize(str, on) {
  let i = 0; const n = str.length;
  while (i < n) {
    const lt = str.indexOf('<', i);
    if (lt < 0) { on.text(str.slice(i)); break; }
    if (lt > i) on.text(str.slice(i, lt));
    if (str.startsWith('<![CDATA[', lt)) { const e = str.indexOf(']]>', lt); if (e < 0) throw new Error('xml: unterminated CDATA'); on.cdata(str.slice(lt + 9, e)); i = e + 3; continue; }
    if (str.startsWith('<!--', lt)) { const e = str.indexOf('-->', lt); if (e < 0) throw new Error('xml: unterminated comment'); i = e + 3; continue; }
    if (str[lt + 1] === '?' || str[lt + 1] === '!') { const e = str.indexOf('>', lt); if (e < 0) throw new Error('xml: bad decl'); i = e + 1; continue; }
    if (str[lt + 1] === '/') { const e = str.indexOf('>', lt); on.close(str.slice(lt + 2, e).trim()); i = e + 1; continue; }
    // open tag
    let j = lt + 1; let inQ = null;
    while (j < n) { const c = str[j]; if (inQ) { if (c === inQ) inQ = null; } else if (c === '"' || c === "'") inQ = c; else if (c === '>') break; j++; }
    if (j >= n) throw new Error('xml: unterminated tag');
    let body = str.slice(lt + 1, j); let self = false;
    if (body.endsWith('/')) { self = true; body = body.slice(0, -1); }
    const sp = body.search(/\s/); const name = sp < 0 ? body : body.slice(0, sp); const attrs = {};
    if (sp >= 0) { const re = /([^\s=]+)\s*=\s*("([^"]*)"|'([^']*)')/g; let m; const rest = body.slice(sp); while ((m = re.exec(rest))) attrs[m[1]] = unescapeXml(m[3] !== undefined ? m[3] : m[4]); }
    on.open(name, attrs); if (self) on.close(name);
    i = j + 1;
  }
}

function num(s) { const v = Number(String(s).trim()); return Number.isFinite(v) ? v : (String(s).trim().toLowerCase() === 'inf' ? Infinity : 0); }
function convert(tag, node) {
  // node: {text, kids:{name:text...}}
  const k = node.kids, t = node.text;
  switch (tag) {
    case 'string': case 'ProtectedString': return { type: tag === 'ProtectedString' ? 'ProtectedString' : 'string', value: t };
    case 'BinaryString': return { type: 'BinaryString', value: t.replace(/\s+/g, '') };
    case 'bool': return { type: 'bool', value: t.trim() === 'true' };
    case 'int': case 'token': case 'BrickColor': return { type: tag === 'int' ? 'int' : tag, value: Math.trunc(num(t)) };
    case 'int64': return { type: 'int64', value: t.trim() };
    case 'float': case 'double': return { type: tag, value: num(t) };
    case 'Content': return { type: 'Content', value: (k.url !== undefined ? k.url : ((k.null !== undefined || k.binary !== undefined || k.hash !== undefined) ? '' : t)).trim() }; // spec: binary/hash are legacy and mean empty
    case 'Vector3': return { type: 'Vector3', value: { x: num(k.X), y: num(k.Y), z: num(k.Z) } };
    case 'Vector2': return { type: 'Vector2', value: { x: num(k.X), y: num(k.Y) } };
    case 'CoordinateFrame': case 'CFrame': return { type: 'CFrame', value: { pos: { x: num(k.X), y: num(k.Y), z: num(k.Z) },
      rot: [k.R00, k.R01, k.R02, k.R10, k.R11, k.R12, k.R20, k.R21, k.R22].map(num) } };
    case 'Color3': if (k.R !== undefined) return { type: 'Color3', value: { r: num(k.R), g: num(k.G), b: num(k.B) } };
      { const v = num(t) >>> 0; return { type: 'Color3', value: { r: ((v >> 16) & 255) / 255, g: ((v >> 8) & 255) / 255, b: (v & 255) / 255 } }; }
    case 'Color3uint8': { const v = num(t) >>> 0; return { type: 'Color3', value: { r: ((v >> 16) & 255) / 255, g: ((v >> 8) & 255) / 255, b: (v & 255) / 255 } }; }
    case 'UDim2': return { type: 'UDim2', value: { xs: num(k.XS), xo: num(k.XO), ys: num(k.YS), yo: num(k.YO) } };
    case 'UDim': return { type: 'UDim', value: { scale: num(k.S), offset: num(k.O) } };
    case 'Ref': return { type: 'Reference', value: t.trim() };
    case 'Faces': return { type: 'Faces', value: { bits: Math.trunc(num(k.faces)) } };
    case 'Axes': return { type: 'Axes', value: { bits: Math.trunc(num(k.axes)) } };
    case 'PhysicalProperties': return { type: 'PhysicalProperties', value: k.CustomPhysics === 'true' ? { density: num(k.Density), friction: num(k.Friction), elasticity: num(k.Elasticity) } : null };
    case 'Vector3int16': return { type: 'Vector3int16', value: { x: num(k.X), y: num(k.Y), z: num(k.Z) } };
    case 'NumberRange': case 'NumberSequence': case 'ColorSequence': case 'Rect2D': case 'Ray': case 'Region3int16': case 'Font': case 'UniqueId': case 'SharedString': case 'OptionalCoordinateFrame':
      return { type: tag, value: t.trim() };
    default: return { type: tag, value: Object.keys(k).length ? k : t };
  }
}

function parseRbxlx(input) {
  const str = Buffer.isBuffer(input) ? input.toString('utf8') : String(input);
  const place = new Place(); place.format = 'rbxlx-xml';
  const stack = []; // frames: {kind, tag, attrs, inst?, text, kids}
  const pendingRefs = []; const pendingProps = [];
  let cur = null; let depth = 0;
  const top = () => stack[stack.length - 1];
  tokenize(str, {
    open(name, attrs) {
      const parent = top();
      if (name === 'roblox') { place.meta.xmlVersion = attrs.version || null; stack.push({ kind: 'root' }); return; }
      if (name === 'Item' && (!parent || parent.kind === 'root' || parent.kind === 'item')) {
        const inst = new Instance(attrs.class || 'Instance', attrs.referent || null);
        if (inst.referent) place.byReferent.set(inst.referent, inst);
        const pi = parent && parent.kind === 'item' ? parent.inst : null;
        if (pi) pi.addChild(inst); else place.roots.push(inst);
        stack.push({ kind: 'item', inst }); return;
      }
      if (name === 'Properties' && parent && parent.kind === 'item') { stack.push({ kind: 'props', inst: parent.inst }); return; }
      if (parent && parent.kind === 'props') { stack.push({ kind: 'prop', tag: name, attrs, inst: parent.inst, text: '', kids: {} }); return; }
      if (parent && parent.kind === 'prop') { stack.push({ kind: 'propchild', tag: name, text: '' }); return; }
      stack.push({ kind: 'other', tag: name });
    },
    text(t) { const f = top(); if (!f) return; if (f.kind === 'prop') f.text += unescapeXml(t); else if (f.kind === 'propchild') f.text += unescapeXml(t); },
    cdata(t) { const f = top(); if (!f) return; if (f.kind === 'prop' || f.kind === 'propchild') f.text += t; },
    close(name) {
      const f = stack.pop(); if (!f) return;
      if (f.kind === 'propchild') { const p = top(); if (p && p.kind === 'prop') p.kids[f.tag] = f.text; }
      else if (f.kind === 'prop') {
        const pname = f.attrs.name || f.tag; const conv = convert(f.tag, f);
        if (conv.type === 'Reference') pendingRefs.push([f.inst, pname, conv.value]);
        f.inst.props.set(pname, conv);
      }
    }
  });
  for (const [inst, pname, ref] of pendingRefs) {
    const target = ref && ref !== 'null' ? place.byReferent.get(ref) : null;
    inst.props.set(pname, { type: 'Reference', value: target ? target.referent : null });
  }
  return place;
}
module.exports = { parseRbxlx, unescapeXml };
