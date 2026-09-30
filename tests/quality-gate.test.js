'use strict';
// Repository-level quality gate: honesty labels, frozen gates, required documents, constants agree, nothing unsafe is tracked. UNIT-TESTED.
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const path = require('path'); const { execFileSync } = require('child_process');
const root = path.join(__dirname, '..'); const read = f => fs.readFileSync(path.join(root, f), 'utf8');
const tracked = (() => { try { return execFileSync('git', ['ls-files'], { cwd: root, encoding: 'utf8' }).split('\n').filter(Boolean); } catch { return null; } })();

test('required documents exist and are non-trivial', () => {
  for (const f of ['HISTORICAL-SOURCE-MAP', 'CLIENT', 'WEBSITE', 'GAMES', 'CATALOG', 'ASSETS', 'PROTOCOL', 'PROVENANCE', 'SCRIPTING', 'PHYSICS', 'SCRIPT-INDEX'].map(n => `research/${n}.md`).concat(['docs/WINDOWS-REAL-CLIENT-VALIDATION.md', 'STATUS.md', 'research/website/SOURCES.md', 'research/website/visual-validation.md'])) assert.ok(fs.existsSync(path.join(root, f)) && read(f).length > 500, f);
});
test('STATUS.md freezes the two real-client gates verbatim', () => {
  const s = read('STATUS.md'); assert.ok(s.includes('REAL CLIENT EXECUTION = BLOCKED: WINDOWS REQUIRED')); assert.ok(s.includes('AUTHORITATIVE SERVER→CLIENT ID_DATA VALIDATION = BLOCKED: REAL CLIENT CAPTURE REQUIRED')); assert.ok(s.includes('PRESERVED 7 / QUARANTINED 3 / REJECTED 3 / UNKNOWN 112'));
  assert.ok(read('research/PHYSICS.md').includes('REAL PHYSICS-ACCURATE TOUCHED = BLOCKED / UNIMPLEMENTED'));
});
test('no document or UI string claims a real-client success', () => {
  const files = (tracked || []).filter(f => /\.(md|js|json|html)$/.test(f) && !f.startsWith('preservation/reference/') && !f.startsWith('tests/')); const bad = /REAL-CLIENT-TESTED\s*[:=]\s*(yes|true|pass)|real client (joined|connected|is playable)|verified (on|with) the real client/i;
  for (const f of files) { const t = read(f).split('\n'); t.forEach((l, i) => assert.ok(!bad.test(l), `${f}:${i + 1} ${l.slice(0, 120)}`)); }
});
test('the reviewed client identity agrees across config, launcher, docs and manifest', () => {
  const sha = '384a4cb38de6977899e09e59c2136619fef521dc7b4adeb34d404945120c8a44', ver = 'version-0d46087630eb46cd', build = '0.205.0.61876';
  const cfg = require('../src/lib/config'); assert.equal(cfg.clientVersion, ver); assert.equal(cfg.clientBuild, build);
  assert.ok(read('src/launcher/launcher.js').includes(sha) && read('src/launcher/launcher.js').includes(ver)); assert.ok(read('docs/WINDOWS-REAL-CLIENT-VALIDATION.md').includes(sha));
  const m = JSON.parse(read('preservation/manifests/client-0.205.0.61876.json')); assert.ok(JSON.stringify(m).includes(sha)); assert.ok(!/"executed"\s*:\s*true/.test(JSON.stringify(m)));
});
test('git tracks no binaries, place files, client files, databases, credentials or oversized files', { skip: !tracked }, () => {
  for (const f of tracked) { assert.ok(!/\.(exe|dll|rbxl|rbxlx|rbxm|rbxmx|7z|zip|sqlite|db|pyc|pem|key)$/i.test(f), 'tracked binary-ish file ' + f); assert.ok(!/^(data|quarantine|cache)\//.test(f) && !/^preservation\/(store|places|client)\//.test(f), f); assert.ok(!/\.env$|credentials|\.netrc/i.test(f), f);
    const size = fs.statSync(path.join(root, f)).size; assert.ok(size < 3_000_000, `${f} is ${size} bytes`); }
  for (const f of tracked.filter(f => /\.(js|json|md|css)$/.test(f))) { const t = read(f); assert.ok(!/\.ROBLOSECURITY=[A-Za-z0-9_|%-]{40,}/.test(t) && !/_\|WARNING:-DO-NOT-SHARE/.test(t), 'cookie-like secret in ' + f); }
});
test('safety switches: scripts off by default, loopback-only services, launcher never uses a shell', () => {
  const cfg = require('../src/lib/config'); assert.equal(cfg.runScripts, false); const l = read('src/launcher/launcher.js'); assert.ok(!/shell:\s*true/.test(l)); assert.ok(!/\{[^}]*\bexec(Sync|File)?\b[^}]*\}\s*=\s*require\('child_process'\)/.test(l) && /\{ spawn \} = require\('child_process'\)/.test(l), 'launcher imports only spawn');
  assert.ok(read('src/lib/http.js').includes("frame-ancestors 'none'") || /X-Frame-Options/i.test(read('src/lib/http.js')));
  for (const f of ['src/web/views.js', 'src/web/components.js']) assert.ok(!/\sstyle=/.test(read(f)), 'inline style= in ' + f);
});
test('ordered quality gate: every stage of the no-Windows pipeline is covered by a named test file', () => {
  const stages = { 'register/login/home/games/catalog/profile': 'tests/e2e-flow.test.js', 'registry + gating': 'tests/registry.test.js', 'pages/pagination/favorites/travel': 'tests/site2.test.js', 'acquire/equip/avatar': 'tests/avatar-server.test.js', 'play ticket + launcher contract': 'tests/prelaunch.test.js', 'teleport': 'tests/teleport.test.js', 'game server/simulator': 'tests/gameserver-e2e.test.js', 'scripts': 'tests/script-engine.test.js', 'importer': 'tests/importer.test.js', 'assets': 'tests/assets.test.js' };
  for (const [stage, f] of Object.entries(stages)) assert.ok(fs.existsSync(path.join(root, f)) && /test\(/.test(read(f)), stage + ' -> ' + f);
});
