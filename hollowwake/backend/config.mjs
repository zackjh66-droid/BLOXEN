import { resolve } from 'node:path';
export function readConfig(env = process.env) {
  const origin = env.PUBLIC_ORIGIN || 'http://127.0.0.1:8787';
  const url = new URL(origin);
  const local = ['localhost', '127.0.0.1', '[::1]'].includes(url.hostname);
  if (url.pathname !== '/' || url.search || url.hash) throw new Error('PUBLIC_ORIGIN must contain only scheme and host');
  if (url.protocol !== 'https:' && !(env.ALLOW_LOCAL_HTTP === '1' && local)) throw new Error('HTTPS required; HTTP is allowed only on loopback with ALLOW_LOCAL_HTTP=1');
  if (!env.BETTER_AUTH_SECRET || env.BETTER_AUTH_SECRET.length < 32) throw new Error('Set a random BETTER_AUTH_SECRET of at least 32 characters');
  if (!local && (!env.SMTP_HOST || !env.SMTP_FROM)) throw new Error('Hosted use requires SMTP_HOST and SMTP_FROM for verification and recovery');
  return { contentManifest:env.HOLLOWWAKE_CONTENT_MANIFEST_JSON, origin: url.origin, local, secret: env.BETTER_AUTH_SECRET, adminRpcSecret: env.ADMIN_RPC_SECRET, fortnite621AdapterEnabled: env.FORTNITE621_ADAPTER_ENABLED==='1',
    dbPath: resolve(env.DATABASE_PATH || './data/hollowwake.sqlite'), host: env.HOST || '127.0.0.1', port: Number(env.PORT || 8787),
    trustProxy: env.TRUST_PROXY === '1', smtp: env.SMTP_HOST ? { host: env.SMTP_HOST, port: Number(env.SMTP_PORT || 587), secure: env.SMTP_SECURE === '1', auth: env.SMTP_USER ? { user: env.SMTP_USER, pass: env.SMTP_PASSWORD } : undefined } : null,
    smtpFrom: env.SMTP_FROM, deviceExpires: '10m', pollInterval: '5s', sessionSeconds: 7 * 86400, sessionUpdateSeconds: 86400,
    maxDeviceDays: 30, requireEmailVerification: !local || env.REQUIRE_EMAIL_VERIFICATION === '1' };
}
