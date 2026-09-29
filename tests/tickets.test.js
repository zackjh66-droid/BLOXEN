'use strict';
// UNIT tests for launch/join tickets (src/lib/auth.js): random, short-lived, single-use, scoped to account + game + server, stored only as a hash.
const test = require('node:test'); const assert = require('node:assert/strict'); const os = require('os'); const path = require('path'); const fs = require('fs');
const db0 = require('../src/lib/db'); const auth = require('../src/lib/auth');
const mkdb = () => db0.open(path.join(fs.mkdtempSync(path.join(os.tmpdir(), 'bx-tk-')), 't.sqlite'));

test('ticket is random, URL-safe, >=128 bits, unique over many issues, and only its SHA-256 is stored', () => {
  const db = mkdb(); const seen = new Set();
  for (let i = 0; i < 500; i++) { const t = auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 60000 }); assert.match(t, /^[A-Za-z0-9_-]{20,64}$/); assert.ok(t.length >= 32, '24 random bytes -> 32 base64url chars'); assert.ok(!seen.has(t)); seen.add(t); }
  const t = [...seen][0]; const rows = db.prepare('SELECT * FROM launch_tickets').all(); assert.equal(rows.length, 500);
  assert.ok(rows.every(r => r.hash !== t && /^[0-9a-f]{64}$/.test(r.hash)), 'hash, not plaintext'); assert.equal(JSON.stringify(rows).includes(t), false);
});
test('single use: the second redeem fails, even immediately, and concurrent-style double redeem yields exactly one success', () => {
  const db = mkdb(); const t = auth.issueTicket(db, 'launch_tickets', { userId: 5, placeId: 'p', serverId: 's', ttl: 60000 });
  const results = [auth.redeemTicket(db, 'launch_tickets', t), auth.redeemTicket(db, 'launch_tickets', t), auth.redeemTicket(db, 'launch_tickets', t)];
  assert.equal(results.filter(Boolean).length, 1); assert.deepEqual({ ...results[0] }, { user_id: 5, place_id: 'p', server_id: 's' });
});
test('expiry: an expired ticket is refused and stays refused; default TTL is 60 s or less', async () => {
  const db = mkdb(); const t = auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 40 });
  await new Promise(r => setTimeout(r, 80)); assert.equal(auth.redeemTicket(db, 'launch_tickets', t), null);
  const z = auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 0 }); assert.equal(auth.redeemTicket(db, 'launch_tickets', z), null, 'ttl 0 is already expired');
  const cfg = require('../src/lib/config'); assert.ok(cfg.launchTicketTtlMs <= 60_000 && cfg.joinTokenTtlMs <= 60_000);
  const fresh = auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 5000 }); assert.ok(auth.redeemTicket(db, 'launch_tickets', fresh));
});
test('scope: wrong server or wrong place is refused AND does not burn the ticket for the right server', () => {
  const db = mkdb(); const t = auth.issueTicket(db, 'join_tokens', { userId: 9, placeId: 'place-A', serverId: 'srv-1', ttl: 60000 });
  assert.equal(auth.redeemTicket(db, 'join_tokens', t, { serverId: 'srv-2', placeId: 'place-A' }), null); assert.equal(auth.redeemTicket(db, 'join_tokens', t, { serverId: 'srv-1', placeId: 'place-B' }), null);
  assert.deepEqual({ ...auth.redeemTicket(db, 'join_tokens', t, { serverId: 'srv-1', placeId: 'place-A' }) }, { user_id: 9, place_id: 'place-A', server_id: 'srv-1' });
});
test('account scope: the redeemed row carries exactly the issuing account; tickets of two accounts never cross', () => {
  const db = mkdb(); const a = auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 60000 }); const b = auth.issueTicket(db, 'launch_tickets', { userId: 2, placeId: 'p', serverId: 's', ttl: 60000 });
  assert.equal(auth.redeemTicket(db, 'launch_tickets', b).user_id, 2); assert.equal(auth.redeemTicket(db, 'launch_tickets', a).user_id, 1);
});
test('table isolation: a launch ticket is not a join token and vice versa', () => {
  const db = mkdb(); const l = auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 60000 }); const j = auth.issueTicket(db, 'join_tokens', { userId: 1, placeId: 'p', serverId: 's', ttl: 60000 });
  assert.equal(auth.redeemTicket(db, 'join_tokens', l), null); assert.equal(auth.redeemTicket(db, 'launch_tickets', j), null); assert.ok(auth.redeemTicket(db, 'launch_tickets', l)); assert.ok(auth.redeemTicket(db, 'join_tokens', j));
});
test('malformed and hostile ticket values never reach SQL and never match', () => {
  const db = mkdb(); auth.issueTicket(db, 'launch_tickets', { userId: 1, placeId: 'p', serverId: 's', ttl: 60000 });
  for (const bad of [undefined, null, 0, {}, [], '', 'short', "' OR 1=1 --", 'a'.repeat(200), 'x'.repeat(24) + '\n', '%'.repeat(30), ['a'.repeat(30)], { toString: () => 'a'.repeat(30) }]) assert.equal(auth.redeemTicket(db, 'launch_tickets', bad), null);
  assert.equal(db.prepare('SELECT COUNT(*) n FROM launch_tickets WHERE used_at IS NOT NULL').get().n, 0);
});
