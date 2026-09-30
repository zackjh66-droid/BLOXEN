'use strict';
// Reusable page components. Structure/class names follow the ARCHIVED-NEAR-DATE 2015-02-21 item-page DOM+CSS (header.rbx-header, #navContent / .rbx-left-col, #BodyWrapper,
// #Footer/.FooterNav/.legal, .btn-large/.btn-medium, .SquareTabGray, .StandardPanelWhite). Nothing here is a Roblox asset; all imagery is MISSING and labelled.
// Every dynamic value passes through esc(). No inline style= (CSP).
const { esc } = require('../lib/http');
const money = n => n == null ? '' : Number(n).toLocaleString('en-US');
const when = ms => ms ? new Date(ms).toISOString().slice(0, 10) : 'UNKNOWN';
const csrf = c => `<input type="hidden" name="_csrf" value="${esc(c.csrf)}">`;
const postForm = (c, action, inner, cls = '') => `<form method="post" action="${esc(action)}" class="inl ${cls}">${csrf(c)}${inner}</form>`;

// ---- status/grade badges
const gradeBadge = g => `<span class="badge ${/EXACT|PRESERVED/.test(g) ? 'g' : /NEAR|RECON/.test(g) ? 'b' : /INFERRED|QUARANTINED/.test(g) ? 'y' : 'r'}" title="BLOXEN provenance grade">${esc(g)}</span>`;
const statusBadge = s => `<span class="badge ${{ PRESERVED: 'g', QUARANTINED: 'y', REJECTED: 'r', UNKNOWN: 'r' }[s] || 'r'}" title="BLOXEN game registry status">${esc(s)}</span>`;

// ---- images: never substituted; a labelled flat block
const thumbMissing = (alt, cls = '') => `<div class="thumb ${cls}" title="Archived thumbnail not retrieved">${esc(alt || '')}<br>(thumbnail MISSING)</div>`;

// ---- buttons (.btn-large h39 / .btn-medium h28 / .btn-small; kinds primary #007001, neutral #0852b7, control, negative)
const btn = (label, { href, size = 'medium', kind = 'primary', disabled = false, title = '' } = {}) => disabled || !href
  ? `<span class="btn-${size} btn-${kind} btn-disabled"${title ? ` title="${esc(title)}"` : ''}>${esc(label)}</span>` : `<a class="btn-${size} btn-${kind}" href="${esc(href)}">${esc(label)}</a>`;
const submit = (label, { size = 'medium', kind = 'primary', cls = '' } = {}) => `<button class="btn-${size} btn-${kind} ${cls}" type="submit">${esc(label)}</button>`;

// ---- tabs (.SquareTabGray; selected white)
const tabs = (items, cls = 'SquareTabGray') => `<div class="tabs ${cls}">${items.map(t => t.disabled ? `<span class="off" title="${esc(t.title || '')}">${esc(t.label)}</span>` : `<a class="${t.selected ? 'sel' : ''}" href="${esc(t.href)}">${esc(t.label)}</a>`).join('')}</div>`;

// ---- panel (.StandardPanelWhite)
const panel = (title, inner, cls = '') => `<div class="StandardPanelWhite ${cls}">${title ? `<div class="pn-h">${esc(title)}</div>` : ''}<div class="pn-b">${inner}</div></div>`;

// ---- pager: "Page N of M", Previous/Next (profile capture 20150903 shows "Page 1 of 2"). hrefFn(page) -> url
function pager({ page, pages, hrefFn }) {
  if (pages <= 1) return '';
  return `<div class="pager">${page > 1 ? `<a href="${esc(hrefFn(page - 1))}">&laquo; Previous</a>` : '<span class="off">&laquo; Previous</span>'}<span class="pg">Page ${page} of ${pages}</span>${page < pages ? `<a href="${esc(hrefFn(page + 1))}">Next &raquo;</a>` : '<span class="off">Next &raquo;</span>'}</div>`;
}
const paginate = (total, page, per) => { const pages = Math.max(1, Math.ceil(total / per)); const p = Math.min(Math.max(1, +page || 1), pages); return { page: p, pages, offset: (p - 1) * per, limit: per, total }; };

