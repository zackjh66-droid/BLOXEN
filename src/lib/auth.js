'use strict';
const crypto = require('crypto'); const cfg = require('./config');
const sha = s => crypto.createHash('sha256').update(s).digest('hex');
const rand = (n = 32) => crypto.randomBytes(n).toString('base64url');
const N = 1 << 15, R = 8, P = 1, KEYLEN = 64;
// scrypt with per-password salt; stored as scrypt$N$r$p$salt$hash. Constant-time comparison.
function hashPassword(pw) { return new Promise((res, rej) => { const salt = crypto.randomBytes(16); crypto.scrypt(pw, salt, KEYLEN, { N, r: R, p: P, maxmem: 128 * N * R * 2 }, (e, k) => e ? rej(e) : res(`scrypt$${N}$${R}$${P}$${salt.toString('base64')}$${k.toString('base64')}`)); }); }
function verifyPassword(pw, stored) { return new Promise(res => { const [alg, n, r, p, salt, hash] = String(stored).split('$'); if (alg !== 'scrypt') return res(false); const exp = Buffer.from(hash, 'base64');
  crypto.scrypt(pw, Buffer.from(salt, 'base64'), exp.length, { N: +n, r: +r, p: +p, maxmem: 128 * +n * +r * 2 }, (e, k) => res(!e && k.length === exp.length && crypto.timingSafeEqual(k, exp))); }); }
const DUMMY = '';
function validUsername(u) { return typeof u === 'string' && /^[A-Za-z0-9](?:[A-Za-z0-9_]{1,18})[A-Za-z0-9]$/.test(u) && !/__/.test(u); }
function validPassword(p) { return typeof p === 'string' && p.length >= 8 && p.length <= 200; }
function createSession(db, userId) { const token = rand(32); const csrf = rand(24); const now = Date.now(); db.prepare('INSERT INTO sessions(token_hash,user_id,csrf,created_at,expires_at) VALUES(?,?,?,?,?)').run(sha(token), userId, csrf, now, now + cfg.sessionTtlMs); return { token, csrf }; }
function getSession(db, token) { if (!token || token.length > 100) return null; const row = db.prepare('SELECT s.csrf, s.expires_at, u.* FROM sessions s JOIN users u ON u.id=s.user_id WHERE s.token_hash=?').get(sha(token)); if (!row) return null; if (row.expires_at < Date.now()) { destroySession(db, token); return null; } return row; }
function destroySession(db, token) { if (token) db.prepare('DELETE FROM sessions WHERE token_hash=?').run(sha(token)); }
// In-memory login throttle: 8 failures / 10 minutes per (ip, username)
const fails = new Map();
function throttled(key) { const a = (fails.get(key) || []).filter(t => Date.now() - t < 600_000); fails.set(key, a); return a.length >= 8; }
function noteFail(key) { const a = fails.get(key) || []; a.push(Date.now()); fails.set(key, a); }
function clearFails(key) { fails.delete(key); }
// Opaque single-use tickets. Only SHA-256 of the ticket is stored.
function issueTicket(db, table, { userId, placeId, serverId, ttl }) { const t = rand(24); const now = Date.now(); db.prepare(`INSERT INTO ${table}(hash,user_id,place_id,server_id,created_at,expires_at) VALUES(?,?,?,?,?,?)`).run(sha(t), userId, placeId, serverId, now, now + ttl); return t; }
function redeemTicket(db, table, ticket, expect = {}) {
  if (typeof ticket !== 'string' || !/^[A-Za-z0-9_-]{20,64}$/.test(ticket)) return null; const h = sha(ticket);
  const r = db.prepare(`UPDATE ${table} SET used_at=? WHERE hash=? AND used_at IS NULL AND expires_at>?${expect.serverId ? ' AND server_id=?' : ''}${expect.placeId ? ' AND place_id=?' : ''} RETURNING user_id, place_id, server_id`).get(...[Date.now(), h, Date.now(), ...(expect.serverId ? [expect.serverId] : []), ...(expect.placeId ? [expect.placeId] : [])]);
  return r || null; // UPDATE..RETURNING makes redemption atomic: a ticket can never be used twice
}
module.exports = { hashPassword, verifyPassword, validUsername, validPassword, createSession, getSession, destroySession, throttled, noteFail, clearFails, issueTicket, redeemTicket, rand, sha };
