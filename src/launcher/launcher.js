'use strict';
// BLOXEN launcher: handles `bloxen-player:` URIs. It runs ONLY the reviewed client build, ONLY against a loopback compat backend, with a fixed argument
// vector built from validated fields. It never executes anything taken from the URI, never uses a shell, and never accepts a remote server address.
const fs = require('fs'); const path = require('path'); const http = require('http'); const crypto = require('crypto'); const { spawn } = require('child_process');
const EXPECTED = { version: 'version-0d46087630eb46cd', build: '0.205.0.61876', exeName: 'RobloxPlayerBeta.exe', sha256: '384a4cb38de6977899e09e59c2136619fef521dc7b4adeb34d404945120c8a44' };
const URI_RE = /^bloxen-player:1\+ticket:([A-Za-z0-9_-]{20,64})\+version:(version-[0-9a-f]{16})$/;
class LaunchError extends Error { constructor(code, msg) { super(msg); this.code = code; } }

function parseUri(uri) { if (typeof uri !== 'string' || uri.length > 200 || /[\s"'`\\<>|&;$%^\x00-\x1f]/.test(uri)) throw new LaunchError('BAD_URI', 'malformed launch URI'); const m = URI_RE.exec(uri); if (!m) throw new LaunchError('BAD_URI', 'unsupported launch URI'); return { ticket: m[1], version: m[2] }; }
function loadConfig(file) {
  const raw = JSON.parse(fs.readFileSync(file, 'utf8')); const c = { clientDir: raw.clientDir, compatBase: raw.compatBase || 'http://127.0.0.1:8081', logFile: raw.logFile || null, expectedSha256: EXPECTED.sha256 };
  if (typeof c.clientDir !== 'string' || !path.isAbsolute(c.clientDir)) throw new LaunchError('BAD_CONFIG', 'clientDir must be an absolute path to a WORKING COPY of the reviewed client');
  const u = new URL(c.compatBase); if (u.protocol !== 'http:' || !['127.0.0.1', 'localhost', '[::1]'].includes(u.hostname) || u.username || u.password || (u.pathname !== '/' && u.pathname !== '')) throw new LaunchError('BAD_CONFIG', 'compatBase must be a bare loopback http origin'); c.compatBase = u.origin; return c;
}
function sha256File(f) { const h = crypto.createHash('sha256'); const fd = fs.openSync(f, 'r'); const buf = Buffer.alloc(1 << 20); try { let n; while ((n = fs.readSync(fd, buf, 0, buf.length, null)) > 0) h.update(buf.subarray(0, n)); } finally { fs.closeSync(fd); } return h.digest('hex'); }
function verifyClient(cfg, uriVersion) {
  if (uriVersion !== EXPECTED.version) throw new LaunchError('VERSION', `this launcher only runs ${EXPECTED.version}, got ${uriVersion}`);
  const exe = path.join(cfg.clientDir, EXPECTED.exeName); let st; try { st = fs.lstatSync(exe); } catch { throw new LaunchError('NO_CLIENT', 'client executable not found in clientDir'); } if (!st.isFile() || st.isSymbolicLink()) throw new LaunchError('NO_CLIENT', 'client executable is not a regular file');
  const got = sha256File(exe); if (got !== cfg.expectedSha256) throw new LaunchError('HASH', `client SHA-256 mismatch: ${got}`); return exe;
}
// The client reads BaseUrl from AppSettings.xml next to the exe. Only the WORKING COPY is ever modified.
function writeAppSettings(cfg) { const f = path.join(cfg.clientDir, 'AppSettings.xml'); fs.writeFileSync(f, `<?xml version="1.0" encoding="UTF-8"?>\n<Settings>\n\t<ContentFolder>content</ContentFolder>\n\t<BaseUrl>${cfg.compatBase}</BaseUrl>\n</Settings>\n`); return f; }
function redeem(cfg, ticket) {
  return new Promise((res, rej) => { const body = JSON.stringify({ ticket }); const u = new URL(cfg.compatBase + '/launcher/redeem');
    const r = http.request({ host: u.hostname === '[::1]' ? '::1' : u.hostname, port: u.port, path: u.pathname, method: 'POST', headers: { 'Content-Type': 'application/json', 'Content-Length': Buffer.byteLength(body), 'X-Bloxen-Launcher': '1' }, timeout: 8000 }, resp => { const ch = []; resp.on('data', d => ch.push(d)); resp.on('end', () => { let j; try { j = JSON.parse(Buffer.concat(ch).toString()); } catch { return rej(new LaunchError('REDEEM', 'bad redeem response')); } if (resp.statusCode !== 200) return rej(new LaunchError('REDEEM', j.error || 'ticket rejected')); res(j); }); });
    r.on('error', e => rej(new LaunchError('REDEEM', 'compat backend unreachable: ' + e.message))); r.on('timeout', () => r.destroy(new Error('timeout'))); r.end(body); });
}
function buildArgs(cfg, join) {
  const origin = cfg.compatBase; const okUrl = s => typeof s === 'string' && s.startsWith(origin + '/') && !/[\s"'`\\<>|&;^\x00-\x1f]/.test(s.replace(/[?=&]/g, '')) && s.length < 300;
  if (!okUrl(join.joinScriptUrl) || !okUrl(join.authenticationUrl)) throw new LaunchError('BAD_JOIN', 'join URLs must stay on the loopback compat origin');
  if (!/^[A-Za-z0-9_-]{20,64}$/.test(join.authenticationTicket)) throw new LaunchError('BAD_JOIN', 'bad authentication ticket');
  return ['-a', join.authenticationUrl, '-t', join.authenticationTicket, '-j', join.joinScriptUrl]; // boost options observed in the client: authenticationUrl,a authenticationTicket,t joinScriptUrl,j
}
function makeLogger(cfg) { return m => { const line = `${new Date().toISOString()} ${m.replace(/ticket:[A-Za-z0-9_-]+/g, 'ticket:***')}\n`; if (cfg.logFile) { try { fs.appendFileSync(cfg.logFile, line); } catch {} } else process.stderr.write(line); }; }

async function launch(uri, cfg, { spawnFn = spawn, dryRun = false } = {}) {
  const log = makeLogger(cfg); const { ticket, version } = parseUri(uri); log('launch request accepted (ticket redacted)');
  const exe = verifyClient(cfg, version); log('client hash verified'); writeAppSettings(cfg);
  const join = await redeem(cfg, ticket); const args = buildArgs(cfg, join); log('redeemed; args validated');
  if (dryRun) return { exe, args, join };
  const child = spawnFn(exe, args, { cwd: cfg.clientDir, shell: false, detached: true, stdio: 'ignore', windowsHide: false }); child.unref?.(); log('client started pid=' + child.pid); return { exe, args, join, pid: child.pid };
}
function registryFile(launcherCmd) { const esc = s => s.replace(/\\/g, '\\\\').replace(/"/g, '\\"'); return `Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\Classes\\bloxen-player]\r\n@="URL:BLOXEN Player"\r\n"URL Protocol"=""\r\n\r\n[HKEY_CURRENT_USER\\Software\\Classes\\bloxen-player\\shell\\open\\command]\r\n@="${esc(launcherCmd)} \\"%1\\""\r\n`; }
module.exports = { EXPECTED, LaunchError, parseUri, loadConfig, verifyClient, writeAppSettings, redeem, buildArgs, launch, registryFile, sha256File };

if (require.main === module) {
  const args = process.argv.slice(2);
  (async () => {
    if (args[0] === '--print-reg') { process.stdout.write(registryFile(args[1] || 'C:\\BLOXEN\\launcher\\bloxen-launcher.cmd')); return; }
    const cfgFile = process.env.BLOXEN_LAUNCHER_CONFIG || path.join(__dirname, 'launcher.config.json'); const cfg = loadConfig(cfgFile);
    if (args[0] === '--verify') { const exe = verifyClient(cfg, EXPECTED.version); console.log('OK', exe, cfg.expectedSha256); return; }
    if (args.length !== 1) throw new LaunchError('USAGE', 'usage: launcher <bloxen-player:...> | --verify | --print-reg [cmd]');
    await launch(args[0], cfg);
  })().catch(e => { console.error(`launcher: ${e.code || 'ERROR'}: ${e.message}`); process.exit(1); });
}