// ---- tiles
const gameTile = (g, { fav, status } = {}) => `<div class="gtile"><a href="/games/${esc(g.id)}">${thumbMissing(g.title)}<div class="nm" title="${esc(g.title)}">${esc(g.title)}</div></a><div class="tip">by ${esc(g.creator || 'UNKNOWN')}</div>${status ? `<div>${statusBadge(status)}</div>` : ''}${fav != null ? `<div class="tip">&#9733; ${fav}</div>` : ''}</div>`;
const itemTile = (i, { owned } = {}) => `<div class="tile"><a href="/catalog/item/${i.asset_id}">${thumbMissing(i.name)}<div class="nm" title="${esc(i.name)}">${esc(i.name)}</div></a><div class="tip">by ${esc(i.creator)}</div><div class="price${i.price_robux == null ? ' t' : ''}">${owned ? 'Item Owned' : i.price_robux != null ? 'R$ ' + money(i.price_robux) : i.price_tickets != null ? 'Tix ' + money(i.price_tickets) : 'Off sale'}</div></div>`;
const userTile = u => `<a class="ftile" href="/users/${u.id}/profile"><div class="fthumb">(thumbnail MISSING)</div><div class="fnm">${esc(u.username)}</div></a>`;

// ---- site chrome
const NAV = [['Games', '/games'], ['Catalog', '/catalog'], ['Develop', '/develop']]; // no ROBUX tab: BLOXEN has no purchasable currency
function header(c) {
  const u = c.user;
  const acct = u ? `<a href="/users/${u.id}/profile">${esc(u.username)}</a><span title="BLOXEN local play allowance (not a historical amount)">R$ ${money(u.robux)}</span><a href="/my/messages">Messages</a>${postForm(c, '/logout', '<button class="linkbtn">Logout</button>')}` : '<a href="/register">Sign Up</a><a href="/login">Login</a>';
  return `<div id="header" class="rbx-header header-2014"><div class="inner container-header"><a class="logo rbx-logo" href="${u ? '/home' : '/'}">BLOXEN</a><ul class="rbx-navbar links">${NAV.map(([n, h]) => `<li><a href="${h}">${n}</a></li>`).join('')}</ul><form class="search rbx-search" action="/search" method="get"><input type="text" name="q" placeholder="Search" maxlength="60" value=""></form><span class="acct">${acct}</span></div></div>`;
}
// Left nav: shown only >= 1480px (CSS). ONLY pages that exist in BLOXEN are listed.
function leftNav(c) {
  const a = (t, h) => `<li><a href="${h}">${t}</a></li>`;
  return `<div id="navContent" class="rbx-left-col"><ul>${c.user ? a('Home', '/home') + a('Profile', `/users/${c.user.id}/profile`) + a('Messages', '/my/messages') + a('Friends', '/my/friends') + a('Character', '/my/character') + a('Inventory', '/my/inventory') + a('Favorites', '/my/favorites') + a('Travel', '/my/travel') + a('Groups', '/my/groups') : ''}${a('Games', '/games')}${a('Catalog', '/catalog')}${a('People', '/users')}${a('Develop', '/develop')}${c.user ? a('Account Settings', '/my/account') : ''}</ul></div>`;
}
function footer(cfg) {
  return `<div id="Footer" class="footer-2015"><div class="FooterNav"><a href="/about">About BLOXEN</a> | <a href="/games">Games</a> | <a href="/catalog">Catalog</a> | <a href="/develop">Develop</a> | <a href="/games/registry">Game Registry</a> | <a href="/users">People</a></div><div class="legal"><p class="legalese">BLOXEN is an unofficial, non-commercial preservation project reconstructing the July 2015 web experience from archived evidence. It is not affiliated with or endorsed by Roblox Corporation. ROBLOX, the names of historical games and items, and their marks belong to their owners. Target client: Windows Player ${esc(cfg.clientBuild)} (${esc(cfg.clientVersion)}).</p></div></div>`;
}
module.exports = { esc, money, when, csrf, postForm, gradeBadge, statusBadge, thumbMissing, btn, submit, tabs, panel, pager, paginate, gameTile, itemTile, userTile, header, leftNav, footer };
