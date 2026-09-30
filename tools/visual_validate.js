#!/usr/bin/env node
'use strict';
// Visual validation, reference -> render -> measure -> compare -> correct. NOT a pixel-perfect claim: no pixel-exact July-2015 screenshot exists in evidence (Wayback gave
// rendered text, not images), so the "reference" is the set of CSS VALUES read from real 2015 Roblox CSS (ARCHIVED-NEAR-DATE, 2015-02-21 item-page bundles) plus
// the archived DOM structure. This tool renders the real pages in headless Chromium, reads COMPUTED styles and geometry, compares them with those values and writes
// research/website/visual-validation.md + screenshots. Exit code 1 if any check fails.
// Run: AWS_EXECUTION_ENV=AWS_Lambda_nodejs22.x SHOT_ENV=/home/user/tools-env node --disable-warning=ExperimentalWarning tools/visual_validate.js
const path = require('path'); const fs = require('fs');
const envDir = process.env.SHOT_ENV || path.join(__dirname, '..', '..', 'tools-env'); const req = m => require(require.resolve(m, { paths: [envDir] }));
const chromium = req('@sparticuz/chromium'); const puppeteer = req('puppeteer-core'); const { start } = require('../src/web/main'); const { Browser } = require('../tests/helpers');
const outDir = path.join(__dirname, '..', 'research', 'website'); const shots = path.join(outDir, 'screens'); fs.mkdirSync(shots, { recursive: true });
const E = 'ARCHIVED-NEAR-DATE (2015-02-21 CSS)', S2 = 'SECONDARY (third-party restyle values)';
// [page key, selector, property, expected, evidence tier, note]
const CHECKS = [
  ['home', '#header', 'position', 'fixed', E], ['home', '#header', 'height', '40px', E], ['home', '#header', 'backgroundColor', 'rgb(0, 116, 189)', E + ' #0074bd'],
  ['home', '.rbx-navbar a', 'fontSize', '16px', E], ['home', '.rbx-navbar a', 'paddingTop', '6px', E], ['home', '.rbx-navbar a', 'paddingLeft', '9px', E], ['home', '.rbx-navbar a', 'color', 'rgb(255, 255, 255)', E], ['home', '.rbx-navbar a', 'borderTopLeftRadius', '5px', E],
  ['home', '.rbx-search input', 'height', '28px', E], ['home', '.rbx-search input', 'borderTopColor', 'rgb(184, 184, 184)', E + ' #b8b8b8'], ['home', '.rbx-search input', 'borderTopLeftRadius', '3px', E],
  ['home', '.nav-content', 'marginTop', '40px', E], ['home', '#BodyWrapper', 'width', '970px', E], ['home', '#BodyWrapper', 'backgroundColor', 'rgb(255, 255, 255)', E],
  ['home', 'body', 'fontSize', '14px', E], ['home', 'body', 'color', 'rgb(52, 52, 52)', E + ' #343434'], ['home', 'body', 'lineHeight', '19.992px', E + ' 1.428'], ['home', 'body', 'fontFamily', /^"?Source Sans Pro"?, Arial, Helvetica, sans-serif$/, E], ['home', 'body', 'backgroundColor', 'rgb(227, 227, 227)', S2 + ' #e3e3e3'],
  ['catalog', '#BodyWrapper a.b, #BodyWrapper .tile a', 'color', 'rgb(0, 85, 179)', E + ' #0055B3'],
  ['home', '#Footer', 'fontSize', '13px', E], ['home', '#Footer', 'paddingTop', '26px', E + ' (2em)'], ['home', '#Footer', 'paddingBottom', '60px', E], ['home', '#Footer .legal', 'borderTopColor', 'rgb(53, 92, 149)', E + ' #355C95'], ['home', '#Footer .legal', 'borderTopWidth', '1px', E], ['home', '#Footer .legalese', 'color', 'rgb(137, 159, 193)', E + ' #899fc1'], ['home', '#Footer .legalese', 'fontSize', '10px', E], ['home', '#Footer .FooterNav a', 'color', 'rgb(45, 82, 143)', E + ' #2D528F'], ['home', '#Footer .FooterNav', 'width', '970px', E],
  ['gamedetails', '.btn-large', 'height', '39px', E], ['gamedetails', '.btn-large', 'paddingTop', '9px', E], ['gamedetails', '.btn-large', 'fontSize', '23px', E], ['gamedetails', '.btn-large', 'borderTopColor', 'rgb(0, 112, 1)', E + ' #007001'],
  ['item', '.btn-medium', 'height', '28px', E], ['item', '.btn-medium', 'fontSize', '20px', E],
  ['gamedetails', '.SquareTabGray a:not(.sel)', 'backgroundColor', 'rgb(214, 214, 214)', E + ' #D6D6D6'], ['gamedetails', '.SquareTabGray a:not(.sel)', 'borderTopColor', 'rgb(158, 158, 158)', E + ' #9e9e9e'], ['gamedetails', '.SquareTabGray a:not(.sel)', 'fontWeight', '700', E], ['gamedetails', '.SquareTabGray a:not(.sel)', 'fontSize', '15px', E], ['gamedetails', '.SquareTabGray a.sel', 'backgroundColor', 'rgb(255, 255, 255)', E],
  ['item', '#Item', 'width', '800px', E], ['item', '#Thumbnail', 'width', '320px', E], ['item', '#Thumbnail .thumb', 'height', '320px', E], ['item', '#Summary', 'width', '480px', E], ['item', '.BuyPriceBox', 'width', '158px', E + ' (CSS width)'], ['item', '.BuyPriceBox', 'backgroundColor', 'rgb(225, 225, 225)', E + ' #e1e1e1'], ['item', '.BuyPriceBox', 'borderTopColor', 'rgb(167, 167, 167)', E + ' #a7a7a7'],
  ['item', '.robux-text', 'color', 'rgb(0, 102, 0)', E + ' #060'], ['item', '.stat-label', 'fontSize', '12px', E], ['item', '.stat-label', 'color', 'rgb(153, 153, 153)', E + ' #999'],
  ['home', '.rbx-left-col', 'display', 'none', E + ' (shown only >=1480px) @1100'], ['home1600', '.rbx-left-col', 'display', 'block', E + ' @1600'], ['home1600', '.rbx-left-col', 'width', '175px', E], ['home1600', '.rbx-left-col', 'backgroundColor', 'rgb(242, 242, 242)', E + ' #f2f2f2'], ['home1600', '.rbx-left-col', 'top', '40px', E], ['home1600', '.rbx-left-col li a', 'color', 'rgb(25, 25, 25)', E + ' #191919'],
];
// geometric invariants (layout sanity), independent of the evidence values
const GEOM = [
  ['no horizontal overflow at 1100px', p => p.doc.scrollWidth <= 1100], ['header spans full width and is 40px tall', p => p.header.w >= 1100 && p.header.h === 40],
  ['body wrapper centred', p => Math.abs(p.wrap.l - (p.vw - p.wrap.w) / 2) <= 1], ['content starts below the fixed header', p => p.wrap.t >= 40], ['footer is below the content', p => p.footer.t >= p.wrap.b - 1],
];
(async () => {
  const app = await start({ dbPath: ':memory:', webPort: 0, compatPort: 0, quiet: true }); const b = new Browser(app.webUrl);
  await b.register('PreserveMe'); const uid = app.db.prepare("SELECT id FROM users WHERE username='PreserveMe'").get().id;
  for (const id of [151784320, 223751505]) await b.post(`/catalog/item/${id}/buy`, {}, { page: '/catalog/item/' + id }); await b.post('/my/character/equip', { asset: 151784320 }, { page: '/my/character' });
  await b.post('/games/tabularasa/favorite', { on: '1' }, { page: '/games/tabularasa' }); await b.post('/catalog/item/151784320/favorite', { on: '1' }, { page: '/catalog/item/151784320' });
  app.db.prepare('INSERT INTO teleports(user_id,from_place,to_place,roblox_place_id,created_at) VALUES(?,?,?,?,?)').run(uid, 'crossroads-2007-client', 'roblox-world-headquarters', 1501, Date.now());
  const ck = [...b.jar].map(([name, value]) => ({ name, value, url: app.webUrl })); const chrome = await puppeteer.launch({ args: [...chromium.default.args, '--no-sandbox'], executablePath: await chromium.default.executablePath(), headless: 'shell' });
  const PAGES = { home: ['/home', 1100], home1600: ['/home', 1600], gamedetails: ['/games/crossroads-2007-client', 1100], item: ['/catalog/item/223785473', 1100], catalog: ['/catalog', 1100] };
  const EXTRA = [['games', '/games'], ['game-registry', '/games/registry'], ['game-compat', '/games/crossroads-uncopylocked-commit?tab=compat'], ['catalog-page2', '/catalog?page=2'], ['inventory', '/my/inventory'], ['favorites', '/my/favorites'], ['travel', '/my/travel'], ['people', '/users?q=Pre'], ['character', '/my/character'], ['friends', '/my/friends'], ['groups', '/my/groups'], ['messages', '/my/messages'], ['account-settings', '/my/account'], ['develop', '/develop']];
  const page = async (url, w, name) => { const pg = await chrome.newPage(); await pg.setViewport({ width: w, height: 800 }); await pg.setCookie(...ck); await pg.goto(app.webUrl + url, { waitUntil: 'load' }); if (name) await pg.screenshot({ path: path.join(shots, name + '.png') }); return pg; };
  const rows = []; let fails = 0; const geom = [];
  for (const [key, [url, w]] of Object.entries(PAGES)) {
    const pg = await page(url, w, key === 'home' ? 'home-logged-in' : key === 'home1600' ? 'home-left-nav-1600' : key === 'gamedetails' ? 'game-details' : key === 'item' ? 'catalog-item' : 'catalog');
    for (const [k, sel, prop, exp, tier] of CHECKS.filter(c => c[0] === key)) {
      const got = await pg.evaluate((sel, prop) => { const el = document.querySelector(sel); if (!el) return '<no element>'; const cs = getComputedStyle(el); return cs[prop]; }, sel, prop);
      const ok = exp instanceof RegExp ? exp.test(got) : got === exp; if (!ok) fails++; rows.push({ page: url + (w === 1600 ? ' @1600' : ''), sel, prop, exp: String(exp), got, ok, tier });
    }
    if (key === 'home') { const m = await pg.evaluate(() => { const r = s => { const b = document.querySelector(s).getBoundingClientRect(); return { l: b.left, t: b.top + scrollY, w: b.width, h: b.height, b: b.bottom + scrollY }; }; return { vw: innerWidth, doc: { scrollWidth: document.documentElement.scrollWidth }, header: r('#header'), wrap: r('#BodyWrapper'), footer: r('#Footer') }; }); for (const [n, f] of GEOM) { const ok = f(m); if (!ok) fails++; geom.push({ n, ok }); } }
    await pg.close();
  }
  { const pg = await page('/home', 1600); const r = await pg.evaluate(() => { const a = document.querySelector('.rbx-left-col').getBoundingClientRect(), w = document.querySelector('#BodyWrapper').getBoundingClientRect(); return a.right <= w.left; }); if (!r) fails++; geom.push({ n: 'left nav does not overlap the page body at 1600px', ok: r }); await pg.close(); }
  for (const [n, u] of EXTRA) { const pg = await page(u, 1100, n); const ov = await pg.evaluate(() => document.documentElement.scrollWidth <= innerWidth); const bad = await pg.evaluate(() => [...document.querySelectorAll('#BodyWrapper *')].filter(e => e.getBoundingClientRect().right > document.querySelector('#BodyWrapper').getBoundingClientRect().right + 1).length); if (!ov || bad) fails++; geom.push({ n: `${u}: no page overflow, nothing escapes #BodyWrapper`, ok: ov && !bad, detail: bad ? bad + ' element(s) overflow' : '' }); await pg.close(); }
  for (const [n, u] of [['home-logged-out', '/'], ['login', '/login'], ['register', '/register']]) { const pg = await chrome.newPage(); await pg.setViewport({ width: 1100, height: 800 }); await pg.goto(app.webUrl + u, { waitUntil: 'load' }); await pg.screenshot({ path: path.join(shots, n + '.png') }); await pg.close(); }
  await chrome.close(); app.close();
  const md = ['# Visual validation (computed-style comparison)', '', `Generated ${new Date().toISOString().slice(0, 10)} by \`tools/visual_validate.js\`. Result: **${fails ? fails + ' FAIL' : 'all ' + (rows.length + geom.length) + ' checks pass'}** (${rows.length} style checks, ${geom.length} geometry checks).`, '',
    '## What this is, and what it is not', '', '- **Method:** reference values -> render the real BLOXEN pages in headless Chromium -> read `getComputedStyle`/bounding boxes -> compare -> correct. Corrections already made on the way: the pre-evidence header colour (#0e6ba7, sampled from a blurry thumbnail) was replaced by #0074bd from the real 2015 CSS, the 2014-era values (#2D528F header etc.) were dropped, and the left nav / footer / tabs / item-page sizes were added from the recorded CSS. The tool itself then caught two defects that were fixed: fixed-width form inputs escaping the side column on /my/groups, and the R$ balance rendering dark-on-blue in the header. Any FAIL row below means the CSS currently disagrees with the recorded value.',
    '- **Reference values** were read from real 2015 Roblox CSS/DOM: the 2015-02-21 Roblox item page (real `FetchCSS` bundles: `rbx-header`, `rbx-left-col`, `#BodyWrapper`, `#Footer`, buttons, `SquareTabGray`, `#Item`), ARCHIVED-NEAR-DATE (five months before the July 2015 target). Grey page colour `#e3e3e3` is SECONDARY. See `research/website/SOURCES.md`.',
    '- **It is NOT a pixel-perfect claim.** No pixel-exact July-2015 screenshot is available (Wayback captures were only obtainable as rendered text), so no image diff was possible. Fonts: Source Sans Pro is named first but is not bundled, so the machine falls back to Arial. Logo, sprites, icons, thumbnails and photographs are MISSING and shown as labelled blocks. Copy on the logged-out home is ARCHIVED-EXACT (Wayback 20150723171630); page composition beyond the recorded values is RECONSTRUCTED-FROM-EVIDENCE or INFERRED.',
    '- Screenshots: `research/website/screens/*.png` (1100px wide; `home-left-nav-1600.png` at 1600px to show the >=1480px left nav). `research/visual/` holds the older pre-refit set and is superseded.', '',
    '## Style checks (computed vs recorded 2015 values)', '', '| Page | Selector | Property | Expected | Computed | Evidence tier | |', '|---|---|---|---|---|---|---|', ...rows.map(r => `| ${r.page} | \`${r.sel}\` | ${r.prop} | \`${r.exp}\` | \`${r.got}\` | ${r.tier} | ${r.ok ? 'PASS' : '**FAIL**'} |`), '',
    '## Geometry checks', '', ...geom.map(g => `- ${g.ok ? 'PASS' : '**FAIL**'}: ${g.n}${g.detail ? ' (' + g.detail + ')' : ''}`), '',
    '## Per-page notes (what matches the archive, what does not)', '',
    '| Page | Archive structure followed | Known differences |', '|---|---|---|',
    '| Home (logged in) | fixed 40px blue header, Games/Catalog/Develop, search, left nav >=1480px, 970px white body, footer with legal line | no logo image (MISSING); no ROBUX tab (BLOXEN has no currency); Friends/Recently Played/Favorites layout is RECONSTRUCTED from text captures |',
    '| Games | sort tabs in `SquareTabGray`; tile grid | only sorts backed by local data are offered; archive had genre/time filters and play counts BLOXEN cannot know |',
    '| Game Details (20150627 capture) | h1, "By creator", large green Play, favourite star + count, tabs, stat list, Running Games | tabs are About/Compatibility/Evidence (archive: About/Store/Leaderboards/Game Instances); no carousel/badges/passes/leaderboards (no data); BLOXEN adds registry status, provenance, compatibility |',
    '| Catalog Item (2015-02-21 DOM) | `#Item` 800px, `#Thumbnail` 320px, `#Summary` 480px, `.BuyPriceBox`, favourite star, Item Owned | no Recommendations/Commentary tabs, no voting, no Builders Club price variants; thumbnails MISSING |',
    '| Profile (20150903 capture) | "<name>\'s Profile", [Offline], avatar, friends, Favorites with category dropdown | avatar is a labelled colour preview (not a historical thumbnail); no Badges/Games list with Play/Edit (BLOXEN has no user places); online state always Offline (no presence) |',
    '| Inventory | category list + tile grid + pagination | only the catalog-backed categories exist (no uploads) |',
    '| Travel / Registry / People / Favorites | BLOXEN-specific or minimal | no 2015 equivalent of Travel/Registry; People ~ user search |',
    '| Login / Register / logged-out Home | landing layout from the Web Design Museum reference + ARCHIVED-EXACT copy | photographs MISSING |', ''];
  fs.writeFileSync(path.join(outDir, 'visual-validation.md'), md.join('\n')); console.log(fails ? fails + ' FAIL' : 'all pass', rows.length, geom.length); for (const g of geom.filter(g => !g.ok)) console.log('GEOM FAIL', g.n, g.detail || ''); if (fails) for (const r of rows.filter(r => !r.ok)) console.log('FAIL', r.page, r.sel, r.prop, 'expected', r.exp, 'got', r.got); process.exit(fails ? 1 : 0);
})().catch(e => { console.error(e); process.exit(2); });
