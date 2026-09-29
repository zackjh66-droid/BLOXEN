'use strict';
// Domain logic shared by the website, the compat backend and the game-server hookup.
const cfg = require('./config'); const { TYPE_NAMES } = require('./db');
const BODY_PARTS = ['head_color', 'torso_color', 'left_arm_color', 'right_arm_color', 'left_leg_color', 'right_leg_color'];
const EQUIP_LIMIT = { 8: 3 }; // up to 3 hats; all other avatar slots hold one item
const AVATAR_TYPES = new Set([2, 8, 11, 12, 17, 18, 32]);
// BrickColor numbers offered for body colours (number = Roblox BrickColor id; names from the public BrickColor palette).
const BRICK_COLORS = { 1: ['White', '#F2F3F3'], 5: ['Brick yellow', '#D7C59A'], 9: ['Light reddish violet', '#E8BAC8'], 18: ['Nougat', '#CC8E69'], 21: ['Bright red', '#C4281C'], 23: ['Bright blue', '#0D69AC'], 24: ['Bright yellow', '#F5CD30'], 26: ['Black', '#1B2A35'], 28: ['Dark green', '#287F47'], 37: ['Bright green', '#4B974B'], 102: ['Medium blue', '#6E99CA'], 105: ['Br. yellowish orange', '#E29B40'], 106: ['Bright orange', '#DA8541'], 119: ['Br. yellowish green', '#A4BD47'], 194: ['Medium stone grey', '#A3A2A5'], 199: ['Dark stone grey', '#635F62'], 217: ['Brown', '#7C5C46'], 226: ['Cool yellow', '#FDEA8D'], 1001: ['Institutional white', '#F8F8F8'], 1003: ['Really black', '#111111'], 1004: ['Really red', '#FF0000'], 1010: ['Really blue', '#0000FF'] };

function createUser(db, { username, hash, birthday, gender }) {
  const now = Date.now(); const r = db.prepare('INSERT INTO users(username,pass_hash,created_at,birthday,gender,robux,last_online) VALUES(?,?,?,?,?,?,?)').run(username, hash, now, birthday || null, gender || null, cfg.startingRobux, now);
  const id = Number(r.lastInsertRowid); db.prepare('INSERT INTO avatars(user_id,updated_at) VALUES(?,?)').run(id, now); return id;
}
const userByName = (db, n) => db.prepare('SELECT * FROM users WHERE username=?').get(String(n));
const userById = (db, id) => db.prepare('SELECT * FROM users WHERE id=?').get(+id);

// ---- catalog
function catalogList(db, { q = '', type = 0, sort = 'relevance', limit = 60, offset = 0 } = {}) {
  const w = []; const a = []; if (q) { w.push('(name LIKE ? ESCAPE \'\\\' OR description LIKE ? ESCAPE \'\\\')'); const like = '%' + String(q).replace(/[\\%_]/g, m => '\\' + m) + '%'; a.push(like, like); } if (+type) { w.push('type_id=?'); a.push(+type); }
  const order = { relevance: 'sales_n DESC', price_asc: 'COALESCE(price_robux,price_tickets) ASC', price_desc: 'COALESCE(price_robux,price_tickets) DESC', updated: 'updated_ms DESC' }[sort] || 'sales_n DESC';
  return db.prepare(`SELECT *, CAST(REPLACE(sales, ',', '') AS INTEGER) AS sales_n FROM catalog_items ${w.length ? 'WHERE ' + w.join(' AND ') : ''} ORDER BY ${order} LIMIT ? OFFSET ?`).all(...a, limit, offset);
}
const catalogItem = (db, id) => db.prepare('SELECT * FROM catalog_items WHERE asset_id=?').get(+id);
function acquire(db, userId, assetId) {
  const item = catalogItem(db, assetId); if (!item) return { ok: false, error: 'No such catalog item' };
  if (db.prepare('SELECT 1 FROM inventory WHERE user_id=? AND asset_id=?').get(userId, item.asset_id)) return { ok: false, error: 'You already own this item' };
  if (item.min_membership > 0) return { ok: false, error: 'Builders Club membership is required for this item in the archive; BLOXEN has no membership system' };
  if (item.resale_only) return { ok: false, error: 'Limited item: in the archive it was only available from private sellers (resale is not implemented)' };
  if (item.limited_unique) return { ok: false, error: 'Sold out: this Limited U item was only available from private sellers in the archive (resale is not implemented)' };
  if (item.price_robux == null && item.price_tickets == null) return { ok: false, error: 'This item is off sale (no price in the archived catalog entry)' };
  db.exec('BEGIN IMMEDIATE');
  try {
    const u = userById(db, userId); const price = item.price_robux; const tix = item.price_tickets;
    if (price != null) { if (u.robux < price) { db.exec('ROLLBACK'); return { ok: false, error: 'Not enough R$' }; } db.prepare('UPDATE users SET robux=robux-? WHERE id=?').run(price, userId); }
    else if (tix != null) { if (u.tickets < tix) { db.exec('ROLLBACK'); return { ok: false, error: 'Not enough Tickets' }; } db.prepare('UPDATE users SET tickets=tickets-? WHERE id=?').run(tix, userId); }
    db.prepare('INSERT INTO inventory(user_id,asset_id,acquired_at) VALUES(?,?,?)').run(userId, item.asset_id, Date.now()); db.exec('COMMIT'); return { ok: true, item };
  } catch (e) { try { db.exec('ROLLBACK'); } catch {} throw e; }
}
const inventory = (db, userId, type = 0) => db.prepare(`SELECT c.*, i.acquired_at FROM inventory i JOIN catalog_items c ON c.asset_id=i.asset_id WHERE i.user_id=? ${+type ? 'AND c.type_id=?' : ''} ORDER BY i.acquired_at DESC`).all(...[userId, ...(+type ? [+type] : [])]);

