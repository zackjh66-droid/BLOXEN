'use strict';
const crypto = require('crypto');
const esc = s => String(s ?? '').replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
function parseCookies(h) { const o = {}; for (const p of String(h || '').split(';')) { const i = p.indexOf('='); if (i > 0) o[p.slice(0, i).trim()] = p.slice(i + 1).trim(); } return o; }
function readBody(req, limit = 64 * 1024) { return new Promise((res, rej) => { const ch = []; let n = 0; req.on('data', d => { n += d.length; if (n > limit) { rej(Object.assign(new Error('body too large'), { status: 413 })); req.destroy(); } else ch.push(d); }); req.on('end', () => res(Buffer.concat(ch))); req.on('error', rej); }); }
async function readForm(req, limit) { const ct = String(req.headers['content-type'] || ''); if (!ct.startsWith('application/x-www-form-urlencoded')) { if (req.method === 'POST') throw Object.assign(new Error('unsupported content type'), { status: 415 }); return {}; } const b = (await readBody(req, limit)).toString('utf8'); const o = Object.create(null); for (const [k, v] of new URLSearchParams(b)) if (!(k in o)) o[k] = v; return o; }
const SEC_HEADERS = { 'X-Content-Type-Options': 'nosniff', 'X-Frame-Options': 'DENY', 'Referrer-Policy': 'same-origin', 'Cross-Origin-Opener-Policy': 'same-origin',
  'Content-Security-Policy': "default-src 'self'; img-src 'self' data:; style-src 'self'; script-src 'self'; frame-ancestors 'none'; form-action 'self'; base-uri 'none'; object-src 'none'" };
function send(res, status, body, headers = {}) { res.writeHead(status, { ...SEC_HEADERS, 'Content-Type': 'text/html; charset=utf-8', ...headers }); res.end(body); }
const redirect = (res, to, headers = {}) => { res.writeHead(303, { ...SEC_HEADERS, Location: to, ...headers }); res.end(); };
function cookie(name, value, { maxAge, path = '/', httpOnly = true, secure = false } = {}) { return `${name}=${value}; Path=${path}; SameSite=Lax${httpOnly ? '; HttpOnly' : ''}${secure ? '; Secure' : ''}${maxAge != null ? '; Max-Age=' + maxAge : ''}`; }
const safeEqual = (a, b) => { a = Buffer.from(String(a)); b = Buffer.from(String(b)); return a.length === b.length && crypto.timingSafeEqual(a, b); };
// Only same-site relative redirects are allowed (open-redirect protection).
const safeNext = n => (typeof n === 'string' && /^\/(?![\/\\])[A-Za-z0-9\-._~\/?=&%]*$/.test(n) ? n : '/home');
module.exports = { esc, parseCookies, readBody, readForm, send, redirect, cookie, safeEqual, safeNext, SEC_HEADERS };
