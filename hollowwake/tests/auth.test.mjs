import { test } from 'node:test';
import assert from 'node:assert/strict';
import { DatabaseSync } from 'node:sqlite';
import { randomBytes } from 'node:crypto';
import { createApplication } from '../backend/app.mjs';
import { readConfig } from '../backend/config.mjs';
const origin = 'http://127.0.0.1:8787';
async function fixture(t, extra = {}) {
  const config = { ...readConfig({ PUBLIC_ORIGIN: origin, ALLOW_LOCAL_HTTP: '1', BETTER_AUTH_SECRET: randomBytes(48).toString('hex'), DATABASE_PATH: '/tmp/hollowwake-tests/db.sqlite' }), ...extra };
  const mail = [];
  const app = await createApplication(config, { database: new DatabaseSync(':memory:'), sendMail: message => { mail.push(message); }, captureMigration: true });
  t.after(() => app.close());
  function client(ip) {
    let cookie = '';
    return { async request(path, data, { bearer, method = data === undefined ? 'GET' : 'POST', noOrigin = false } = {}) {
      const headers = new Headers();
      if (!noOrigin) headers.set('Origin', origin);
      if (data !== undefined) headers.set('Content-Type', 'application/json');
      if (bearer) headers.set('Authorization', `Bearer ${bearer}`); else if (cookie) headers.set('Cookie', cookie);
      const response = await app.fetch(new Request(origin + path, { method, headers, ...(data !== undefined ? { body: JSON.stringify(data) } : {}) }), ip);
      for (const value of response.headers.getSetCookie()) {
        const [pair] = value.split(';'); const name = pair.split('=')[0];
        const existing = cookie.split('; ').filter(c => c && !c.startsWith(name + '=')); existing.push(pair); cookie = existing.join('; ');
      }
      let body; try { body = await response.json(); } catch { body = null; }
      return { status: response.status, body, headers: response.headers };
    }, get cookie() { return cookie; } };
  }
  async function register(c, name) { const r = await c.request('/api/auth/sign-up/email', { email: `${name}@example.test`, password: 'correct horse battery cloud 77', name, username: name }); assert.equal(r.status, 200, r.body?.message); assert.equal(r.body.token, undefined); return r; }
  async function code(c) { const r = await c.request('/api/device/code', { client_id: 'hollowwake-uwp' }); assert.equal(r.status, 200, r.body?.message); return r.body; }
  async function redeem(c, value) { return c.request('/api/device/token', { grant_type: 'urn:ietf:params:oauth:grant-type:device_code', device_code: value.device_code, client_id: 'hollowwake-uwp' }); }
  async function approve(c, value) {
    assert.equal((await c.request('/api/link/verify', { user_code: value.user_code })).status, 200);
    assert.equal((await c.request('/api/link/approve', { user_code: value.user_code })).status, 200);
  }
  return { app, mail, client, register, code, redeem, approve };
}

test('registration, explicit approval, single use, profile restoration, renewal, disabled matches, revocation', async t => {
  const f = await fixture(t, { sessionSeconds: 60, sessionUpdateSeconds: 1 });
  const browser = f.client('browser-a'), xbox = f.client('xbox-a'); await f.register(browser, 'raven');
  const code = await f.code(xbox);
  assert.equal(code.device_code.length, 64); assert.equal(code.user_code.length, 8); assert.equal(code.interval, 5);
  assert.equal(new URL(code.verification_uri_complete).searchParams.get('user_code'), code.user_code);
  assert(!code.verification_uri_complete.includes(code.device_code)); assert(code.qr_png_base64.length > 100);
  const pending = await f.redeem(xbox, code); assert.equal(pending.body.error, 'authorization_pending');
  assert.equal((await f.redeem(xbox, code)).body.error, 'slow_down');
  await f.approve(browser, code);
  f.app.db.prepare('UPDATE deviceCode SET lastPolledAt=NULL').run(); // controlled clock state; production polling interval is unchanged
  const tokens = await f.redeem(xbox, code); assert.equal(tokens.status, 200); const token = tokens.body.access_token; assert(token);
  assert.equal((await f.redeem(xbox, code)).status, 400);
  const restored = await f.client('restarted-xbox').request('/api/device/profile', undefined, { bearer: token });
  assert.equal(restored.status, 200); assert.equal(restored.body.publicUsername, 'raven'); assert.equal(restored.body.fortnitePortAvailable, false);
  assert.equal((await xbox.request('/api/device/matchmaking', {}, { bearer: token })).body.message, 'Matches are not available yet');
  const before = Date.now() + 10000; f.app.db.prepare('UPDATE session SET expiresAt=? WHERE token=?').run(before, token);
  const renew = await xbox.request('/api/device/renew', {}, { bearer: token }); assert.equal(renew.status, 200); assert(new Date(renew.body.expires_at).getTime() > before);
  const devices = await browser.request('/api/devices'); assert.equal(devices.body.devices.length, 1);
  assert.equal((await browser.request('/api/devices/revoke', { id: devices.body.devices[0].id })).status, 200);
  assert.equal((await xbox.request('/api/device/profile', undefined, { bearer: token })).status, 401);
});

