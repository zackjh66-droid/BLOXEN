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

// ---- tiny HTTP client with a cookie jar (manual redirects)
class Browser {
  constructor(base) { this.base = base; this.jar = new Map(); }
  _cookies() { return [...this.jar].map(([k, v]) => k + '=' + v).join('; '); }
  async req(method, p, { form, headers = {} } = {}) {
    const r = await fetch(this.base + p, { method, redirect: 'manual', headers: { cookie: this._cookies(), ...(form ? { 'content-type': 'application/x-www-form-urlencoded' } : {}), ...headers }, body: form ? new URLSearchParams(form).toString() : undefined });
    for (const sc of r.headers.getSetCookie()) { const [kv, ...attrs] = sc.split(';'); const i = kv.indexOf('='); const k = kv.slice(0, i), v = kv.slice(i + 1); if (/max-age=0/i.test(attrs.join(';')) || v === '') this.jar.delete(k); else this.jar.set(k, v); }
    return { status: r.status, headers: r.headers, location: r.headers.get('location'), text: await r.text() };
  }
  get(p, o) { return this.req('GET', p, o); }
  async follow(r) { let x = r; for (let i = 0; i < 5 && x.location; i++) x = await this.get(x.location); return x; }
  csrfFrom(html) { const m = /name="_csrf" value="([^"]+)"/.exec(html); return m && m[1]; }
  async post(p, form, { page = p, token } = {}) { const t = token || this.csrfFrom((await this.get(page)).text); return this.req('POST', p, { form: { ...form, _csrf: t } }); }
  async register(username, password = 'correct horse 9') { const pg = await this.get('/register'); return this.req('POST', '/register', { form: { username, password, bm: 7, bd: 4, by: 1999, gender: 'm', _csrf: this.csrfFrom(pg.text) } }); }
  async login(username, password = 'correct horse 9') { const pg = await this.get('/login'); return this.req('POST', '/login', { form: { username, password, _csrf: this.csrfFrom(pg.text) } }); }
}
const XML_PLACE = `<roblox version="4"><Item class="Workspace" referent="RBX0"><Properties><string name="Name">Workspace</string></Properties><Item class="Part" referent="RBX1"><Properties><string name="Name">Baseplate</string><Vector3 name="size"><X>64</X><Y>2</Y><Z>64</Z></Vector3><CoordinateFrame name="CFrame"><X>0</X><Y>0</Y><Z>0</Z><R00>1</R00><R01>0</R01><R02>0</R02><R10>0</R10><R11>1</R11><R12>0</R12><R20>0</R20><R21>0</R21><R22>1</R22></CoordinateFrame><bool name="Anchored">true</bool></Properties></Item></Item></roblox>`;
Object.assign(module.exports, { Browser, XML_PLACE });
