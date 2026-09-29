# BLOXEN STATUS

BLOXEN is an **unofficial, non-commercial preservation project**, not affiliated with Roblox Corporation.
Test labels: UNIT-TESTED · SIMULATOR-TESTED · REAL-CLIENT-TESTED. **No item is REAL-CLIENT-TESTED.** BLOCKED is per task.
Suite: `npm test` (all green at last run) · e2e: `npm run e2e`.

| Area | State | Detail |
|---|---|---|
| Research docs (`research/*.md`) | **DONE (first pass)** | Source map, provenance, client, protocol, website, catalog, games, assets. Gaps listed inside each. |
| Client acquisition + verification | **DONE** / execution **BLOCKED** | Hash, PE, Authenticode, manifest done; never executed (needs Windows VM + approval). |
| Place importer (.rbxl/.rbxlx) | **DONE, UNIT-TESTED** | quarantine→SHA-256→parse→tree→services/props/refs→inert scripts→asset ids→compat report. 125/125 archive files parse. `rbxlx.md` spec read and conformance-tested (`tests/rbxlx-spec.test.js`). |
| Game candidates | **PARTIAL** | 9 curated; 7 fetched and SHA-256-verified into the git-ignored store (Tabula Rasa, both Crossroads files, Happy Home, Roblox HQ, World Headquarters, Mission to the Moon); each joins through the full e2e flow (SIMULATOR-TESTED, geometry only). All provenance INFERRED; no 2015-era place found. |
| Script intake/classification | **PARTIAL** | Inventory + static classification (`analyze.js`); replication support is taken from `world.js` (single source of truth). The 4 files first flagged post-2015 are itemised in `preservation/manifests/places-post2015-itemised.json` (2 truly post-2015, 2 era-consistent but provenance-quarantined). |
| Script runtime | **PARTIAL, UNIT + SIMULATOR-TESTED (experimental, off by default)** | Sandboxed Lua 5.1 interpreter + ScriptHost (`src/script/`, `research/SCRIPTING.md`). Crossroads regeneration, day/night and leaderboard scripts run; Touched/BodyMover scripts do not (no server physics). Enable with `BLOXEN_SCRIPTS=1`. No place is claimed playable. |
| Website pages (16 types) | **PARTIAL** | All render, INTEGRATION-TESTED. Logged-out Home/Login/Register rebuilt to match a real ~2015 screenshot (structure, colours sampled); the other pages are still from 2012–14 CSS values + period memory (no 2015 reference found) and NOT pixel-verified. Photos, logo, fonts, thumbnails MISSING. |
| Accounts/sessions/security | **DONE, TESTED** | scrypt, hashed sessions, CSRF, XSS/SQLi/traversal/redirect tests, throttling. Email, settings beyond password/blurb, favourites UI minimal. |
| Catalog | **PARTIAL** | 83 distinct real archived items from two fully-read captures (first pages only); no shirts/pants/heads; thumbnails/content MISSING; resale and Builders Club not implemented. |
| Inventory/ownership/avatar/equip | **DONE, TESTED** | Wearable hats/faces/T-shirts equip; body colours reach spawned character (SIMULATOR-TESTED). Shirts/pants/packages not exercised for lack of items. Thumbnails: **NOT STARTED** (no renderer; labelled colour preview only). |
| Asset service | **PARTIAL, UNIT/INTEGRATION-TESTED** | Tracking table, hash-addressed store, `tools/register_asset.js` (provenance required), AVAILABLE only while SHA-256 verifies (re-checked per serve), 404/MISSING logging all done. **No real asset file has been retrieved**: the committed manifest is empty, every asset is MISSING. |
| Compat backend | **PARTIAL, SIMULATOR-TESTED** | PlaceLauncher/Join/Negotiate/CharacterFetch/BodyColors/asset/settings stubs/analytics sinks; loopback-only, host-allow-list, no proxying. Formats INFERRED; never hit by the real client. Join script unsigned (acceptance UNKNOWN). |
| Game server | **PARTIAL, UNIT + SIMULATOR-TESTED** | (no standalone entry point: `GameServerManager` starts one per place on demand from the web app; verified `npm start`)  RakNet-style reliability/ordering, protocol 31, descriptor sync, legacy strings, SET_GLOBALS, ID_DATA, replication, move/spawn/respawn. Wire ids INFERRED. Authoritative server→client ID_DATA for the real client: **BLOCKED** on real-client capture. |
| Launcher + Play | **DONE, UNIT + SIMULATOR-TESTED** | Strict URI, hardcoded version/hash gate, working-copy only, no shell, loopback only, single-use 60-second tickets. Windows registration file generated, not exercised on Windows. |
| E2E without client | **DONE** | `tests/e2e-flow.test.js`: register→login→pages→buy/equip→Play→ticket→launcher (stub exe)→compat→game server→simulated client in a parsed place, for a synthetic fixture and the real Tabula Rasa. Stub exe/simulator ≠ real client. |
| Visual comparison | **PARTIAL** | `research/visual/*.png`, notes in WEBSITE.md; 2 real defects found and fixed. |
| Windows real-client gate | **BLOCKED (needs Windows VM + approval)** | Procedure in `docs/WINDOWS-REAL-CLIENT-VALIDATION.md`. |

## Open items, in priority order
1. Run the Windows gate (owner). Expect gates 4 (unsigned join script) and 9 (server ID_DATA) to be the hard ones.
2. Shirts/pants/heads/free items: the Wayback CDX shows only two distinct `catalog/json` queries in 2015 (both read). Individual July-2015 `*-item?id=` pages exist (for example user-made T-shirts) and give title/creator/created/description, but the text extraction drops the price, and the pages read so far were user-made. Needs another route (full-text HTML extraction or a per-item price source) before adding them. Verify hand-transcribed fields.
3. Cross-check the other 769 client files against a second archive (DeployHistory line and ReflectionMetadata are already cross-checked).
4. Independent cross-checks for place bytes (only repo-internal hashes exist today).
5. Part-vs-part contact needs physics; scripts-as-tree-members is low priority (only the cross-place Crossroads teleporters need it). See `research/SCRIPTING.md` §4.
6. Pixel reference for 2015 pages if any becomes available; fonts/logo/sprites.
