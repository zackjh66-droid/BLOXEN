import {contentManifestResponse} from './content621.mjs';
import { createHmac } from 'node:crypto';
import QRCode from 'qrcode';
import { CLIENT_ID } from './auth-options.mjs';
const security = { 'Cache-Control': 'no-store', 'Referrer-Policy': 'no-referrer', 'X-Content-Type-Options': 'nosniff',
  'Content-Security-Policy': "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'; form-action 'self'; base-uri 'none'",
  'Permissions-Policy': 'camera=(), microphone=(), geolocation=()' };
const json = (body, status = 200, headers = {}) => new Response(JSON.stringify(body), { status, headers: { ...security, 'Content-Type': 'application/json', ...headers } });
const publicUser = user => ({ id: user.id, username: user.username || user.name, email: user.email, emailVerified: user.emailVerified });
const hasProjectCookie = request => (request.headers.get('cookie') || '').split(';').some(value => /^\s*(?:__Secure-|__Host-)?hollowwake[._-]/i.test(value));
const failure = (code, status = 400, message = code) => Object.assign(new Error(message), { code, status });
export function createCore(config, { db, auth, readAsset, close, clientAdapter }) {
  const limitStmt = db.prepare(`INSERT INTO request_limit(bucket,started_at,attempts) VALUES(?,?,1)
    ON CONFLICT(bucket) DO UPDATE SET attempts=CASE WHEN request_limit.started_at<=? THEN 1 ELSE request_limit.attempts+1 END,
    started_at=CASE WHEN request_limit.started_at<=? THEN excluded.started_at ELSE request_limit.started_at END RETURNING attempts`);
  async function limit(key, max = 20, windowSeconds = 600) {
    const now = Date.now(), cutoff = now - windowSeconds * 1000;
    const bucket = createHmac('sha256', config.secret).update(key).digest('hex');
    const row = await limitStmt.get(bucket, now, cutoff, cutoff);
    if (row.attempts > max) throw failure('rate_limited', 429, 'Too many attempts. Wait before trying again.');
  }
  async function body(request) {
    const type = request.headers.get('content-type') || '';
    if (!type.startsWith('application/json')) throw failure('json_required', 415);
    const text = await request.text(); if (Buffer.byteLength(text) > 16384) throw failure('request_too_large', 413);
    try { const data = JSON.parse(text); if (!data || Array.isArray(data) || typeof data !== 'object') throw Error(); return data; }
    catch { throw failure('invalid_json'); }
  }
  function browserOrigin(request) {
    if (request.headers.get('origin') !== config.origin || request.headers.has('authorization')) throw failure('invalid_browser_origin', 403);
  }
  async function session(request, device = false) {
    if (device && (!request.headers.get('authorization')?.startsWith('Bearer ') || hasProjectCookie(request))) throw failure('device_bearer_required', 401);
    if (!device && (request.headers.has('authorization') || !request.headers.has('cookie'))) throw failure('browser_sign_in_required', 401);
    const response = await auth.handler(new Request(`${config.origin}/api/auth/get-session`, { headers: request.headers }));
    const data = await response.json();
    if (!data?.session || !data?.user) throw failure('session_expired_or_revoked', 401);
    const moderation=await db.prepare('SELECT banned_until FROM project_moderation WHERE user_id=?').get(data.user.id);
    if(moderation?.banned_until>Date.now()) throw failure('account_suspended',403,'This Hollowwake account is suspended.');
    const linked = await db.prepare('SELECT * FROM project_device WHERE session_id=?').get(data.session.id);
    if (device) {
      if (!linked) throw failure('not_a_device_session', 401);
      if (Date.now() - linked.created_at > config.maxDeviceDays * 86400000) {
        await db.prepare('DELETE FROM session WHERE id=?').run(data.session.id);
        throw failure('session_expired_or_revoked', 401);
      }
      await db.prepare('UPDATE project_device SET last_seen_at=? WHERE session_id=?').run(Date.now(), data.session.id);
    } else if (linked) throw failure('browser_sign_in_required', 403);
    return { ...data, responseHeaders: response.headers };
  }
  function withSession(body, current) {
    const response = json(body);
    for (const cookie of current.responseHeaders.getSetCookie()) response.headers.append('Set-Cookie', cookie);
    return response;
  }
  async function forward(path, data, request, { browser = false } = {}) {
    const headers = new Headers({ 'Content-Type': 'application/json' });
    for (const name of ['origin', 'cookie', 'user-agent']) {
      // Native/public device grants never carry browser cookies into the provider.
      if (name === 'cookie' && ['/device/code', '/device/token'].includes(path)) continue;
      if (request.headers.has(name)) headers.set(name, request.headers.get(name));
    }
    const response = await auth.handler(new Request(`${config.origin}/api/auth${path}`, { method: 'POST', headers, body: JSON.stringify(data) }));
    const result = await response.json();
    if (browser) { delete result.token; delete result.access_token; }
    const out = json(result, response.status);
    for (const cookie of response.headers.getSetCookie()) out.headers.append('Set-Cookie', cookie);
    return out;
  }
  const files = new Map([
    ['/', ['index.html', 'text/html; charset=utf-8']], ['/link', ['index.html', 'text/html; charset=utf-8']],
    ['/recover', ['index.html', 'text/html; charset=utf-8']], ['/reset', ['index.html', 'text/html; charset=utf-8']],
    ['/settings', ['index.html', 'text/html; charset=utf-8']], ['/app.js', ['app.js', 'text/javascript']],
    ['/style.css', ['style.css', 'text/css']], ['/logo.svg', ['logo.svg', 'image/svg+xml']], ['/favicon.png', ['favicon.png', 'image/png']], ['/assets/landmark.jpg', ['assets/landmark.jpg', 'image/jpeg']]
  ]);
  async function handle(request, ip = 'local') {
    const url = new URL(request.url), path = url.pathname;
    try {
      if (url.origin !== config.origin) throw failure('invalid_host', 400);
      // Secrets are accepted only in POST bodies or Authorization; never in public URLs.
      for (const key of ['device_code', 'access_token', 'refresh_token', 'password','exchange_code']) if (url.searchParams.has(key)) throw failure('secret_in_url', 400);
      if (request.headers.has('origin') && request.headers.get('origin') !== config.origin) throw failure('invalid_origin', 403);
      if(path.startsWith('/client621/')) {if(!clientAdapter)return json({error:'adapter_disabled'},404);await limit(`client621:${ip}`,120,60);return clientAdapter.fetch(request);}
      if (request.method !== 'GET' && request.method !== 'POST') throw failure('method_not_allowed', 405);
      if (path.startsWith('/api/')) await limit(`global:${ip}`, 240, 60);
      if (request.method === 'GET' && files.has(path)) {
        const [name, type] = files.get(path); return new Response(await readAsset(name), { headers: { ...security, 'Content-Type': type } });
      }
      if (path === '/api/status' && request.method === 'GET') return json({ project: 'Hollowwake', unofficial: true, clientId: CLIENT_ID, origin: config.origin,
        localDevelopment: config.local, emailDelivery: config.emailDelivery || (config.smtp ? 'smtp' : 'private-local-outbox'), fortnitePortAvailable: false, matchmakingAvailable: false });
      if (path === '/api/me' && request.method === 'GET') {
        const current = await session(request); return withSession({ user: publicUser(current.user), expiresAt: current.session.expiresAt }, current);
      }
      if (path === '/api/devices' && request.method === 'GET') {
        const current = await session(request);
        const devices = await db.prepare(`SELECT d.session_id AS id,d.label,d.created_at AS createdAt,d.last_seen_at AS lastSeenAt,s.expiresAt
          FROM project_device d JOIN session s ON s.id=d.session_id WHERE d.user_id=? AND s.expiresAt>? ORDER BY d.created_at DESC`).all(current.user.id, Date.now());
        return withSession({ devices }, current);
      }
      if (path === '/api/project/inventory' && request.method === 'GET') {
        const current=await session(request,request.headers.has('authorization'));
        const inventory=await db.prepare('SELECT cosmetic_id AS cosmeticId,granted_at AS grantedAt FROM project_inventory WHERE user_id=? ORDER BY cosmetic_id').all(current.user.id);
        return json({inventory,scope:'Hollowwake project records only; not connected to the Fortnite client',fortniteIntegration:false});
      }
      if(path==='/api/device/content/6.21'&&request.method==='GET'){await session(request,true);return contentManifestResponse(config.contentManifest);}
      if (path === '/api/device/profile' && request.method === 'GET') {
        const current = await session(request, true);
        return json({ accountId: current.user.id, publicUsername: current.user.username || current.user.name, sessionExpiresAt: current.session.expiresAt,
          profile: { profileId: 'athena', revision: 1, displayName: current.user.username || current.user.name, items: {}, testProfile: true },
          clientIntegration: 'not_implemented', fortnitePortAvailable: false, matchmakingAvailable: false, message: 'Matches are not available yet' });
      }
      if (path === '/api/auth/verify-email' && request.method === 'GET') {
        // Provider validates its own email-verification token. Only same-origin callback allowed.
        url.searchParams.set('callbackURL', `${config.origin}/settings`);
        const response = await auth.handler(new Request(url, { headers: request.headers }));
        for (const [key, value] of Object.entries(security)) response.headers.set(key, value);
        return response;
      }
      if (request.method !== 'POST') throw failure('not_found', 404);
      const data = await body(request);
      if(path==='/api/client-adapter/exchange'){const current=await session(request,true);await limit(`adapter-exchange:${current.user.id}`,10,60);if(!clientAdapter)throw failure('adapter_disabled',404);return json(await clientAdapter.issue(current.user,current.session.id));}
      if (path === '/api/device/code') {
        if (hasProjectCookie(request)) throw failure('device_request_requires_no_browser_cookie', 400);
        await limit(`issue:${ip}`, 10, 600);
        await db.prepare('DELETE FROM consumed_link_code WHERE expires_at<=?').run(Date.now());
        if (Object.keys(data).some(k => !['client_id'].includes(k)) || data.client_id !== CLIENT_ID) throw failure('invalid_client', 400);
        const response = await forward('/device/code', { client_id: CLIENT_ID, scope: 'profile lobby:test' }, request);
        if (!response.ok) return response;
        const code = await response.json();
        const qrUrl = new URL('/link', config.origin); qrUrl.searchParams.set('user_code', code.user_code);
        code.verification_uri_complete = qrUrl.href;
        code.qr_png_base64 = (await QRCode.toDataURL(qrUrl.href, { margin: 2, width: 320, errorCorrectionLevel: 'M' })).split(',')[1];
        return json(code);
      }
      if (path === '/api/device/token') {
        if (hasProjectCookie(request)) throw failure('device_request_requires_no_browser_cookie', 400);
        await limit(`poll:${ip}`, 180, 60);
        const pendingRecord = typeof data.device_code === 'string' ? await db.prepare('SELECT userCode,expiresAt,userId FROM deviceCode WHERE deviceCode=?').get(data.device_code) : null;
        if(pendingRecord?.userId){const ban=await db.prepare('SELECT banned_until FROM project_moderation WHERE user_id=?').get(pendingRecord.userId);if(ban?.banned_until>Date.now())throw failure('account_suspended',403);}
        const response = await forward('/device/token', data, request);
        if (!response.ok) return response;
        const result = await response.json();
        if (pendingRecord) {
          const fingerprint = createHmac('sha256', config.secret).update(pendingRecord.userCode).digest('hex');
          await db.prepare('INSERT OR REPLACE INTO consumed_link_code(code_fingerprint,expires_at) VALUES(?,?)').run(fingerprint, new Date(pendingRecord.expiresAt).getTime());
        }
        const row = await db.prepare('SELECT id,userId FROM session WHERE token=?').get(result.access_token);
        if (!row) throw failure('token_session_missing', 500);
        const now = Date.now();
        await db.prepare('INSERT INTO project_device(session_id,user_id,created_at,last_seen_at) VALUES(?,?,?,?)').run(row.id, row.userId, now, now);
        return json(result);
      }
      if (path === '/api/device/cancel') {
        if (typeof data.device_code !== 'string' || data.device_code.length > 191) throw failure('invalid_request');
        await db.prepare('DELETE FROM deviceCode WHERE deviceCode=? AND clientId=?').run(data.device_code, CLIENT_ID);
        return json({ success: true });
      }
      if (path === '/api/device/renew') {
        const current = await session(request, true); // Provider rolls expiry after configured updateAge; checks DB every request.
        return json({ expires_at: current.session.expiresAt, token_type: 'Bearer', valid: true, renewal_policy: 'rolling_after_update_age' });
      }
      if (path === '/api/device/logout') {
        const current = await session(request, true); await db.prepare('DELETE FROM session WHERE id=?').run(current.session.id); return json({ success: true });
      }
      if (path === '/api/device/matchmaking') { await session(request, true); return json({ error: 'matches_unavailable', message: 'Matches are not available yet' }, 409); }
      if (['/api/link/verify', '/api/link/approve', '/api/link/deny'].includes(path)) {
        // Code typos must not lock a signed-in user out of reviewing or approving a valid code.
        // Existing global traffic and authentication-provider limits still apply.
        browserOrigin(request); const current = await session(request);
        const userCode = String(data.user_code || '').replace(/[-\s]/g, '').toUpperCase();
        if (!/^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{8}$/.test(userCode)) throw failure('invalid_user_code');
        if (path.endsWith('/verify')) {
          const fingerprint = createHmac('sha256', config.secret).update(userCode).digest('hex');
          if (await db.prepare('SELECT 1 FROM consumed_link_code WHERE code_fingerprint=? AND expires_at>?').get(fingerprint, Date.now())) throw failure('code_already_used', 409);
          const providerUrl = new URL('/api/auth/device', config.origin); providerUrl.searchParams.set('user_code', userCode);
          const response = await auth.handler(new Request(providerUrl, { headers: request.headers }));
          const result = await response.json();
          if (!response.ok && result.error === 'invalid_request') return json({ error: 'invalid_user_code', message: 'This device code is invalid.' }, 400);
          if (response.ok && !result.client_id) throw failure('code_claimed_by_another_account', 403);
          if (response.ok && result.status !== 'pending') throw failure('code_already_used', 409);
          return json(result, response.status);
        }
        return forward(path.endsWith('/approve') ? '/device/approve' : '/device/deny', { userCode }, request);
      }
      if (path === '/api/devices/revoke') {
        browserOrigin(request); const current = await session(request);
        if (typeof data.id !== 'string') throw failure('invalid_device');
        const owned = await db.prepare('SELECT session_id FROM project_device WHERE session_id=? AND user_id=?').get(data.id, current.user.id);
        if (!owned) throw failure('device_not_found', 404);
        await db.prepare('DELETE FROM session WHERE id=? AND userId=?').run(data.id, current.user.id); return json({ success: true });
      }
      const browserAuth = new Map([
        ['/api/auth/sign-up/email', ['/sign-up/email', ['email', 'password', 'name', 'username']]],
        ['/api/auth/sign-in/email', ['/sign-in/email', ['email', 'password', 'rememberMe']]],
        ['/api/auth/sign-out', ['/sign-out', []]],
        ['/api/auth/request-password-reset', ['/request-password-reset', ['email']]],
        ['/api/auth/reset-password', ['/reset-password', ['token', 'newPassword']]],
        ['/api/auth/change-password', ['/change-password', ['currentPassword', 'newPassword', 'revokeOtherSessions']]],
        ['/api/auth/update-user', ['/update-user', ['name', 'username']]],
        ['/api/auth/send-verification-email', ['/send-verification-email', ['email']]]
      ]);
      if (browserAuth.has(path)) {
        if (config.emailDelivery === 'unavailable' && ['/api/auth/request-password-reset','/api/auth/reset-password','/api/auth/send-verification-email'].includes(path)) throw failure('email_delivery_unavailable', 503, 'Recovery and verification email are not configured. Keep your password safe.');
        browserOrigin(request); await limit(`browser-auth:${ip}`, 15, 600);
        const [endpoint, fields] = browserAuth.get(path), clean = Object.fromEntries(fields.filter(k => data[k] !== undefined).map(k => [k, data[k]]));
        if (['/sign-up/email', '/sign-in/email'].includes(endpoint)) clean.callbackURL = `${config.origin}/settings`;
        if (endpoint === '/request-password-reset') clean.redirectTo = `${config.origin}/reset`;
        if (endpoint === '/send-verification-email') clean.callbackURL = `${config.origin}/settings`;
        if (['/change-password', '/update-user', '/sign-out'].includes(endpoint)) await session(request);
        if (endpoint === '/change-password') clean.revokeOtherSessions = true;
        return forward(endpoint, clean, request, { browser: true });
      }
      // No Fortnite production protocol or mock lobby endpoints are exposed.
      throw failure('not_found', 404);
    } catch (error) {
      const status = error.status || 500;
      return json({ error: error.code || 'internal_error', message: status === 500 ? 'Request failed. Retry or contact the operator.' : error.message }, status,
        status === 429 ? { 'Retry-After': '600' } : {});
    }
  }
  return { fetch: handle, auth, db, close };
}
