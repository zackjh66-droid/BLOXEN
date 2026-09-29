'use strict';
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const os = require('os'); const path = require('path'); const crypto = require('crypto'); const http = require('http');
const L = require('../src/launcher/launcher'); const { start } = require('../src/web/main');
const T = 'A'.repeat(24);
test('URI parser: strict grammar, rejects injection', () => {
  assert.deepEqual(L.parseUri(`bloxen-player:1+ticket:${T}+version:version-0d46087630eb46cd`), { ticket: T, version: 'version-0d46087630eb46cd' });
  for (const bad of [`bloxen-player:1+ticket:${T}+version:version-0d46087630eb46cd+server:evil.com`, `bloxen-player:1+ticket:${T};calc+version:version-0d46087630eb46cd`, `bloxen-player:1+ticket:${T}+version:version-0d46087630eb46cd" --script "x`, `bloxen-player:2+ticket:${T}+version:version-0d46087630eb46cd`, `http://evil/${T}`, `bloxen-player:1+ticket:short+version:version-0d46087630eb46cd`, 'bloxen-player:1+ticket:' + 'A'.repeat(300) + '+version:version-0d46087630eb46cd', `bloxen-player:1+ticket:${T}+version:version-0d46087630eb46cd\n`, '', null, 5]) assert.throws(() => L.parseUri(bad), L.LaunchError, String(bad).slice(0, 60));
});
test('config: compatBase must be a bare loopback origin; clientDir absolute', () => {
  const w = o => { const f = path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bx-')), 'c.json'); fs.writeFileSync(f, JSON.stringify(o)); return f; };
  assert.ok(L.loadConfig(w({ clientDir: '/tmp/x', compatBase: 'http://127.0.0.1:8081' })));
  for (const cb of ['http://evil.com', 'https://127.0.0.1', 'http://127.0.0.1.evil.com', 'http://user:pw@127.0.0.1/', 'http://127.0.0.1:8081/path', 'http://192.168.1.5:8081']) assert.throws(() => L.loadConfig(w({ clientDir: '/tmp/x', compatBase: cb })), /compatBase/, cb);
  assert.throws(() => L.loadConfig(w({ clientDir: 'relative/dir' })), /absolute/);
  assert.equal(L.EXPECTED.sha256, '384a4cb38de6977899e09e59c2136619fef521dc7b4adeb34d404945120c8a44');
});
test('client gate: wrong hash / missing exe / wrong version / symlink all refused', () => {
  const d = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-')); const cfg = { clientDir: d, expectedSha256: L.EXPECTED.sha256 };
  assert.throws(() => L.verifyClient(cfg, L.EXPECTED.version), /not found/); fs.writeFileSync(path.join(d, 'RobloxPlayerBeta.exe'), 'tampered'); assert.throws(() => L.verifyClient(cfg, L.EXPECTED.version), /mismatch/);
  assert.throws(() => L.verifyClient(cfg, 'version-ffffffffffffffff'), /only runs/);
  fs.rmSync(path.join(d, 'RobloxPlayerBeta.exe')); fs.writeFileSync(path.join(d, 'real'), 'x'); fs.symlinkSync(path.join(d, 'real'), path.join(d, 'RobloxPlayerBeta.exe')); assert.throws(() => L.verifyClient({ clientDir: d, expectedSha256: crypto.createHash('sha256').update('x').digest('hex') }, L.EXPECTED.version), /regular file/);
});
test('args: join URLs outside the loopback compat origin are refused', () => {
  const cfg = { compatBase: 'http://127.0.0.1:8081' }; const ok = { joinScriptUrl: 'http://127.0.0.1:8081/Game/Join.ashx?t=' + T, authenticationUrl: 'http://127.0.0.1:8081/Login/Negotiate.ashx', authenticationTicket: T };
  assert.deepEqual(L.buildArgs(cfg, ok), ['-a', ok.authenticationUrl, '-t', T, '-j', ok.joinScriptUrl]);
  for (const bad of [{ ...ok, joinScriptUrl: 'http://evil.com/x' }, { ...ok, joinScriptUrl: 'http://127.0.0.1:8081.evil.com/x' }, { ...ok, authenticationUrl: 'http://127.0.0.1:8081/x" -script "y' }, { ...ok, authenticationTicket: 'a b' }, { ...ok, joinScriptUrl: 'http://127.0.0.1:8081/x y' }]) assert.throws(() => L.buildArgs(cfg, bad), L.LaunchError);
});
test('registry file escapes the command and only registers the current user', () => { const r = L.registryFile('C:\\a "b"\\l.cmd'); assert.match(r, /HKEY_CURRENT_USER/); assert.ok(!r.includes('HKEY_LOCAL_MACHINE')); assert.match(r, /\\"b\\"/); });
test('compat backend: host allow-list, launcher header, no proxying, tickets', async t => {
  const app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true }); t.after(() => app.close());
  const get = (p, headers = {}) => new Promise((res, rej) => http.get(app.compatUrl + p, { headers }, r => { let b = ''; r.on('data', d => b += d); r.on('end', () => res({ status: r.statusCode, body: b })); }).on('error', rej));
  assert.equal((await get('/health')).status, 200); assert.equal((await get('/health', { Host: 'evil.example' })).status, 421, 'DNS-rebinding style host rejected');
  const post = (body, headers) => fetch(app.compatUrl + '/launcher/redeem', { method: 'POST', headers, body: JSON.stringify(body) });
  assert.equal((await post({ ticket: T }, {})).status, 403, 'needs launcher header'); assert.equal((await post({ ticket: T }, { 'X-Bloxen-Launcher': '1', Origin: 'http://evil.com' })).status, 403, 'browser origin refused'); assert.equal((await post({ ticket: T }, { 'X-Bloxen-Launcher': '1' })).status, 403);
  assert.equal((await get('/Game/Join.ashx?t=' + T)).status, 403); assert.equal((await get('/asset/?id=abc')).status, 400); assert.equal((await get('/asset/?id=999')).status, 404);
  assert.equal((await get('/nonexistent')).status, 404); assert.equal((await get('/Analytics/Measurement.ashx')).status, 204);
  assert.equal(app.compat.address().address, '127.0.0.1'); assert.equal(app.web.address().address, '127.0.0.1');
  await assert.rejects(() => start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true, host: '0.0.0.0' }), /non-loopback/);
});