// ---- avatar
function avatarGet(db, userId) { const a = db.prepare('SELECT * FROM avatars WHERE user_id=?').get(userId); const items = db.prepare('SELECT c.* FROM avatar_items ai JOIN catalog_items c ON c.asset_id=ai.asset_id WHERE ai.user_id=?').all(userId); return { colors: a, items }; }
function equip(db, userId, assetId) {
  const item = catalogItem(db, assetId); if (!item) return { ok: false, error: 'No such item' }; if (!AVATAR_TYPES.has(item.type_id)) return { ok: false, error: `${item.type} items cannot be worn on your character` };
  if (!db.prepare('SELECT 1 FROM inventory WHERE user_id=? AND asset_id=?').get(userId, item.asset_id)) return { ok: false, error: 'You do not own this item' };
  const limit = EQUIP_LIMIT[item.type_id] || 1; const cur = db.prepare('SELECT c.asset_id FROM avatar_items ai JOIN catalog_items c ON c.asset_id=ai.asset_id WHERE ai.user_id=? AND c.type_id=? ORDER BY ai.rowid').all(userId, item.type_id);
  if (cur.some(c => c.asset_id === item.asset_id)) return { ok: true, item, noop: true };
  db.exec('BEGIN IMMEDIATE'); try { while (cur.length >= limit) db.prepare('DELETE FROM avatar_items WHERE user_id=? AND asset_id=?').run(userId, cur.shift().asset_id); db.prepare('INSERT INTO avatar_items(user_id,asset_id) VALUES(?,?)').run(userId, item.asset_id); db.prepare('UPDATE avatars SET updated_at=? WHERE user_id=?').run(Date.now(), userId); db.exec('COMMIT'); } catch (e) { db.exec('ROLLBACK'); throw e; }
  return { ok: true, item };
}
function unequip(db, userId, assetId) { db.prepare('DELETE FROM avatar_items WHERE user_id=? AND asset_id=?').run(userId, +assetId); db.prepare('UPDATE avatars SET updated_at=? WHERE user_id=?').run(Date.now(), userId); return { ok: true }; }
function setColor(db, userId, part, color) { if (!BODY_PARTS.includes(part) || !BRICK_COLORS[color]) return { ok: false, error: 'Invalid body colour' }; db.prepare(`UPDATE avatars SET ${part}=?, updated_at=? WHERE user_id=?`).run(+color, Date.now(), userId); return { ok: true }; }
// Avatar description handed to the game server. Missing asset content is reported by the world, never substituted.
function avatarForGame(db, userId) {
  const { colors, items } = avatarGet(db, userId); const by = t => items.filter(i => i.type_id === t).map(i => i.asset_id);
  return { bodyColors: { HeadColor: colors.head_color, TorsoColor: colors.torso_color, LeftArmColor: colors.left_arm_color, RightArmColor: colors.right_arm_color, LeftLegColor: colors.left_leg_color, RightLegColor: colors.right_leg_color },
    shirtId: by(11)[0] || 0, pantsId: by(12)[0] || 0, tshirtId: by(2)[0] || 0, faceId: by(18)[0] || 0, headId: by(17)[0] || 0, hatIds: by(8), packageIds: by(32) };
}

// ---- friends
function sendFriendRequest(db, from, toName) { const to = userByName(db, toName); if (!to) return { ok: false, error: 'No such user' }; if (to.id === from) return { ok: false, error: 'You cannot friend yourself' };
  const [a, b] = from < to.id ? [from, to.id] : [to.id, from]; if (db.prepare('SELECT 1 FROM friends WHERE a=? AND b=?').get(a, b)) return { ok: false, error: 'Already friends' };
  if (db.prepare('SELECT 1 FROM friend_requests WHERE from_id=? AND to_id=?').get(to.id, from)) return respondFriend(db, from, to.id, true);
  db.prepare('INSERT OR IGNORE INTO friend_requests(from_id,to_id,created_at) VALUES(?,?,?)').run(from, to.id, Date.now()); return { ok: true, pending: true }; }
