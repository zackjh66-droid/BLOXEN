#!/usr/bin/env node
'use strict';
// Render every required page of the running BLOXEN site to research/visual/*.png for the visual-comparison record.
// Needs puppeteer-core + @sparticuz/chromium (npm i in a scratch dir; path via SHOT_ENV). Run: AWS_EXECUTION_ENV=AWS_Lambda_nodejs22.x SHOT_ENV=/path node tools/screenshot_pages.js
const path = require('path'); const fs = require('fs');
const envDir = process.env.SHOT_ENV || path.join(__dirname, '..', '..', 'tools-env'); const req = m => require(require.resolve(m, { paths: [envDir] }));
const chromium = req('@sparticuz/chromium'); const puppeteer = req('puppeteer-core'); const { start } = require('../src/web/main'); const { Browser } = require('../tests/helpers');
const out = path.join(__dirname, '..', 'research', 'visual'); fs.mkdirSync(out, { recursive: true });
(async () => {
  const app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true }); const b = new Browser(app.webUrl);
  const pub = [['home-logged-out', '/'], ['games', '/games'], ['game-details', '/games/crossroads-2007-client'], ['catalog', '/catalog'], ['catalog-item', '/catalog/item/223785473'], ['login', '/login'], ['register', '/register'], ['develop', '/develop'], ['search', '/search?q=wings']];
  const priv = [['home-logged-in', '/home'], ['character', '/my/character'], ['inventory', '/my/inventory'], ['friends', '/my/friends'], ['groups', '/my/groups'], ['messages', '/my/messages'], ['account-settings', '/my/account'], ['profile', null]];
  const chrome = await puppeteer.launch({ args: [...chromium.default.args, '--no-sandbox'], executablePath: await chromium.default.executablePath(), headless: 'shell' });
  const shoot = async (name, url, cookies) => { const pg = await chrome.newPage(); await pg.setViewport({ width: 1100, height: 760 }); if (cookies) await pg.setCookie(...cookies); await pg.goto(url, { waitUntil: 'load' }); await pg.screenshot({ path: path.join(out, name + '.png'), fullPage: true }); await pg.close(); console.log('shot', name); };
  for (const [n, p] of pub) await shoot(n, app.webUrl + p);
  await b.register('PreserveMe'); await b.follow(await b.post('/my/groups/create', { name: 'Archivists', description: 'demo' }, { page: '/my/groups' }));
  for (const id of [151784320, 223751505]) await b.post(`/catalog/item/${id}/buy`, {}, { page: '/catalog/item/' + id }); await b.post('/my/character/equip', { asset: 151784320 }, { page: '/my/character' }); await b.post('/my/character/color', { part: 'torso_color', color: 23 }, { page: '/my/character' });
  const ck = [...b.jar].map(([name, value]) => ({ name, value, url: app.webUrl })); const uid = app.db.prepare("SELECT id FROM users WHERE username='PreserveMe'").get().id;
  for (const [n, p] of priv) await shoot(n, app.webUrl + (p || `/users/${uid}/profile`), ck);
  await chrome.close(); app.close();
})().catch(e => { console.error(e); process.exit(1); });