test('existing-user login, logout, settings and recovery use library password/session handling', async t => {
  const f = await fixture(t); const browser = f.client('browser'); await f.register(browser, 'willow');
  assert.equal((await browser.request('/api/auth/sign-out', {})).status, 200);
  assert.equal((await browser.request('/api/me')).status, 401);
  assert.equal((await browser.request('/api/auth/sign-in/email', { email: 'willow@example.test', password: 'wrong password' })).status, 401);
  const login = await browser.request('/api/auth/sign-in/email', { email: 'willow@example.test', password: 'correct horse battery cloud 77' });
  assert.equal(login.status, 200); assert.equal(login.body.token, undefined); assert.equal(login.headers.get('set-auth-token'), null);
  assert.equal((await browser.request('/api/auth/update-user', { username: 'newwillow', name: 'newwillow' })).status, 200);
  assert.equal((await browser.request('/api/me')).body.user.username, 'newwillow');
  assert.equal((await browser.request('/api/auth/request-password-reset', { email: 'willow@example.test' })).status, 200);
  assert.equal(f.mail.length, 1);
  const recoveryUrl = new URL(f.mail[0].text.match(/http[^\s]+/)[0]);
  // Better Auth reset mail points to its one-use token route; obtain the token from its path.
  const resetToken = new URLSearchParams(recoveryUrl.hash.slice(1)).get('token');
  const result = await browser.request('/api/auth/reset-password', { token: resetToken, newPassword: 'replacement stronger password 99' });
  assert.equal(result.status, 200, result.body?.message);
  assert.equal((await browser.request('/api/me')).status, 401);
  assert.equal((await browser.request('/api/auth/reset-password', { token: resetToken, newPassword: 'replacement stronger password 99' })).status, 400);
  assert.equal((await browser.request('/api/auth/sign-in/email', { email: 'willow@example.test', password: 'replacement stronger password 99' })).status, 200);
});

test('another account cannot approve a claimed code, revoke devices, or obtain device token from user code', async t => {
  const f = await fixture(t); const alice = f.client('alice'), bob = f.client('bob'), xbox = f.client('xbox');
  await f.register(alice, 'alice'); await f.register(bob, 'bob');
  const code = await f.code(xbox);
  assert.equal((await alice.request('/api/link/approve', { user_code: code.user_code })).status, 400); // explicit review required
  assert.equal((await alice.request('/api/link/verify', { user_code: code.user_code })).status, 200);
  assert.equal((await bob.request('/api/link/verify', { user_code: code.user_code })).status, 403);
  assert.equal((await bob.request('/api/link/approve', { user_code: code.user_code })).status, 403);
  assert.equal((await f.redeem(xbox, { device_code: code.user_code })).status, 400);
  assert.equal((await alice.request('/api/link/approve', { user_code: code.user_code })).status, 200);
  const tokens = await f.redeem(xbox, code); assert.equal(tokens.status, 200);
  const list = await alice.request('/api/devices'); assert.equal(list.body.devices.length, 1);
  assert.equal((await bob.request('/api/devices/revoke', { id: list.body.devices[0].id })).status, 404);
  assert.equal((await xbox.request('/api/device/profile', undefined, { bearer: tokens.body.access_token })).status, 200);
});

