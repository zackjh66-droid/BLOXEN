# Visual validation (computed-style comparison)

Generated 2026-09-30 by `tools/visual_validate.js`. Result: **all 76 checks pass** (56 style checks, 20 geometry checks).

## What this is, and what it is not

- **Method:** reference values -> render the real BLOXEN pages in headless Chromium -> read `getComputedStyle`/bounding boxes -> compare -> correct. Corrections already made on the way: the pre-evidence header colour (#0e6ba7, sampled from a blurry thumbnail) was replaced by #0074bd from the real 2015 CSS, the 2014-era values (#2D528F header etc.) were dropped, and the left nav / footer / tabs / item-page sizes were added from the recorded CSS. The tool itself then caught two defects that were fixed: fixed-width form inputs escaping the side column on /my/groups, and the R$ balance rendering dark-on-blue in the header. Any FAIL row below means the CSS currently disagrees with the recorded value.
- **Reference values** were read from real 2015 Roblox CSS/DOM: the 2015-02-21 Roblox item page (real `FetchCSS` bundles: `rbx-header`, `rbx-left-col`, `#BodyWrapper`, `#Footer`, buttons, `SquareTabGray`, `#Item`), ARCHIVED-NEAR-DATE (five months before the July 2015 target). Grey page colour `#e3e3e3` is SECONDARY. See `research/website/SOURCES.md`.
- **It is NOT a pixel-perfect claim.** No pixel-exact July-2015 screenshot is available (Wayback captures were only obtainable as rendered text), so no image diff was possible. Fonts: Source Sans Pro is named first but is not bundled, so the machine falls back to Arial. Logo, sprites, icons, thumbnails and photographs are MISSING and shown as labelled blocks. Copy on the logged-out home is ARCHIVED-EXACT (Wayback 20150723171630); page composition beyond the recorded values is RECONSTRUCTED-FROM-EVIDENCE or INFERRED.
- Screenshots: `research/website/screens/*.png` (1100px wide; `home-left-nav-1600.png` at 1600px to show the >=1480px left nav). `research/visual/` holds the older pre-refit set and is superseded.

## Style checks (computed vs recorded 2015 values)

| Page | Selector | Property | Expected | Computed | Evidence tier | |
|---|---|---|---|---|---|---|
| /home | `#header` | position | `fixed` | `fixed` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#header` | height | `40px` | `40px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#header` | backgroundColor | `rgb(0, 116, 189)` | `rgb(0, 116, 189)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #0074bd | PASS |
| /home | `.rbx-navbar a` | fontSize | `16px` | `16px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-navbar a` | paddingTop | `6px` | `6px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-navbar a` | paddingLeft | `9px` | `9px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-navbar a` | color | `rgb(255, 255, 255)` | `rgb(255, 255, 255)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-navbar a` | borderTopLeftRadius | `5px` | `5px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-search input` | height | `28px` | `28px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-search input` | borderTopColor | `rgb(184, 184, 184)` | `rgb(184, 184, 184)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #b8b8b8 | PASS |
| /home | `.rbx-search input` | borderTopLeftRadius | `3px` | `3px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.nav-content` | marginTop | `40px` | `40px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#BodyWrapper` | width | `970px` | `970px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#BodyWrapper` | backgroundColor | `rgb(255, 255, 255)` | `rgb(255, 255, 255)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `body` | fontSize | `14px` | `14px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `body` | color | `rgb(52, 52, 52)` | `rgb(52, 52, 52)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #343434 | PASS |
| /home | `body` | lineHeight | `19.992px` | `19.992px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) 1.428 | PASS |
| /home | `body` | fontFamily | `/^"?Source Sans Pro"?, Arial, Helvetica, sans-serif$/` | `"Source Sans Pro", Arial, Helvetica, sans-serif` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `body` | backgroundColor | `rgb(227, 227, 227)` | `rgb(227, 227, 227)` | SECONDARY (third-party restyle values) #e3e3e3 | PASS |
| /home | `#Footer` | fontSize | `13px` | `13px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#Footer` | paddingTop | `26px` | `26px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) (2em) | PASS |
| /home | `#Footer` | paddingBottom | `60px` | `60px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#Footer .legal` | borderTopColor | `rgb(53, 92, 149)` | `rgb(53, 92, 149)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #355C95 | PASS |
| /home | `#Footer .legal` | borderTopWidth | `1px` | `1px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#Footer .legalese` | color | `rgb(137, 159, 193)` | `rgb(137, 159, 193)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #899fc1 | PASS |
| /home | `#Footer .legalese` | fontSize | `10px` | `10px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `#Footer .FooterNav a` | color | `rgb(45, 82, 143)` | `rgb(45, 82, 143)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #2D528F | PASS |
| /home | `#Footer .FooterNav` | width | `970px` | `970px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home | `.rbx-left-col` | display | `none` | `none` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) (shown only >=1480px) @1100 | PASS |
| /home @1600 | `.rbx-left-col` | display | `block` | `block` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) @1600 | PASS |
| /home @1600 | `.rbx-left-col` | width | `175px` | `175px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home @1600 | `.rbx-left-col` | backgroundColor | `rgb(242, 242, 242)` | `rgb(242, 242, 242)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #f2f2f2 | PASS |
| /home @1600 | `.rbx-left-col` | top | `40px` | `40px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /home @1600 | `.rbx-left-col li a` | color | `rgb(25, 25, 25)` | `rgb(25, 25, 25)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #191919 | PASS |
| /games/crossroads-2007-client | `.btn-large` | height | `39px` | `39px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /games/crossroads-2007-client | `.btn-large` | paddingTop | `9px` | `9px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /games/crossroads-2007-client | `.btn-large` | fontSize | `23px` | `23px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /games/crossroads-2007-client | `.btn-large` | borderTopColor | `rgb(0, 112, 1)` | `rgb(0, 112, 1)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #007001 | PASS |
| /games/crossroads-2007-client | `.SquareTabGray a:not(.sel)` | backgroundColor | `rgb(214, 214, 214)` | `rgb(214, 214, 214)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #D6D6D6 | PASS |
| /games/crossroads-2007-client | `.SquareTabGray a:not(.sel)` | borderTopColor | `rgb(158, 158, 158)` | `rgb(158, 158, 158)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #9e9e9e | PASS |
| /games/crossroads-2007-client | `.SquareTabGray a:not(.sel)` | fontWeight | `700` | `700` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /games/crossroads-2007-client | `.SquareTabGray a:not(.sel)` | fontSize | `15px` | `15px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /games/crossroads-2007-client | `.SquareTabGray a.sel` | backgroundColor | `rgb(255, 255, 255)` | `rgb(255, 255, 255)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `.btn-medium` | height | `28px` | `28px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `.btn-medium` | fontSize | `20px` | `20px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `#Item` | width | `800px` | `800px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `#Thumbnail` | width | `320px` | `320px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `#Thumbnail .thumb` | height | `320px` | `320px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `#Summary` | width | `480px` | `480px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `.BuyPriceBox` | width | `158px` | `158px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) (CSS width) | PASS |
| /catalog/item/223785473 | `.BuyPriceBox` | backgroundColor | `rgb(225, 225, 225)` | `rgb(225, 225, 225)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #e1e1e1 | PASS |
| /catalog/item/223785473 | `.BuyPriceBox` | borderTopColor | `rgb(167, 167, 167)` | `rgb(167, 167, 167)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #a7a7a7 | PASS |
| /catalog/item/223785473 | `.robux-text` | color | `rgb(0, 102, 0)` | `rgb(0, 102, 0)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #060 | PASS |
| /catalog/item/223785473 | `.stat-label` | fontSize | `12px` | `12px` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) | PASS |
| /catalog/item/223785473 | `.stat-label` | color | `rgb(153, 153, 153)` | `rgb(153, 153, 153)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #999 | PASS |
| /catalog | `#BodyWrapper a.b, #BodyWrapper .tile a` | color | `rgb(0, 85, 179)` | `rgb(0, 85, 179)` | ARCHIVED-NEAR-DATE (2015-02-21 CSS) #0055B3 | PASS |

## Geometry checks

- PASS: no horizontal overflow at 1100px
- PASS: header spans full width and is 40px tall
- PASS: body wrapper centred
- PASS: content starts below the fixed header
- PASS: footer is below the content
- PASS: left nav does not overlap the page body at 1600px
- PASS: /games: no page overflow, nothing escapes #BodyWrapper
- PASS: /games/registry: no page overflow, nothing escapes #BodyWrapper
- PASS: /games/crossroads-uncopylocked-commit?tab=compat: no page overflow, nothing escapes #BodyWrapper
- PASS: /catalog?page=2: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/inventory: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/favorites: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/travel: no page overflow, nothing escapes #BodyWrapper
- PASS: /users?q=Pre: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/character: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/friends: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/groups: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/messages: no page overflow, nothing escapes #BodyWrapper
- PASS: /my/account: no page overflow, nothing escapes #BodyWrapper
- PASS: /develop: no page overflow, nothing escapes #BodyWrapper

## Per-page notes (what matches the archive, what does not)

| Page | Archive structure followed | Known differences |
|---|---|---|
| Home (logged in) | fixed 40px blue header, Games/Catalog/Develop, search, left nav >=1480px, 970px white body, footer with legal line | no logo image (MISSING); no ROBUX tab (BLOXEN has no currency); Friends/Recently Played/Favorites layout is RECONSTRUCTED from text captures |
| Games | sort tabs in `SquareTabGray`; tile grid | only sorts backed by local data are offered; archive had genre/time filters and play counts BLOXEN cannot know |
| Game Details (20150627 capture) | h1, "By creator", large green Play, favourite star + count, tabs, stat list, Running Games | tabs are About/Compatibility/Evidence (archive: About/Store/Leaderboards/Game Instances); no carousel/badges/passes/leaderboards (no data); BLOXEN adds registry status, provenance, compatibility |
| Catalog Item (2015-02-21 DOM) | `#Item` 800px, `#Thumbnail` 320px, `#Summary` 480px, `.BuyPriceBox`, favourite star, Item Owned | no Recommendations/Commentary tabs, no voting, no Builders Club price variants; thumbnails MISSING |
| Profile (20150903 capture) | "<name>'s Profile", [Offline], avatar, friends, Favorites with category dropdown | avatar is a labelled colour preview (not a historical thumbnail); no Badges/Games list with Play/Edit (BLOXEN has no user places); online state always Offline (no presence) |
| Inventory | category list + tile grid + pagination | only the catalog-backed categories exist (no uploads) |
| Travel / Registry / People / Favorites | BLOXEN-specific or minimal | no 2015 equivalent of Travel/Registry; People ~ user search |
| Login / Register / logged-out Home | landing layout from the Web Design Museum reference + ARCHIVED-EXACT copy | photographs MISSING |
