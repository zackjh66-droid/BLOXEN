# WEBSITE — 2015 site reconstruction

Implementation: `src/web/{app,views,main}.js`, `static/css/bloxen.css`, `static/css/brickcolors.css`; zero dependencies; server-rendered; CSP `default-src 'self'` with **no inline styles or scripts**. Every page carries a fixed-position banner "BLOXEN is an unofficial, non-commercial preservation project…" and a footer with the target client version. The banner and footer are the only additions to the period UI.

## Pages (all implemented, all render 200; screenshots in `research/visual/`)
| Required page | Route | Content evidence | Layout/CSS evidence |
|---|---|---|---|
| Home (logged out) | `/` | **ARCHIVED-EXACT text** (Wayback 20150723171630): "You Make the Game ™", "Sign up and start having fun!", signup form fields Username/Password/Birthday/Gender, "What is ROBLOX?" paragraph | RECONSTRUCTED-FROM-EVIDENCE |
| Home (logged in) | `/home` | "Hello, <name>!", Recently Played, Favorites, Friends — structure INFERRED from memory of 2015 | RECONSTRUCTED |
| Games | `/games` | Sort names, time filters and genre list from Wayback 20150726062742 (the list itself is JS-loaded in the capture and absent); BLOXEN lists only its evidence-backed places | RECONSTRUCTED |
| Game Details | `/games/:id` | Adds an honest **provenance panel** (grade, SHA-256, evidence, playability badge) that did not exist in 2015 — deliberate deviation for preservation honesty | RECONSTRUCTED |
| Catalog / Catalog Item | `/catalog`, `/catalog/item/:id` | Real archived items (see CATALOG.md); thumbnails shown as explicit "MISSING" tiles | RECONSTRUCTED |
| Profile | `/users/:id/profile` | INFERRED | RECONSTRUCTED |
| Character, Inventory | `/my/character`, `/my/inventory` | INFERRED; body-colour picker + wardrobe; figure is a **labelled BLOXEN colour preview, not a thumbnail** | RECONSTRUCTED |
| Friends, Groups, Messages | `/my/friends`, `/groups/:id`, `/my/groups`, `/my/messages` | INFERRED | RECONSTRUCTED |
| Develop | `/develop` | INFERRED (static page; no upload — uploads deliberately not implemented) | RECONSTRUCTED |
| Search | `/search?q=` | INFERRED | RECONSTRUCTED |
| Login / Register | `/login` (`/Login` redirects), `/register` | 2015 `/Login` → `/NewLogin` redirect observed in Wayback 20150724123053 | RECONSTRUCTED |
| Account Settings | `/my/account` | INFERRED | RECONSTRUCTED |

## Visual comparison — honest statement
**No 2015 pixel reference was obtainable.** Wayback HTML captures exist but the CSS/image sub-resources were not retrievable from this sandbox, and the only real-CSS source (RobloxLabs/web) is 2012–14. The method actually used:
1. *Reference*: archived 2015 text/structure for logged-out Home and Games; 2012–14 CSS numeric values (header #2D528F, hover #27487E, 40 px fixed header, 16 px header links, buttons #007001 / #0852b7 / #565656, link #095fb5, text #343434, h1 32 px, 970 px page width).
2. *Render*: `node tools/screenshot_pages.js` → `research/visual/*.png` (Chromium, 1100 px viewport).
3. *Compare* against those values/structures, *correct*, re-render.

Findings corrected this way: (a) inline `style=` attributes silently blocked by CSP left the avatar figure and colour swatches blank → replaced with CSS classes and a generated `brickcolors.css`, test added; (b) logged-in header wrapped the "Character" link onto a second line at 970 px → search/links sized to fit.

Known residual deviations (NOT corrected, recorded honestly): font is a web-safe sans stack, not the Source Sans Pro used in 2015 (no font file retrieved); no sprite sheets/logo (the meatball logo SVG path is known from the capture but the file was not retrieved, so a text "BLOXEN" wordmark is used); Games/Catalog tiles use a flat grid rather than the 2015 gradient cards; no thumbnails at all. The site is therefore **structurally faithful and unmodernised but not pixel-verified**. Status: PARTIAL.

## Accounts & security (UNIT/INTEGRATION-TESTED, `tests/web.test.js`)
scrypt password hashing; sessions stored only as SHA-256 of the cookie token (`HttpOnly; SameSite=Lax`); CSRF token on every POST (double-submit cookie before login); login throttling (429); output escaping (XSS test); parameterised SQL only (SQLi test); open-redirect guard on `next`; static-file traversal blocked; security headers. Registration never asks for a Roblox password and BLOXEN never reads or stores Roblox cookies. The start allowance of 100,000 R$ is a BLOXEN choice and is labelled NOT historical.