function respondFriend(db, me, fromId, accept) { const r = db.prepare('DELETE FROM friend_requests WHERE from_id=? AND to_id=?').run(+fromId, me); if (!r.changes) return { ok: false, error: 'No such request' }; if (accept) { const [a, b] = me < +fromId ? [me, +fromId] : [+fromId, me]; db.prepare('INSERT OR IGNORE INTO friends(a,b,since) VALUES(?,?,?)').run(a, b, Date.now()); } return { ok: true }; }
const friendsOf = (db, id) => db.prepare('SELECT u.id,u.username,u.last_online FROM friends f JOIN users u ON u.id = CASE WHEN f.a=? THEN f.b ELSE f.a END WHERE f.a=? OR f.b=? ORDER BY u.username').all(id, id, id);
const requestsFor = (db, id) => db.prepare('SELECT u.id,u.username,r.created_at FROM friend_requests r JOIN users u ON u.id=r.from_id WHERE r.to_id=? ORDER BY r.created_at').all(id);
const areFriends = (db, x, y) => { const [a, b] = x < y ? [x, y] : [y, x]; return !!db.prepare('SELECT 1 FROM friends WHERE a=? AND b=?').get(a, b); };

// ---- messages
function sendMessage(db, from, toName, subject, body) { const to = userByName(db, toName); if (!to) return { ok: false, error: 'No such user' }; subject = String(subject || '').trim().slice(0, 100); body = String(body || '').trim().slice(0, 5000); if (!subject || !body) return { ok: false, error: 'Subject and message are required' };
  db.prepare('INSERT INTO messages(from_id,to_id,subject,body,created_at) VALUES(?,?,?,?,?)').run(from, to.id, subject, body, Date.now()); return { ok: true }; }

// ---- groups
function createGroup(db, owner, name, description) { name = String(name || '').trim(); if (!/^[A-Za-z0-9 ._-]{3,40}$/.test(name)) return { ok: false, error: 'Group names are 3-40 letters, numbers, spaces, . _ -' };
  try { db.exec('BEGIN'); const r = db.prepare('INSERT INTO groups(name,description,owner_id,created_at) VALUES(?,?,?,?)').run(name, String(description || '').slice(0, 1000), owner, Date.now()); db.prepare('INSERT INTO group_members(group_id,user_id,role) VALUES(?,?,?)').run(Number(r.lastInsertRowid), owner, 'Owner'); db.exec('COMMIT'); return { ok: true, id: Number(r.lastInsertRowid) }; } catch (e) { try { db.exec('ROLLBACK'); } catch {} return { ok: false, error: /UNIQUE/.test(e.message) ? 'That group name is taken' : 'Could not create group' }; } }
const joinGroup = (db, user, gid) => { if (!db.prepare('SELECT 1 FROM groups WHERE id=?').get(+gid)) return { ok: false, error: 'No such group' }; db.prepare("INSERT OR IGNORE INTO group_members(group_id,user_id,role) VALUES(?,?,'Member')").run(+gid, user); return { ok: true }; };

// ---- places
const placeList = db => db.prepare("SELECT * FROM places WHERE status LIKE 'ACCEPTED%' ORDER BY sort, title").all();
const placeById = (db, id) => db.prepare('SELECT * FROM places WHERE id=?').get(String(id));
const favorite = (db, u, p, on) => on ? db.prepare('INSERT OR IGNORE INTO favorites(user_id,place_id) VALUES(?,?)').run(u, p) : db.prepare('DELETE FROM favorites WHERE user_id=? AND place_id=?').run(u, p);

// ---- search
function search(db, q) { q = String(q || '').slice(0, 60); const like = '%' + q.replace(/[\\%_]/g, m => '\\' + m) + '%';
  return { users: db.prepare("SELECT id,username FROM users WHERE username LIKE ? ESCAPE '\\' ORDER BY username LIMIT 25").all(like), games: db.prepare("SELECT id,title,creator FROM places WHERE status LIKE 'ACCEPTED%' AND title LIKE ? ESCAPE '\\' LIMIT 25").all(like), items: db.prepare("SELECT asset_id,name,type FROM catalog_items WHERE name LIKE ? ESCAPE '\\' LIMIT 25").all(like), groups: db.prepare("SELECT id,name FROM groups WHERE name LIKE ? ESCAPE '\\' LIMIT 25").all(like) }; }
module.exports = { BODY_PARTS, BRICK_COLORS, TYPE_NAMES, createUser, userByName, userById, catalogList, catalogItem, acquire, inventory, avatarGet, equip, unequip, setColor, avatarForGame, sendFriendRequest, respondFriend, friendsOf, requestsFor, areFriends, sendMessage, createGroup, joinGroup, placeList, placeById, favorite, search };
