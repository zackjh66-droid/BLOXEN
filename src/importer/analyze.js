'use strict';
// Static analysis of a parsed Place. NOTHING is executed. Scripts are hashed + pattern-scanned as text only.
const crypto = require('crypto');
const fs = require('fs'); const path = require('path');
const API = JSON.parse(fs.readFileSync(path.join(__dirname, '../../preservation/reference/api-0.205.0.61876.json'), 'utf8'));

const LATER = JSON.parse(fs.readFileSync(path.join(__dirname, '../../preservation/reference/api-later-production.json'), 'utf8')).classes;
function laterHas(cls, prop) { let c = cls; const seen = new Set(); while (c && LATER[c] && !seen.has(c)) { seen.add(c); if (LATER[c].includes(prop)) return true; c = (API.classes[c] || {}).super; } return false; }
const SCRIPT_CLASSES = new Set(['Script', 'LocalScript', 'ModuleScript', 'CoreScript']);
const SCRIPT_PATTERNS = [
  ['http', /HttpService|HttpGet|HttpPost|GetAsync|PostAsync|RequestAsync/], ['loadstring', /loadstring|getfenv|setfenv/], ['require', /\brequire\s*\(/],
  ['insertservice', /InsertService|LoadAsset|LoadAssetVersion/], ['datastore', /DataStoreService|GlobalDataStore|GetDataStore/], ['marketplace', /MarketplaceService|PromptPurchase|PlayerOwnsAsset|UserOwnsGamePass/],
  ['teleport', /TeleportService|Teleport\s*\(/], ['badges', /BadgeService|AwardBadge/], ['remote', /RemoteEvent|RemoteFunction|FireServer|FireClient|OnServerEvent/],
  ['gui', /\bScreenGui\b|\bPlayerGui\b|\bTextLabel\b|\bTextButton\b|\bFrame\b/], ['tool', /\bTool\b|Equipped|Unequipped|Activated/], ['physics', /BodyVelocity|BodyPosition|BodyGyro|BodyForce|BodyThrust|RocketPropulsion/],
  ['leaderstats', /leaderstats|IntValue|StringValue/], ['chat', /ChatService|\.Chatted|Chat:Chat/], ['obfuscated', /\\\d{3}\\\d{3}\\\d{3}|\bl{4,}\b|string\.char\s*\(\s*\d+\s*,\s*\d+/],
  ['ban/kick', /:Kick\(|\.Kick\(|BanList|ban\s*=/], ['adminlike', /\badmin\b|\bcommand[s]?\b/i],
];
const ASSET_RES = [/rbxassetid:\/\/(\d+)/g, /(?:www\.)?roblox\.com\/asset\/?\?id=(\d+)/gi, /rbxasset:\/\/[^\s"']+/g, /[?&]id=(\d{3,})/g, /\/asset\/\?version=\d+&id=(\d+)/gi];
const TEXTURE_PROPS = /^(Texture|TextureID|TextureId|MeshId|Image|Face|Shirt|ShirtTemplate|PantsTemplate|Graphic|SoundId|SkyboxBk|SkyboxDn|SkyboxFt|SkyboxLf|SkyboxRt|SkyboxUp|AnimationId|MeshID|Mesh|ColorMap|ImageTexture)$/i;
const SERVICE_NAMES = new Set(Object.entries(API.classes).filter(([, c]) => c.tags.includes('Service')).map(([n]) => n));

function sha256(b) { return crypto.createHash('sha256').update(b).digest('hex'); }
function superChain(cls) { const out = []; let c = cls; while (c && API.classes[c]) { out.push(c); c = API.classes[c].super; } return out; }
function propsFor(cls) { const s = new Set(); for (const c of superChain(cls)) for (const p of API.classes[c].props) s.add(p[0]); return s; }
const propCache = new Map(); function pf(cls) { let v = propCache.get(cls); if (!v) { v = propsFor(cls); propCache.set(cls, v); } return v; }

// Classes the BLOXEN replicator currently knows how to place into a world (see src/gameserver/world.js)
const REPLICATED_OK = new Set(['Workspace', 'Model', 'Part', 'WedgePart', 'CornerWedgePart', 'TrussPart', 'SpawnLocation', 'Seat', 'VehicleSeat', 'Camera', 'Folder',
  'BlockMesh', 'SpecialMesh', 'CylinderMesh', 'Decal', 'Texture', 'Sound', 'SkateboardPlatform', 'Team', 'Lighting', 'Sky', 'Smoke', 'Fire', 'Sparkles', 'PointLight', 'SpotLight', 'SurfaceLight']);

function analyze(place, { fileName = '' } = {}) {
  const classCounts = {}; const scripts = []; const assets = new Map(); const postClasses = {}; const legacyClasses = {}; const anachronisticProps = {}; const legacyProps = {}; const servicesPresent = [];
  let parts = 0, terrainData = false, guis = 0, sounds = 0, meshes = 0;
  const walk = (inst, p) => {
    classCounts[inst.className] = (classCounts[inst.className] || 0) + 1;
    const here = p ? p + '.' + inst.name : inst.name;
    const known = API.classes[inst.className];
    if (!known) { if (LATER[inst.className]) postClasses[inst.className] = (postClasses[inst.className] || 0) + 1; else legacyClasses[inst.className] = (legacyClasses[inst.className] || 0) + 1; }
    else { const allowed = pf(inst.className); for (const k of inst.props.keys()) if (!allowed.has(k)) { const key = inst.className + '.' + k;
      if (laterHas(inst.className, k)) anachronisticProps[key] = (anachronisticProps[key] || 0) + 1; else legacyProps[key] = (legacyProps[key] || 0) + 1; } }
    if (SCRIPT_CLASSES.has(inst.className)) {
      const srcProp = inst.props.get('Source'); const src = srcProp ? String(srcProp.value) : '';
      const hits = SCRIPT_PATTERNS.filter(([, re]) => re.test(src)).map(([n]) => n);
      const dis = inst.props.get('Disabled');
      scripts.push({ path: here, class: inst.className, bytes: Buffer.byteLength(src), sha256: sha256(Buffer.from(src)), disabled: !!(dis && dis.value), linked: !!(inst.props.get('LinkedSource') && inst.props.get('LinkedSource').value), markers: hits });
      for (const re of ASSET_RES) { re.lastIndex = 0; let m; while ((m = re.exec(src))) if (m[1]) addAsset(m[1], 'script:' + here); }
    }
    for (const [k, v] of inst.props) {
      if (v.type === 'Content' || (v.type === 'string' && TEXTURE_PROPS.test(k)) || (v.type === 'ProtectedString' && false)) {
        const s = String(v.value);
        for (const re of ASSET_RES) { re.lastIndex = 0; let m; while ((m = re.exec(s))) { if (m[1]) addAsset(m[1], inst.className + '.' + k); else if (m[0].startsWith('rbxasset://')) addAsset(m[0], inst.className + '.' + k, 'client-builtin'); } }
      }
    }
    if (/Part$|^Part$|Seat$|SpawnLocation|TrussPart|Platform$/.test(inst.className)) parts++;
    if (inst.className === 'Terrain') terrainData = terrainData || inst.props.has('SmoothGrid') || inst.props.has('PhysicsGrid');
    if (/Gui$|^Frame$|Label$|Button$/.test(inst.className)) guis++;
    if (inst.className === 'Sound') sounds++;
    if (/Mesh$/.test(inst.className) || inst.className === 'MeshPart') meshes++;
    for (const c of inst.children) walk(c, here);
  };
  function addAsset(idOrPath, where, kind) { const k = kind === 'client-builtin' ? idOrPath : String(idOrPath); let e = assets.get(k); if (!e) { e = { id: k, kind: kind || 'asset-id', uses: 0, where: new Set() }; assets.set(k, e); } e.uses++; if (e.where.size < 5) e.where.add(where); }
  for (const r of place.roots) { walk(r, ''); if (SERVICE_NAMES.has(r.className)) servicesPresent.push(r.className); }
  const unsupportedClasses = Object.keys(classCounts).filter(c => API.classes[c] && !REPLICATED_OK.has(c) && !SERVICE_NAMES.has(c) && !SCRIPT_CLASSES.has(c));
  const total = place.count();
  const sum = o => Object.values(o).reduce((x, y) => x + y, 0);
  const postCount = sum(postClasses) + sum(anachronisticProps), legacyCount = sum(legacyClasses) + sum(legacyProps);
  const assetList = [...assets.values()].map(a => ({ ...a, where: [...a.where] })).sort((a, b) => b.uses - a.uses);
  const top = (o, n = 25) => Object.fromEntries(Object.entries(o).sort((a, b) => b[1] - a[1]).slice(0, n));
  // Era plausibility against the 0.205.0.61876 API dump, cross-checked with a later production dump.
  // post-2015 = class/property exists in the LATER dump but not in the 2015 one. legacy = in neither (older or removed API).
  // This is a heuristic, NOT proof of a date.
  const era = { targetBuild: '0.205.0.61876', post2015Classes: postClasses, post2015PropertyCounts: top(anachronisticProps), legacyClasses, legacyPropertyCounts: top(legacyProps),
    post2015Score: +(postCount / Math.max(total, 1)).toFixed(4), legacyScore: +(legacyCount / Math.max(total, 1)).toFixed(4),
    verdict: postCount === 0 ? (legacyCount > 0 ? 'predates-or-matches-2015-API (legacy members present)' : 'consistent-with-2015-API') : 'post-2015-features-present' };
  const scriptSummary = { total: scripts.length, byClass: scripts.reduce((a, s) => (a[s.class] = (a[s.class] || 0) + 1, a), {}), disabled: scripts.filter(s => s.disabled).length, bytes: scripts.reduce((a, s) => a + s.bytes, 0),
    markerCounts: scripts.reduce((a, s) => { s.markers.forEach(m => a[m] = (a[m] || 0) + 1); return a; }, {}), flagged: scripts.filter(s => s.markers.some(m => ['http', 'loadstring', 'require', 'insertservice', 'obfuscated', 'ban/kick', 'adminlike'].includes(m))).length };
  const compat = {
    geometry: parts > 0 ? (unsupportedClasses.length ? 'PARTIAL' : 'SUPPORTED') : 'UNKNOWN',
    geometryDetail: `${parts} part-like instances; instance classes without a replication mapping yet: ${unsupportedClasses.slice(0, 20).join(', ') || 'none'}`,
    terrain: terrainData ? 'UNSUPPORTED' : 'SUPPORTED', scripts: scripts.length ? 'UNSUPPORTED' : 'SUPPORTED',
    scriptsDetail: scripts.length ? 'No Lua runtime is wired into the game server yet (inert data only); game logic will not run.' : 'no scripts',
    gui: guis ? 'UNSUPPORTED' : 'SUPPORTED', assets: assetList.length ? 'PARTIAL' : 'SUPPORTED',
    assetsDetail: assetList.length ? `${assetList.length} distinct asset references; availability tracked in asset service (most are MISSING until preserved copies are supplied)` : 'no external assets',
  };
  const overall = compat.scripts === 'SUPPORTED' && compat.gui === 'SUPPORTED' && compat.terrain === 'SUPPORTED' && compat.geometry === 'SUPPORTED' ? 'SUPPORTED' : (parts > 0 ? 'PARTIAL' : 'UNKNOWN');
  return { instanceCount: total, rootServices: place.roots.map(r => r.className), classCounts: Object.fromEntries(Object.entries(classCounts).sort((a, b) => b[1] - a[1])), parts, meshes, sounds, guis, terrain: terrainData,
    scripts, scriptSummary, assets: assetList, era, compat, overall, parseWarnings: place.warnings.slice(0, 50), parseWarningCount: place.warnings.length, meta: place.meta };
}
module.exports = { analyze, API, SERVICE_NAMES, sha256 };