test('expiry, denial, cancellation, invalid clients, and expired sessions are rejected', async t => {
  const f = await fixture(t); const browser = f.client('browser'), xbox = f.client('xbox'); await f.register(browser, 'ashgrove');
  assert.equal((await xbox.request('/api/device/code', { client_id: 'unknown' })).status, 400);
  const expired = await f.code(xbox); f.app.db.prepare('UPDATE deviceCode SET expiresAt=?').run(Date.now() - 1000);
  assert.equal((await browser.request('/api/link/verify', { user_code: expired.user_code })).body.error, 'expired_token');
  assert.equal((await f.redeem(xbox, expired)).body.error, 'expired_token');
  const denied = await f.code(xbox); await browser.request('/api/link/verify', { user_code: denied.user_code });
  assert.equal((await browser.request('/api/link/deny', { user_code: denied.user_code })).status, 200);
  assert.equal((await f.redeem(xbox, denied)).body.error, 'access_denied');
  const cancelled = await f.code(xbox); await xbox.request('/api/device/cancel', { device_code: cancelled.device_code });
  assert.equal((await f.redeem(xbox, cancelled)).body.error, 'invalid_grant');
  const good = await f.code(xbox); await f.approve(browser, good); const tokens = await f.redeem(xbox, good);
  f.app.db.prepare('UPDATE session SET expiresAt=? WHERE token=?').run(Date.now() - 1000, tokens.body.access_token);
  assert.equal((await xbox.request('/api/device/profile', undefined, { bearer: tokens.body.access_token })).status, 401);
});

test('rate limits persist in SQL and cannot be bypassed by restarting application', async t => {
  const f = await fixture(t); const browser = f.client('browser'); await f.register(browser, 'nightfall');
  let response;
  for (let i = 0; i < 9; i++) response = await browser.request('/api/link/verify', { user_code: 'AAAAAAAA' });
  assert.equal(response.status, 429); assert.equal(response.headers.get('retry-after'), '600');
  assert(f.app.db.prepare('SELECT COUNT(*) AS n FROM request_limit').get().n > 0);
  const xbox = f.client('xbox'); for (let i = 0; i < 11; i++) response = await xbox.request('/api/device/code', { client_id: 'hollowwake-uwp' });
  assert.equal(response.status, 429);
});

test('cross-origin approval, secrets in URLs, browser bearer access and direct provider bypass are blocked', async t => {
  const f = await fixture(t);
  const response = await f.app.fetch(new Request(`${origin}/api/link/approve`, { method: 'POST', headers: { origin: 'https://attacker.invalid', 'content-type': 'application/json' }, body: '{}' }), 'attacker');
  assert.equal(response.status, 403);
  assert.equal((await f.app.fetch(new Request(`${origin}/link?device_code=not-a-real-secret`), 'attacker')).status, 400);
  const anon = f.client('anon'); assert.equal((await anon.request('/api/link/verify', { user_code: 'AAAAAAAA' })).status, 401);
  assert.equal((await anon.request('/api/auth/device/approve', { userCode: 'AAAAAAAA' })).status, 404);
  assert.equal((await anon.request('/api/me', undefined, { bearer: 'not-a-token' })).status, 401);
});

test('production refuses HTTP, weak secrets and missing recovery delivery', () => {
  assert.throws(() => readConfig({ PUBLIC_ORIGIN: 'http://example.com', BETTER_AUTH_SECRET: 'x'.repeat(48) }), /HTTPS/);
  assert.throws(() => readConfig({ PUBLIC_ORIGIN: origin, ALLOW_LOCAL_HTTP: '1', BETTER_AUTH_SECRET: 'short' }), /secret/i);
  assert.throws(() => readConfig({ PUBLIC_ORIGIN: 'https://example.com', BETTER_AUTH_SECRET: 'x'.repeat(48) }), /SMTP/);
});

