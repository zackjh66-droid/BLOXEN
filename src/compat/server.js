'use strict';
// BLOXEN 2015-client compatibility backend. Local only. It answers the endpoints the 0.205.0.61876 client was observed (by static string analysis)
// to request. It NEVER proxies to Roblox. Response FORMATS are INFERRED from recollection of the public-era services unless a comment says otherwise;
// none have been exercised by the real client. All of this is UNIT/SIMULATOR-TESTED at most.
const fs = require('fs'); const path = require('path'); const crypto = require('crypto');
const cfg = require('../lib/config'); const auth = require('../lib/auth'); const S = require('../lib/services'); const { esc, readBody, SEC_HEADERS } = require('../lib/http');

const numId = id => crypto.createHash('sha256').update('bloxen-place:' + id).digest().readUInt32BE(0) & 0x7fffffff; // stable numeric place id for the client's placeId argument
const xmlEsc = esc;
const LOOPBACK_HOSTS = new Set(['localhost', '127.0.0.1', '[::1]', ...String(process.env.BLOXEN_COMPAT_HOSTS || '').split(',').filter(Boolean)]);

function createCompatApp({ db, gameservers, log = () => {}, compatBase }) {
  const base = () => compatBase();
  const reply = (res, status, body, type = 'text/plain; charset=utf-8', extra = {}) => { res.writeHead(status, { ...SEC_HEADERS, 'Content-Type': type, 'Cache-Control': 'no-store', ...extra }); res.end(body); };
  const json = (res, o, st = 200) => reply(res, st, JSON.stringify(o), 'application/json; charset=utf-8');
  const note = (p, n) => db.prepare('INSERT INTO compat_log(at,path,note) VALUES(?,?,?)').run(Date.now(), p, n || '');
  const placeByNum = n => db.prepare('SELECT * FROM places').all().find(p => numId(p.id) === n);
  const assetUrls = userId => { const a = S.avatarGet(db, userId); return [`${base()}/Asset/BodyColors.ashx?userId=${userId}`, ...a.items.filter(i => [2, 8, 11, 12, 17, 18].includes(i.type_id)).map(i => `${base()}/asset/?id=${i.asset_id}`)]; };

  function joinScript(token, row, user) {
    const srv = [...gameservers.servers.values()].find(s => s.serverId === row.server_id); if (!srv) return null;
    // UNSIGNED. The real client verifies `--rbxsig%...%` on scripts; whether it accepts this unsigned bootstrap is UNKNOWN (Windows validation only).
    return `-- BLOXEN join script (unsigned, INFERRED structure)
local baseUrl = ${JSON.stringify(base())}
game:GetService("ContentProvider"):SetBaseUrl(baseUrl .. "/")
pcall(function() settings().Network.MtuOverride = 1400 end)
local client = game:GetService("NetworkClient")
local player = game:GetService("Players"):CreateLocalPlayer(${user.id})
player.Name = ${JSON.stringify(user.username)}
player.CharacterAppearance = ${JSON.stringify(`${base()}/Asset/CharacterFetch.ashx?userId=${user.id}`)}
client:PlayerConnect(${user.id}, "127.0.0.1", ${srv.port}, 0, 20)
-- joinTicket: ${token}
`;
  }

  return async function handle(req, res) {
    try {
      const host = String(req.headers.host || '').replace(/:\d+$/, '').toLowerCase(); if (!LOOPBACK_HOSTS.has(host)) { note('(blocked host)', host); return reply(res, 421, 'Misdirected request'); }
      const url = new URL(req.url, 'http://x'); const p = url.pathname.toLowerCase(); const q = new URLSearchParams([...url.searchParams].map(([k, v]) => [k.toLowerCase(), v])); note(url.pathname);

      if (p === '/launcher/redeem' && req.method === 'POST') { // called by the local launcher only: custom header, no browser Origin
        if (req.headers.origin || req.headers['x-bloxen-launcher'] !== '1') return reply(res, 403, 'forbidden'); let body; try { body = JSON.parse((await readBody(req, 4096)).toString('utf8')); } catch { return reply(res, 400, 'bad json'); }
        const t = auth.redeemTicket(db, 'launch_tickets', body.ticket); if (!t) return json(res, { error: 'invalid, expired or already used ticket' }, 403);
        const place = S.placeById(db, t.place_id); const user = S.userById(db, t.user_id); if (!place || !user) return json(res, { error: 'gone' }, 410);
        const join = auth.issueTicket(db, 'join_tokens', { userId: user.id, placeId: place.id, serverId: t.server_id, ttl: cfg.joinTokenTtlMs });
        return json(res, { placeId: numId(place.id), userId: user.id, userName: user.username, baseUrl: base(), joinScriptUrl: `${base()}/Game/Join.ashx?t=${join}`, authenticationUrl: `${base()}/Login/Negotiate.ashx`, authenticationTicket: join, clientVersion: cfg.clientVersion });
      }
      if (p === '/game/placelauncher.ashx') { // INFERRED response shape: {jobId,status,joinScriptUrl,authenticationUrl,authenticationTicket,message}
        const t = q.get('t'); const row = peekToken(t); if (!row) return json(res, { jobId: null, status: 0, joinScriptUrl: null, authenticationUrl: null, authenticationTicket: null, message: 'No valid BLOXEN join token' }, 403);
        return json(res, { jobId: row.server_id, status: 2, joinScriptUrl: `${base()}/Game/Join.ashx?t=${t}`, authenticationUrl: `${base()}/Login/Negotiate.ashx`, authenticationTicket: t, message: null });
      }
      if (p === '/game/join.ashx' || p === '/game/visit.ashx') {
        const t = q.get('t'); const row = peekToken(t); if (!row) return reply(res, 403, 'invalid join token'); const user = S.userById(db, row.user_id); const s = joinScript(t, row, user); if (!s) return reply(res, 410, 'server gone'); return reply(res, 200, s);
      }
      if (p === '/login/negotiate.ashx') return reply(res, 200, 'BLOXEN-LOCAL'); // INFERRED; no cookie is issued or accepted
      if (p === '/asset/characterfetch.ashx') { const id = +q.get('userid'); if (!S.userById(db, id)) return reply(res, 404, 'no such user'); return reply(res, 200, assetUrls(id).join(';')); }
      if (p === '/asset/bodycolors.ashx') { const id = +q.get('userid'); const u = S.userById(db, id); if (!u) return reply(res, 404, 'no such user'); const c = S.avatarGet(db, id).colors;
        return reply(res, 200, `<roblox xmlns:xmime="http://www.w3.org/2005/05/xmlmime" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://www.roblox.com/roblox.xsd" version="4"><External>null</External><External>nil</External><Item class="BodyColors" referent="RBX0"><Properties><int name="HeadColor">${c.head_color}</int><int name="LeftArmColor">${c.left_arm_color}</int><int name="LeftLegColor">${c.left_leg_color}</int><string name="Name">Body Colors</string><int name="RightArmColor">${c.right_arm_color}</int><int name="RightLegColor">${c.right_leg_color}</int><int name="TorsoColor">${c.torso_color}</int><bool name="archivable">true</bool></Properties></Item></roblox>`, 'text/xml; charset=utf-8'); }
      if (p === '/asset/' || p === '/asset' || p === '/asset/') { const id = q.get('id'); if (!/^\d{1,12}$/.test(id || '')) return reply(res, 400, 'bad id'); const a = db.prepare('SELECT * FROM assets WHERE id=?').get(+id);
        if (a && a.availability === 'AVAILABLE' && a.local_path) { const f = path.resolve(cfg.assetDir, a.local_path); if (f.startsWith(path.resolve(cfg.assetDir) + path.sep) && fs.existsSync(f)) return reply(res, 200, fs.readFileSync(f), 'application/octet-stream'); }
        db.prepare('INSERT INTO missing_asset_log(asset_id,requested_at,context) VALUES(?,?,?)').run(id, Date.now(), 'asset request'); return reply(res, 404, 'MISSING: this asset is not in the BLOXEN asset store'); }
      if (p === '/game/machineconfiguration.ashx') return reply(res, 200, 'true'); // INFERRED
      if (p === '/game/validate-machine') return json(res, { success: true, message: '' }); // INFERRED
      if (p === '/game/getallowedexperimentalfeatures') return json(res, []); // INFERRED: no experimental features
      if (p === '/universes/get-info') return json(res, { Id: 0, RootPlaceId: 0 }); // INFERRED
      if (p === '/game/gamepass/gamepasshandler.ashx') return reply(res, 200, 'False');
      if (p === '/game/luawebservice/handlesocialrequest.ashx') { const m = (q.get('method') || '').toLowerCase(); if (m === 'isfriendswith') { const a = +q.get('playerid'), b = +q.get('userid'); return reply(res, 200, `<Value Type="boolean">${a && b && S.areFriends(db, a, b)}</Value>`, 'text/xml'); } if (m === 'getgrouprank') return reply(res, 200, '<Value Type="integer">0</Value>', 'text/xml'); return reply(res, 200, '<Value Type="boolean">false</Value>', 'text/xml'); }
      if (p === '/analytics/measurement.ashx' || p === '/error/dmp.ashx' || p.startsWith('/uploadmedia/')) { await readBody(req, 64 * 1024).catch(() => {}); return reply(res, 204, ''); } // accepted locally, never forwarded
      if (p === '/health') return json(res, { ok: true, service: 'bloxen-compat', client: cfg.clientBuild });
      return reply(res, 404, 'not implemented by BLOXEN compat');
    } catch (e) { log('compat error ' + e.stack); if (!res.headersSent) reply(res, 500, 'error'); }
  };
  function peekToken(t) { if (typeof t !== 'string' || !/^[A-Za-z0-9_-]{20,64}$/.test(t)) return null; return db.prepare('SELECT * FROM join_tokens WHERE hash=? AND used_at IS NULL AND expires_at>?').get(auth.sha(t), Date.now()) || null; }
}
module.exports = { createCompatApp, numId };
