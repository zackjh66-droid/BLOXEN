# Website design sources

Every visual value in `static/css/bloxen.css` (EVIDENCE CHROME block) and every structural class in `src/web/components.js` traces to one of these. Nothing from any source is copied into the repository as code or image: values were *observed* and re-typed; third-party material stays outside Git (`/home/user/quarantine/…`, never committed).

| Source | Tier | What it gave | Limits |
|---|---|---|---|
| Real Roblox item page saved 2015-02-21 (`Extras/HtmlDumps/2015PluginItemPageArchiveOrg…` in Intelinsidecom/Roblox-Webserver, no licence; contains the real `FetchCSS` bundles) | ARCHIVED-NEAR-DATE (5 months before target) | header `#0074bd` 40px, nav item padding/hover, search box, `.rbx-left-col` 175px `#f2f2f2` (>=1480px), `#BodyWrapper` 970px, font stack, `#343434`, link `#0055B3`, footer gradient/legal line/colours, button sizes and colours, `SquareTabGray`, `#Item`/`#Thumbnail`/`#Summary`/`.BuyPriceBox`, `.robux-text`, `.stat-label` | secondary repository copy of a Wayback save; values are CSS, not pixels; not July 2015 exactly |
| Wayback 20150723171630 logged-out home (fetched as text) | ARCHIVED-EXACT (copy) | logged-out landing copy | images not obtainable |
| Wayback 20150627045216 `/games/9689581/Roblox-High-School` | ARCHIVED-NEAR-DATE | Game Details structure: title, "By creator", Play, favourite/stat counts, tabs, info list, Running Games | rendered text only |
| Wayback 20150903181745 `User.aspx?ID=1` | ARCHIVED-NEAR-DATE (2 months after) | Profile structure: "<name>'s Profile", [ Offline ], friends, Favorites category dropdown, "Page 1 of 2" pagination | rendered text only |
| Wayback launch interstitial text | ARCHIVED-NEAR-DATE | "Starting Roblox… Connecting to Players…" wording reference | not reproduced (BLOXEN's launch page states what it really does) |
| Leanbase CSS 20160707 (Wayback) | ARCHIVED-NEAR-DATE (~1 year after) | cross-check only | later era |
| Web Design Museum "Roblox in 2015" | SECONDARY | logged-out landing composition | image held in quarantine only |
| barbiewire/backtoroblox, RobloxLabs/web (MIT), anthony1x6000/ROBLOX2016stylus (MIT) | SECONDARY | page grey `#e3e3e3` and cross-checks | fan restyles; values only |

Rejected/not used: `Roblox/gear` (licence forbids emulating use), leak-like forks of RobloxLabs site repos, `madblox-src`, any modern-DOM restyle as a structural source.

## Explicitly MISSING / INFERRED
- Logo, sprites, icons, thumbnails, photographs, the Source Sans Pro font files: MISSING (labelled blocks; Arial fallback).
- Page composition beyond the recorded values (Home friends row, Character page, Inventory, Groups, Messages, Account Settings): RECONSTRUCTED-FROM-EVIDENCE/INFERRED.
- The currency is a local play allowance, not a historical economy; there is no ROBUX tab.

Validation of the CSS against these values: `tools/visual_validate.js` -> `research/website/visual-validation.md`.