test('parallel redemption issues only one device session', async t => {
  const f = await fixture(t), browser = f.client('browser'), xbox = f.client('xbox'); await f.register(browser, 'moonrise');
  const value = await f.code(xbox); await f.approve(browser, value);
  const results = await Promise.all([f.redeem(xbox, value), f.redeem(xbox, value)]);
  assert.equal(results.filter(r => r.status === 200).length, 1);
  assert.equal(f.app.db.prepare('SELECT COUNT(*) AS n FROM project_device').get().n, 1);
});

test('verified email is required when configured and password change revokes linked sessions', async t => {
  const f = await fixture(t, { requireEmailVerification: true }); const browser = f.client('browser'), xbox = f.client('xbox');
  await f.register(browser, 'duskbound');
  assert.equal((await browser.request('/api/auth/sign-in/email', { email: 'duskbound@example.test', password: 'correct horse battery cloud 77' })).status, 403);
  assert.equal(f.mail.length, 1);
  const verifyUrl = new URL(f.mail[0].text.match(/http[^\s]+/)[0]);
  const verification = await f.app.fetch(new Request(verifyUrl), 'browser'); assert.equal(verification.status, 302);
  assert.equal((await browser.request('/api/auth/sign-in/email', { email: 'duskbound@example.test', password: 'correct horse battery cloud 77' })).status, 200);
  assert.equal((await browser.request('/api/me')).body.user.emailVerified, true);
  const value = await f.code(xbox); await f.approve(browser, value); const issued = await f.redeem(xbox, value);
  const changed = await browser.request('/api/auth/change-password', { currentPassword: 'correct horse battery cloud 77', newPassword: 'changed stronger password 456' }); assert.equal(changed.status, 200);
  assert.equal((await xbox.request('/api/device/profile', undefined, { bearer: issued.body.access_token })).status, 401);
  const account = f.app.db.prepare('SELECT password FROM account WHERE providerId=?').get('credential');
  assert.notEqual(account.password, 'changed stronger password 456'); assert(account.password.length > 50);
});

test('same database retains rate limits and device restoration after a service restart', async t => {
  const { mkdtempSync, rmSync } = await import('node:fs'); const { tmpdir } = await import('node:os'); const { join } = await import('node:path');
  const dir = mkdtempSync(join(tmpdir(), 'hollowwake-restart-'));
  t.after(() => rmSync(dir, { recursive: true, force: true }));
  const config = readConfig({ PUBLIC_ORIGIN: origin, ALLOW_LOCAL_HTTP: '1', BETTER_AUTH_SECRET: randomBytes(48).toString('hex'), DATABASE_PATH: join(dir, 'db.sqlite') });
  let app = await createApplication(config);
  const send = async (path, data, headers = {}) => {
    const r = await app.fetch(new Request(origin + path, { method: data === undefined ? 'GET' : 'POST', headers: { origin, 'content-type': 'application/json', ...headers }, ...(data === undefined ? {} : { body: JSON.stringify(data) }) }), 'restart-ip');
    return { response: r, body: await r.json() };
  };
  const registration = await send('/api/auth/sign-up/email', { email: 'persistent@example.test', username: 'persistent', name: 'persistent', password: 'persistent strong password 77' });
  assert.equal(registration.response.status, 200);
  const cookie = registration.response.headers.getSetCookie().map(v => v.split(';')[0]).join('; ');
  const code = (await send('/api/device/code', { client_id: 'hollowwake-uwp' })).body;
  await send('/api/link/verify', { user_code: code.user_code }, { cookie }); await send('/api/link/approve', { user_code: code.user_code }, { cookie });
  const tokens = (await send('/api/device/token', { device_code: code.device_code, client_id: 'hollowwake-uwp', grant_type: 'urn:ietf:params:oauth:grant-type:device_code' })).body;
  for (let i = 0; i < 8; i++) await send('/api/link/verify', { user_code: 'AAAAAAAA' }, { cookie });
  app.close(); app = await createApplication(config); t.after(() => app.close());
  const restored = await send('/api/device/profile', undefined, { authorization: `Bearer ${tokens.access_token}` }); assert.equal(restored.response.status, 200); assert.equal(restored.body.publicUsername, 'persistent');
  assert.equal((await send('/api/link/verify', { user_code: 'AAAAAAAA' }, { cookie })).response.status, 429);
});

