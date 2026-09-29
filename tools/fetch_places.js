#!/usr/bin/env node
'use strict';
// Fetch accepted place files from the pinned source commit into the git-ignored store, verifying SHA-256 against preservation/manifests/places.json.
// Files that fail verification are deleted and reported (QUARANTINE FAILURE). Only tiers/status allowed by the manifest are fetched. Nothing is executed.
// Usage: node tools/fetch_places.js [--all] [--local <clone-root>] [id ...]
const fs = require('fs'); const path = require('path'); const crypto = require('crypto'); const https = require('https');
const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'preservation', 'manifests', 'places.json'), 'utf8'));
const store = path.join(__dirname, '..', 'preservation', 'store'); fs.mkdirSync(store, { recursive: true });
const args = process.argv.slice(2); const all = args.includes('--all'); const li = args.indexOf('--local'); const localRoot = li >= 0 ? args[li + 1] : null; // --local <clone-root>: take files from an existing clone of the pinned repo instead of the network (SHA-256 still verified)
const ids = args.filter((a, i) => !a.startsWith('--') && !(li >= 0 && i === li + 1));
function get(url, hops = 5, headers = {}) { return new Promise((res, rej) => { https.get(url, { headers: { 'User-Agent': 'bloxen-fetch-places', ...headers } }, r => { if ([301, 302, 307].includes(r.statusCode) && hops) { r.resume(); return res(get(new URL(r.headers.location, url).href, hops - 1, headers)); } if (r.statusCode !== 200) { r.resume(); return rej(new Error('HTTP ' + r.statusCode + ' ' + url)); } const ch = []; r.on('data', d => ch.push(d)); r.on('end', () => res(Buffer.concat(ch))); }).on('error', rej); }); }
(async () => {
  let bad = 0;
  for (const it of manifest.items) {
    if (!/^ACCEPTED/.test(it.status)) continue; if (!all && !ids.includes(it.id)) continue;
    const dest = path.join(store, it.sha256 + '.rbx'); if (fs.existsSync(dest)) { console.log('have', it.id); continue; }
    const url = `${it.source.repo}/raw/${it.source.commit}/${it.source.path.split('/').map(encodeURIComponent).join('/')}`;
    const [, owner, repo] = new URL(it.source.repo).pathname.split('/'); const api = `https://api.github.com/repos/${owner}/${repo}/contents/${it.source.path.split('/').map(encodeURIComponent).join('/')}?ref=${it.source.commit}`;
    try { // primary: raw download; fallback (hosts that cannot reach raw.githubusercontent.com): GitHub contents API, raw media type. Either way the SHA-256 must match the manifest.
      let buf; if (localRoot) buf = fs.readFileSync(path.join(localRoot, it.source.path)); else try { buf = await get(url); } catch (e1) { buf = await get(api, 5, { Accept: 'application/vnd.github.raw' }); } const sha = crypto.createHash('sha256').update(buf).digest('hex');
      if (sha !== it.sha256) { console.error(`QUARANTINE FAILURE ${it.id}: sha256 ${sha} != manifest ${it.sha256}`); bad++; continue; }
      fs.writeFileSync(dest, buf, { mode: 0o444 }); console.log('stored', it.id, it.sha256.slice(0, 12), buf.length + ' B'); }
    catch (e) { console.error('FAILED', it.id, e.message); bad++; } }
  process.exit(bad ? 1 : 0);
})();
