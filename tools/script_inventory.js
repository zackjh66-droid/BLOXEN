#!/usr/bin/env node
'use strict';
// Static, NON-EXECUTING inventory of every Lua script in the stored places: hashes + a lexical scan of which engine APIs the scripts reference.
// Output drives the script-runtime requirements (research/SCRIPTING.md). No source text is written out; scripts remain inert data in the place files.
const fs = require('fs'); const path = require('path'); const crypto = require('crypto');
const { parsePlace } = require('../src/importer/parse');
const root = path.join(__dirname, '..'); const store = path.join(root, 'preservation', 'store');
const manifest = JSON.parse(fs.readFileSync(path.join(root, 'preservation', 'manifests', 'places.json'), 'utf8')).items;
const SCRIPT = new Set(['Script', 'LocalScript', 'ModuleScript']);
// Lexical probes (regexes over text; no parsing/executing).
const PROBES = {
  'script.Parent': /\bscript\.Parent\b/, 'Instance.new': /\bInstance\.new\b/, 'game.Workspace/workspace': /\bgame\.Workspace\b|\bworkspace\b/, 'game:GetService': /:GetService\(/, 'game.Players/Players': /\bgame\.Players\b|\bPlayers\b/,
  'wait()': /\bwait\s*\(/, 'Wait()': /\bWait\s*\(/, 'delay/spawn': /\b(delay|spawn|Delay|Spawn)\s*\(/, 'coroutine': /\bcoroutine\./, 'tick/time': /\b(tick|time|elapsedTime)\s*\(/,
  'Touched': /\bTouched\b/, 'connect/Connect': /:(connect|Connect)\s*\(/, 'Changed/ChildAdded': /\b(Changed|ChildAdded|ChildRemoved|DescendantAdded)\b/, 'Humanoid': /\bHumanoid\b/, 'Died/Health': /\b(Died|Health|TakeDamage)\b/,
  'PlayerAdded/Removed': /\bPlayer(Added|Removing)\b/, 'CharacterAdded': /\bCharacter(Added)?\b/, 'leaderstats/values': /\bleaderstats|IntValue|StringValue|BoolValue|ObjectValue|NumberValue\b/, 'Debris': /\bDebris\b/,
  'Vector3/CFrame': /\b(Vector3|CFrame)\.new\b/, 'BrickColor': /\bBrickColor\b/, 'Remove/Destroy/remove': /[:.](Remove|Destroy|remove|destroy)\s*\(/, 'Clone/clone': /[:.](Clone|clone)\s*\(/, 'FindFirstChild': /\b(FindFirstChild|findFirstChild|WaitForChild|GetChildren|getChildren)\b/,
  'Explosion': /\bExplosion\b/, 'Sound/Play': /\bSound\b|:Play\(/, 'GUI': /\b(ScreenGui|TextLabel|TextButton|Frame|Hint|Message)\b/, 'Tool/Hopperbin': /\b(Tool|HopperBin|Equipped|Unequipped)\b/, 'Teams': /\bTeam(s)?\b|TeamColor/, 'BodyMovers': /\bBody(Velocity|Position|Gyro|Force|Thrust)\b/, 'Chat': /\bChatted\b|\bChat\b/,
  'math.*': /\bmath\./, 'string.*': /\bstring\./, 'table.*': /\btable\./, 'os/io': /\b(os|io)\./, 'loadstring/getfenv': /\b(loadstring|getfenv|setfenv|dofile|loadfile)\b/, 'require': /\brequire\s*\(/, 'HttpService': /\bHttp/, 'InsertService': /InsertService|LoadAsset/, 'Marketplace': /MarketplaceService|PlayerOwnsAsset|PromptPurchase/, 'DataStore': /DataStore/, 'Teleport': /Teleport/,
};
function walk(list, here, out) { for (const i of list) { const p = here ? here + '.' + (i.props.get('Name') ? i.props.get('Name').value : i.className) : (i.props.get('Name') ? i.props.get('Name').value : i.className); if (SCRIPT.has(i.className)) out.push([p, i]); walk(i.children || [], p, out); } }
const result = { schema: 'bloxen.script-inventory/1', generated: new Date().toISOString().slice(0, 10), note: 'Lexical scan only. Nothing executed. Probes are regexes: counts are scripts that mention the token, not proof of behaviour.', places: {} };
const agg = {};
for (const it of manifest) {
  const f = path.join(store, it.sha256 + '.rbx'); if (!fs.existsSync(f)) continue;
  const place = parsePlace(fs.readFileSync(f)); const list = []; walk(place.roots || [], '', list);
  const scripts = list.map(([p, i]) => { const src = String((i.props.get('Source') || { value: '' }).value); const hits = Object.entries(PROBES).filter(([, re]) => re.test(src)).map(([k]) => k); return { path: p, class: i.className, bytes: Buffer.byteLength(src), sha256: crypto.createHash('sha256').update(src).digest('hex'), disabled: !!(i.props.get('Disabled') || {}).value, apis: hits }; });
  const counts = {}; for (const s of scripts) for (const h of s.apis) counts[h] = (counts[h] || 0) + 1; for (const [k, v] of Object.entries(counts)) agg[k] = (agg[k] || 0) + v;
  result.places[it.id] = { placeSha256: it.sha256, scripts: scripts.length, byClass: scripts.reduce((a, s) => (a[s.class] = (a[s.class] || 0) + 1, a), {}), totalBytes: scripts.reduce((a, s) => a + s.bytes, 0), apiScriptCounts: counts, flagged: scripts.filter(s => s.apis.some(a => ['loadstring/getfenv', 'require', 'HttpService', 'InsertService', 'DataStore', 'Marketplace', 'Teleport', 'os/io'].includes(a))).map(s => s.path), list: scripts };
}
result.aggregateApiScriptCounts = Object.fromEntries(Object.entries(agg).sort((a, b) => b[1] - a[1]));
fs.writeFileSync(path.join(root, 'research', 'script-inventory.json'), JSON.stringify(result, null, 1) + '\n');
for (const [id, r] of Object.entries(result.places)) console.log(id.padEnd(34), 'scripts', String(r.scripts).padStart(3), JSON.stringify(r.byClass), 'flagged', r.flagged.length);
console.log(result.aggregateApiScriptCounts);