test('human-code review distinguishes invalid and already redeemed codes without retaining secret codes', async t => {
  const f = await fixture(t), browser = f.client('browser'), xbox = f.client('xbox'); await f.register(browser, 'codekeeper');
  const value = await f.code(xbox); await f.approve(browser, value); assert.equal((await f.redeem(xbox, value)).status, 200);
  const used = await browser.request('/api/link/verify', { user_code: value.user_code }); assert.equal(used.status, 409); assert.equal(used.body.error, 'code_already_used');
  const invalid = await browser.request('/api/link/verify', { user_code: 'AAAAAAAA' }); assert.equal(invalid.status, 400); assert.equal(invalid.body.error, 'invalid_user_code');
  const retained = f.app.db.prepare('SELECT * FROM consumed_link_code').get(); assert.equal(retained.code_fingerprint.length, 64);
  assert(!JSON.stringify(retained).includes(value.user_code)); assert(!JSON.stringify(retained).includes(value.device_code));
});

test('content manifest requires the same live linked-device account, rejects browser/public access and revocation',async t=>{
 const manifest={schema:1,build:'6.21-CL-4526925',archive:{url:'https://public-build-archives-bucket.erafn.dev/6.21.zip',bytes:12528679454,sha256:'bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5'},files:[{path:'6.21/FortniteGame/Content/Hollowwake-owned-fixture.txt',bytes:0,sha256:'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'}]};
 const f=await fixture(t,{contentManifest:manifest});const browser=f.client('content-browser'),xbox=f.client('content-device');await f.register(browser,'contentuser');
 assert.equal((await xbox.request('/api/device/content/6.21')).status,401);assert.equal((await browser.request('/api/device/content/6.21')).status,401);
 const c=await f.code(xbox);await f.approve(browser,c);const issued=await f.redeem(xbox,c);assert.equal(issued.status,200);const access=issued.body.access_token;
 const response=await xbox.request('/api/device/content/6.21',undefined,{bearer:access});assert.equal(response.status,200);assert.equal(response.body.files.length,1);assert.match(response.body.manifestId,/^[a-f0-9]{64}$/);assert(response.body.expiresAt>Date.now());assert(!JSON.stringify(response.body).includes(access));assert.equal(response.headers.get('cache-control'),'no-store');
 const devices=await browser.request('/api/devices');await browser.request('/api/devices/revoke',{id:devices.body.devices[0].id});assert.equal((await xbox.request('/api/device/content/6.21',undefined,{bearer:access})).status,401);
});

test('content policy rejects code, traversal, duplicates and substituted archive sources',async()=>{
 const {validateContentManifest,contentManifestResponse}=await import('../backend/content621.mjs');
 assert.equal(contentManifestResponse(null).status,503);
 const good={schema:1,build:'6.21-CL-4526925',archive:{url:'https://public-build-archives-bucket.erafn.dev/6.21.zip',bytes:12528679454,sha256:'bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5'},files:[{path:'6.21/FortniteGame/Content/owned.txt',bytes:0,sha256:'e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855'}]};
 for(const path of ['6.21/FortniteGame/Content/../owned.pak','6.21/FortniteGame/Content/evil.dll','6.21/FortniteGame/Content/NUL.txt','6.21/FortniteGame/Content/a.pak:stream'])assert.throws(()=>validateContentManifest({...good,files:[{...good.files[0],path}]}));
 assert.throws(()=>validateContentManifest({...good,files:[...good.files,...good.files]}));assert.throws(()=>validateContentManifest({...good,archive:{...good.archive,url:'https://example.test/credential-stealer'}}));
});
