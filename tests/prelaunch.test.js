'use strict';
// Pre-launch contract tests: Play API, ticket creation/TTL/single use/user+game scope, URI build/parse, invalid URI rejection, version + expected-SHA-256 requirements,
// configuration and error handling. The FINAL process launch is mocked (spawnFn): no executable is ever started. SIMULATOR/UNIT level; nothing here is real-client evidence.
const test = require('node:test'); const assert = require('node:assert/strict'); const fs = require('fs'); const os = require('os'); const path = require('path'); const crypto = require('crypto');
const { start } = require('../src/web/main'); const { Browser, XML_PLACE } = require('./helpers'); const L = require('../src/launcher/launcher'); const cfgMod = require('../src/lib/config');

async function boot(t, { seedStore = true } = {}) {
  const store = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-pl-')); const app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true, storeDir: store }); t.after(() => app.close());
  const buf = Buffer.from(XML_PLACE); const sha = crypto.createHash('sha256').update(buf).digest('hex'); if (seedStore) fs.writeFileSync(path.join(store, sha + '.rbx'), buf);
  app.db.prepare("UPDATE places SET sha256=? WHERE id='tabularasa'").run(sha); return app;
}
async function player(app, name) { const b = new Browser(app.webUrl); await b.register(name || 'pl' + Math.random().toString(36).slice(2, 8)); return b; }
const play = async (b, id = 'tabularasa') => b.req('POST', `/games/${id}/play`, { form: { _csrf: b.csrfFrom((await b.get(`/games/${id}`)).text) }, headers: { accept: 'application/json' } });
function stubClient() { const d = fs.mkdtempSync(path.join(os.tmpdir(), 'bx-cl-')); fs.writeFileSync(path.join(d, 'RobloxPlayerBeta.exe'), 'stub'); return { dir: d, sha: crypto.createHash('sha256').update('stub').digest('hex') }; }
const mkCfg = (app, c, extra = {}) => ({ clientDir: c.dir, compatBase: app.compatUrl, logFile: path.join(c.dir, 'l.log'), expectedSha256: c.sha, ...extra });
const spawnMock = () => { const calls = []; return { calls, fn: (exe, args, o) => { calls.push({ exe, args, o }); return { pid: 4242, unref() {} }; } }; };

