# WEBSITE — 2015 site reconstruction

Implementation: `src/web/{app,views,main}.js`, `static/css/bloxen.css`, `static/css/brickcolors.css`; zero dependencies; server-rendered; CSP `default-src 'self'` with **no inline styles or scripts**. Every page carries a fixed-position banner "BLOXEN is an unofficial, non-commercial preservation project…" and a footer with the target client version. The banner and footer are the only additions to the period UI.

## UPDATE (evidence pass): real 2015 CSS/DOM found
A real Roblox item page saved on 2015-02-21 (with its `FetchCSS` bundles) and Wayback captures of Game Details (20150627) and Profile (20150903) were located and read; see `research/website/SOURCES.md`. The shared chrome (header `#0074bd`, left nav >=1480px, 970px `#BodyWrapper`, footer, buttons, tabs, item page sizes) now uses those ARCHIVED-NEAR-DATE values, replacing the 2012-14 and sampled values described further down (kept below as history). `src/web/components.js` holds the reusable pieces. Validation: `tools/visual_validate.js` -> `research/website/visual-validation.md` (computed-style comparison, **not** pixel-perfect; screenshots in `research/website/screens/`; `research/visual/` is the superseded pre-refit set). New/changed pages: Game Details (registry status, About/Compatibility/Evidence), `/games/registry`, catalog + inventory + people pagination, item favourites, `/my/favorites`, profile favourites, `/my/travel`, `/users`.

## Pages (all implemented, all render 200; older screenshots in `research/visual/`)
| Required page | Route | Content evidence | Layout/CSS evidence |
|---|---|---|---|
| Home (logged out) | `/` | **ARCHIVED-EXACT text** (Wayback 20150723171630) | layout from a real ~2015 screenshot (ARCHIVED-NEAR-DATE, reduced size); imagery MISSING |
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
**Pixel-level 2015 references are scarce.** What exists and was used:
| Reference | What it shows | Grade | Held where |
|---|---|---|---|
| Web Design Museum "Roblox in 2015" (`webdesignmuseum.org/uploaded/timeline/roblox/roblox-2015.jpg`; only a 500×1060 reduction could be obtained) | Complete logged-out Home: copy matches Wayback 20150723171630 exactly (ROBLOX Point / StarMarine614), so it is a mid-2015 capture | ARCHIVED-NEAR-DATE (date of the screenshot itself not stated) | `/home/user/quarantine/visual-ref/` (third-party image, **not committed**) |
| Fandom "Roblox Home" gallery "homepage from early 2015 to early 2016" | 185 px thumbnail of logged-in Home ("Hello, <name>!", avatar box, Recently Played thumbnails, blue header) | ARCHIVED-NEAR-DATE, too small to measure | quarantine |
| Wayback text of Home (20150723171630, 20150724155302), Games (20150726062742), /Login→/NewLogin redirect | Copy and control inventory, not layout | ARCHIVED-EXACT for text | URLs in HISTORICAL-SOURCE-MAP |
| RobloxLabs/web 2012–14 CSS | numeric values for logged-in chrome (header #2D528F, 40px, 970px, buttons) | ARCHIVED-NEAR-DATE (1–3 years earlier) for values only | quarantine |
Wayback CSS/image sub-resources could not be retrieved (sandbox cannot reach Wayback directly; the fetch tool returns markdown, not raw HTML/CSS).

**Correction made after finding the screenshot:** the first logged-out Home was built from the 2012–14 CSS and the archived text only, i.e. a two-column 970px page on white. The real 2015 page is a full-width landing: translucent teal top bar (Play / About / Platforms + inline Username/Password/Log In + "Forgot Username/Password?"), full-bleed hero photo with the logo, "You Make the Game ™", "Game: ROBLOX Point / Developer: StarMarine614" bottom-left and a translucent sign-up panel on the right (Username, Password, Confirm Password, Birthday, Gender, green Sign Up #00b35a), a dark "What is ROBLOX?" band, a three-screenshot strip, a light-grey (#f2f2f2) "ROBLOX on your device." band with App Store/Google Play badges, and a large footer link row (About Us, Jobs, Blog, Privacy, Parents, Help) on #fafafa. `/`, `/login` and `/register` now use this layout (`landing()` in views.js, `static/css/landing.css`). Login and Register pages reuse the landing chrome because the 2015 `/NewLogin` page was observed to be part of the same landing design family (structure INFERRED; no screenshot of it was found).

Method: reference → render (`node tools/screenshot_pages.js`, `research/visual/*.png`) → compare → correct.
Measured/compared: band order, nav items and their placement, panel position (right, ~50–84% of width) and translucency, control order in the form, button colour (sampled #00b35a), band colours (#f2f2f2/#fafafa sampled), footer link row. **Not measured:** absolute pixel dimensions (the reference is a 500px reduction of unknown original width), fonts (Source Sans Pro not retrievable), and every photographic asset — hero, character, three strip screenshots, device artwork, the logo — which are MISSING and rendered as labelled flat blocks, never substituted.

Deliberate deviations (honesty/safety, recorded): BLOXEN text wordmark instead of the logo; the preservation notice banner; password rule is BLOXEN's (8+ characters, not the 2015 "4 letters and 2 numbers"); gender shown as labelled radios instead of icons; footer links point at BLOXEN pages.

**Logged-in Home** was restyled the same way from the Fandom "Roblox Home" gallery thumbnail for the 2015–early-2016 homepage (185×99 px, upscaled for inspection): blue header (sampled ≈ #0e6ba7, ±10% because the thumbnail is blurry), light-grey page (#f2f2f2) instead of white, large light-weight "Hello, <name>!" beside the full-body avatar, a white Friends card of avatar tiles with a light-blue "See All" button, then a RECENTLY PLAYED row of square game tiles. The header colour and grey page background were applied site-wide (previously the 2014 #2D528F on white). Two other candidate 2015 screenshots found by image search turned out to be 2016–2017-era (left-sidebar layout) or modern and were NOT used. A narrow left sidebar visible in the thumbnail is empty/collapsed and is not reproduced.

Still NOT reference-checked (structure INFERRED from period memory and 2012–14 CSS): Games, Game Details, Catalog, Catalog Item, Profile, Character, Inventory, Friends, Groups, Messages, Develop, Search, Account Settings, and the logged-in Home beyond the 185 px thumbnail. These use the older-style 970px chrome and are the next targets if more 2015 screenshots can be found. Status: PARTIAL.

Defects found and fixed by screenshot inspection along the way: (a) CSP silently blocked inline `style=` so the avatar figure/swatches were blank → CSS classes + test; (b) logged-in header wrapped "Character" → resized; (c) logged-out Home structurally wrong (above).

## Accounts & security (UNIT/INTEGRATION-TESTED, `tests/web.test.js`)
scrypt password hashing; sessions stored only as SHA-256 of the cookie token (`HttpOnly; SameSite=Lax`); CSRF token on every POST (double-submit cookie before login); login throttling (429); output escaping (XSS test); parameterised SQL only (SQLi test); open-redirect guard on `next`; static-file traversal blocked; security headers. Registration never asks for a Roblox password and BLOXEN never reads or stores Roblox cookies. The start allowance of 100,000 R$ is a BLOXEN choice and is labelled NOT historical.