test('Play API: requires login + CSRF, returns URI + TTL only, never a server address, and creates exactly one hashed ticket', async t => {
  const app = await boot(t); const anon = new Browser(app.webUrl); const r0 = await anon.req('POST', '/games/tabularasa/play', { form: {} }); assert.ok([302, 303, 403].includes(r0.status)); assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n, 0);
  const b = await player(app); const noCsrf = await b.req('POST', '/games/tabularasa/play', { form: {}, headers: { accept: 'application/json' } }); assert.equal(noCsrf.status, 403); assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n, 0);
  const r = await play(b); assert.equal(r.status, 200); const j = JSON.parse(r.text); assert.deepEqual(Object.keys(j).sort(), ['expiresInSeconds', 'uri']); assert.ok(j.expiresInSeconds <= 60);
  assert.ok(!/(127\.0\.0\.1|localhost|:\d{4,5}|http)/i.test(j.uri), 'no address or URL in the URI'); assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n, 1);
  const { ticket, version } = L.parseUri(j.uri); assert.equal(version, cfgMod.clientVersion); assert.equal(app.db.prepare('SELECT hash FROM launch_tickets').get().hash, crypto.createHash('sha256').update(ticket).digest('hex'));
});
test('Play API error handling: unknown, quarantined, rejected, unknown-status and not-in-store games never yield a ticket', async t => {
  const app = await boot(t); const b = await player(app); const csrf = b.csrfFrom((await b.get('/games/tabularasa')).text);
  for (const id of ['nope', 'natural-disaster-survival', 'roblox-evil-game-idk-stolen', 'post2015:classic-basplate', 'unknown:000000000000', '../etc/passwd']) { const r = await b.req('POST', `/games/${encodeURIComponent(id)}/play`, { form: { _csrf: csrf }, headers: { accept: 'application/json' } }); assert.equal(r.status, 404, id); }
  assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n, 0);
  const app2 = await boot(t, { seedStore: false }); const b2 = await player(app2); const r = await b2.req('POST', '/games/tabularasa/play', { form: { _csrf: b2.csrfFrom((await b2.get('/games/tabularasa')).text) }, headers: { accept: 'application/json' } });
  assert.notEqual(r.status, 200); assert.equal(app2.db.prepare('SELECT COUNT(*) n FROM launch_tickets').get().n, 0, 'place file not in store -> no ticket');
});
test('ticket scope through the real flow: bound to the issuing account and game; single use; a second redeem fails; two players never cross', async t => {
  const app = await boot(t); const a = await player(app, 'alice01'), b = await player(app, 'bobby01'); const ua = JSON.parse((await play(a)).text).uri, ub = JSON.parse((await play(b)).text).uri; const c = stubClient(); const cfg = mkCfg(app, c);
  const m = spawnMock(); const ra = await L.launch(ua, cfg, { spawnFn: m.fn }); const rb = await L.launch(ub, cfg, { spawnFn: m.fn }); assert.equal(m.calls.length, 2);
  assert.equal(ra.join.userName, 'alice01'); assert.equal(rb.join.userName, 'bobby01'); assert.notEqual(ra.join.authenticationTicket, rb.join.authenticationTicket);
  await assert.rejects(() => L.launch(ua, cfg, { spawnFn: m.fn }), /invalid, expired or already used|ticket/); assert.equal(m.calls.length, 2, 'replayed URI must not spawn');
  const used = app.db.prepare('SELECT COUNT(*) n FROM launch_tickets WHERE used_at IS NOT NULL').get().n; assert.equal(used, 2);
  // join token: scoped to the server, consumed by the game server only once
  const jt = new URL(ra.join.joinScriptUrl).searchParams.get('t'); const auth = require('../src/lib/auth'); const srv = [...app.gameservers.servers.values()][0];
  assert.equal(auth.redeemTicket(app.db, 'join_tokens', jt, { serverId: 'other-server', placeId: 'tabularasa' }), null); assert.ok(auth.redeemTicket(app.db, 'join_tokens', jt, { serverId: srv.serverId, placeId: 'tabularasa' })); assert.equal(auth.redeemTicket(app.db, 'join_tokens', jt, { serverId: srv.serverId, placeId: 'tabularasa' }), null);
});
test('TTL: a ticket older than its TTL is refused by the compat backend and the launcher does not spawn', async t => {
  const app = await boot(t); const b = await player(app); const uri = JSON.parse((await play(b)).text).uri; app.db.prepare('UPDATE launch_tickets SET expires_at=?').run(Date.now() - 1);
  const c = stubClient(); const m = spawnMock(); await assert.rejects(() => L.launch(uri, mkCfg(app, c), { spawnFn: m.fn }), /expired|invalid|ticket/); assert.equal(m.calls.length, 0);
});
test('URI build/parse round trip and invalid URI rejection (mutations of a genuine URI)', async t => {
  const app = await boot(t); const b = await player(app); const uri = JSON.parse((await play(b)).text).uri; const ok = L.parseUri(uri); assert.equal(`bloxen-player:1+ticket:${ok.ticket}+version:${ok.version}`, uri);
  const bad = [uri + '+x:y', uri + ' --script evil', uri.replace('bloxen-player', 'roblox-player'), uri.replace('+version:', '+ver:'), uri.replace('ticket:', 'ticket:../'), uri.toUpperCase(), uri + '\n', uri.replace('1+', '9+'), '', 'bloxen-player:', uri.slice(0, 40), uri + 'A'.repeat(300), uri.replace('version-0d46087630eb46cd', 'version-0d46087630eb46cd%20x'), uri.replace('+', ' '), ' ' + uri, null, 5, {}, [uri]];
  for (const x of bad) assert.throws(() => L.parseUri(x), L.LaunchError, String(x).slice(0, 60));
});
test('version + expected SHA-256 requirements: launcher refuses a different version, a missing/tampered client, and a config that omits the pinned hash falls back to the built-in hash', async t => {
  const app = await boot(t); const b = await player(app); const uri = JSON.parse((await play(b)).text).uri; const c = stubClient(); const m = spawnMock();
  const wrongVer = uri.replace(cfgMod.clientVersion, 'version-ffffffffffffffff'); await assert.rejects(() => L.launch(wrongVer, mkCfg(app, c), { spawnFn: m.fn }), /only runs/);
  await assert.rejects(() => L.launch(uri, mkCfg(app, c, { expectedSha256: '0'.repeat(64) }), { spawnFn: m.fn }), /mismatch/);
  await assert.rejects(() => L.launch(uri, mkCfg(app, c, { expectedSha256: undefined }), { spawnFn: m.fn }), /mismatch/, 'stub cannot satisfy the built-in genuine-client hash');
  fs.writeFileSync(path.join(c.dir, 'RobloxPlayerBeta.exe'), 'tampered'); await assert.rejects(() => L.launch(uri, mkCfg(app, c), { spawnFn: m.fn }), /mismatch/);
  fs.rmSync(path.join(c.dir, 'RobloxPlayerBeta.exe')); await assert.rejects(() => L.launch(uri, mkCfg(app, c), { spawnFn: m.fn }), /not found/);
  assert.equal(m.calls.length, 0); assert.equal(L.EXPECTED.sha256, cfgMod.clientExeSha256); assert.equal(L.EXPECTED.version, cfgMod.clientVersion);
  assert.equal(app.db.prepare('SELECT COUNT(*) n FROM launch_tickets WHERE used_at IS NOT NULL').get().n, 0, 'a refused client must not burn the ticket (gate runs before redeem)');
});
test('final launch is mocked and safe: no shell, args only -a/-t/-j, cwd = client dir, exe path fixed, ticket redacted in the log', async t => {
  const app = await boot(t); const b = await player(app); const uri = JSON.parse((await play(b)).text).uri; const c = stubClient(); const m = spawnMock(); const cfg = mkCfg(app, c);
  const r = await L.launch(uri, cfg, { spawnFn: m.fn }); const call = m.calls[0]; assert.equal(call.o.shell, false); assert.equal(call.o.cwd, c.dir); assert.equal(call.exe, path.join(c.dir, 'RobloxPlayerBeta.exe'));
  assert.deepEqual(call.args.filter((_, i) => i % 2 === 0), ['-a', '-t', '-j']); assert.ok(call.args.every(x => !/[;&|`$"'\s]/.test(x)), 'no shell metacharacters or whitespace in any argument');
  assert.ok(call.args[1].startsWith(app.compatUrl + '/') && call.args[5].startsWith(app.compatUrl + '/'), 'auth + join URLs stay on the loopback compat origin'); const log = fs.readFileSync(cfg.logFile, 'utf8'); assert.ok(!log.includes(L.parseUri(uri).ticket) && !log.includes(r.join.authenticationTicket), 'log never contains the tickets');
  assert.ok(fs.readFileSync(path.join(c.dir, 'AppSettings.xml'), 'utf8').includes(app.compatUrl));
});
test('launcher error handling: compat backend down -> clear error, nothing spawned; dry run never spawns', async t => {
  const app = await boot(t); const b = await player(app); const uri = JSON.parse((await play(b)).text).uri; const c = stubClient(); const m = spawnMock();
  await assert.rejects(() => L.launch(uri, mkCfg(app, c, { compatBase: 'http://127.0.0.1:1' }), { spawnFn: m.fn }), /unreachable/); assert.equal(m.calls.length, 0);
  const uri2 = JSON.parse((await play(b)).text).uri; const d = await L.launch(uri2, mkCfg(app, c), { spawnFn: m.fn, dryRun: true }); assert.ok(d.args.length === 6 && m.calls.length === 0);
});
test('client-initiated travel: compat PlaceLauncher maps a historical PlaceId to a local place, or returns a controlled unsupported-local-destination; never forwards', async t => {
  const app = await boot(t); const b = await player(app); const uri = JSON.parse((await play(b)).text).uri; const c = stubClient(); const r = await L.launch(uri, mkCfg(app, c), { dryRun: true }); const jt = new URL(r.join.joinScriptUrl).searchParams.get('t');
  const q = (pid, tok = jt) => fetch(`${app.compatUrl}/game/PlaceLauncher.ashx?request=RequestGame&placeId=${encodeURIComponent(pid)}&t=${tok}`).then(async x => ({ s: x.status, j: await x.json() }));
  for (const pid of ['999999', '0', '-1', 'abc', '1818;x', '1.5']) { const x = await q(pid); assert.equal(x.j.status, 4, pid); assert.match(x.j.message, /unsupported-local-destination/); assert.equal(x.j.joinScriptUrl, null); }
  const noTok = await q('1818', 'x'.repeat(24)); assert.equal(noTok.s, 403);
  const mapped = await q('1818'); assert.equal(mapped.j.status, 4, 'mapped target is not in this test store, so UNAVAILABLE, still a controlled refusal'); assert.match(mapped.j.message, /UNAVAILABLE|not in the local store/);
  assert.ok(app.db.prepare("SELECT COUNT(*) n FROM compat_log WHERE path='(teleport)'").get().n >= 7);
});
