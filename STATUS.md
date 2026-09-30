# BLOXEN STATUS

BLOXEN is an **unofficial, non-commercial preservation project**, not affiliated with Roblox Corporation.
Test labels: UNIT-TESTED · SIMULATOR-TESTED · REAL-CLIENT-TESTED. **No item is REAL-CLIENT-TESTED.** BLOCKED is per task.
Suite: `npm test` = 163 tests, 163 pass, 0 fail, 0 skipped (last full run) · e2e: `npm run e2e`.

## Frozen gates (do not change without new evidence or a Windows run)

- **REAL CLIENT EXECUTION = BLOCKED: WINDOWS REQUIRED**
- **AUTHORITATIVE SERVER→CLIENT ID_DATA VALIDATION = BLOCKED: REAL CLIENT CAPTURE REQUIRED**

The non-Windows side is finished to the point where only these two gates (and evidence BLOXEN cannot invent) remain. Packet logging (`GameServer` logger, compat log), the protocol simulator (`src/sim/client.js`) and the Windows procedure (`docs/WINDOWS-REAL-CLIENT-VALIDATION.md`) are kept. **Server→client ID_DATA is not guessed**: the byte layout implemented is the BLOXEN profile `bloxen-profile-v0` (INFERRED) and is labelled as such everywhere.
Nothing blocked here blocks the website, importer, catalog, registry, launcher, tickets, compat backend or simulator work, which are done.

| Area | State | Detail |
|---|---|---|
| Research docs (`research/*.md`) | **DONE (first pass)** | Source map, provenance, client, protocol, website, catalog, games, assets. Gaps listed inside each. |
| Client acquisition + verification | **DONE** / execution **BLOCKED** | Hash, PE, Authenticode, manifest done; never executed (needs Windows VM + approval). |
| Place importer (.rbxl/.rbxlx) | **DONE, UNIT-TESTED** | quarantine→SHA-256→parse→tree→services/props/refs→inert scripts→asset ids→compat report. 125/125 archive files parse. `rbxlx.md` spec read and conformance-tested (`tests/rbxlx-spec.test.js`). |
| Game registry | **DONE, UNIT + INTEGRATION-TESTED** | `src/lib/registry.js`: 125 rows, **PRESERVED 7 / QUARANTINED 3 / REJECTED 3 / UNKNOWN 112**; only PRESERVED is offered (Games page, Play gate `registry.playability`); `/games/registry` shows all; per-place compatibility reports in `preservation/manifests/place-compat-reports.json` (`tools/build_compat_reports.js`). Status is provenance, never playability. |
| Game candidates | **PARTIAL** | 9 curated; 7 fetched and SHA-256-verified into the git-ignored store (Tabula Rasa, both Crossroads files, Happy Home, Roblox HQ, World Headquarters, Mission to the Moon); each joins through the full e2e flow (SIMULATOR-TESTED, geometry only). All provenance INFERRED; no 2015-era place found. |
| Script intake/classification | **PARTIAL** | Inventory + static classification (`analyze.js`); replication support is taken from `world.js` (single source of truth). The 4 files first flagged post-2015 are itemised in `preservation/manifests/places-post2015-itemised.json` (2 truly post-2015, 2 era-consistent but provenance-quarantined). |
| Script runtime | **PARTIAL, UNIT + SIMULATOR-TESTED (experimental, off by default)** | Sandboxed Lua 5.1 + ScriptHost (`src/script/`, `research/SCRIPTING.md`). Scripts' own children are visible to Lua but never replicated. Per-script inert index: `research/SCRIPT-INDEX.md` / `script-index.json` (100 scripts across 7 places incl. nested ones). Crossroads regeneration, day/night, leaderboard and the TeleportScript chain run; BodyMover/Velocity scripts do not (no physics: `research/PHYSICS.md`). Enable with `BLOXEN_SCRIPTS=1`. No place is claimed playable. |
| TeleportService | **DONE, UNIT + SIMULATOR-TESTED** | Server-side Lua `Teleport` is logged and recorded; mapped historical PlaceIds (1818, 1501, 1784) resolve to PRESERVED local places pinned by SHA-256; everything else is a controlled `unsupported-local-destination` and nothing is forwarded to Roblox. Compat PlaceLauncher travel route (INFERRED response shape), `/my/travel` Continue = a normal Play ticket. |
| Physics / Touched | **Simplified contact model only; REAL PHYSICS-ACCURATE TOUCHED = BLOCKED/UNIMPLEMENTED** | `research/PHYSICS.md`. |
| Website pages | **PARTIAL (structure/values from 2015 evidence; not pixel-verified)** | 16 page types + Game Registry, Favorites, Travel, People; `src/web/components.js`; header/left-nav/footer/buttons/tabs/item page from real 2015-02-21 CSS (ARCHIVED-NEAR-DATE), Game Details from the 20150627 capture, Profile from 20150903. Pagination (catalog, inventory, registry, people), item + game favorites, inventory categories, profile favorites. Logo/sprites/fonts/thumbnails/photographs MISSING. INTEGRATION-TESTED (`tests/site2.test.js`). |
| Accounts/sessions/security | **DONE, TESTED** | scrypt, hashed sessions, CSRF, XSS/SQLi/traversal/redirect tests, throttling. Email, settings beyond password/blurb, favourites UI minimal. |
| Catalog | **PARTIAL** | 83 distinct real archived items from two fully-read captures (first pages only); no shirts/pants/heads; thumbnails/content MISSING; resale and Builders Club not implemented. |
| Inventory/ownership/avatar/equip | **DONE, TESTED** | Hats/faces/T-shirts/packages equip (shirts/pants/heads/gear: no archived items); body colours, face decal, T-shirt graphic reach the spawned character; hat/package content MISSING is reported, never substituted (`tests/avatar-server.test.js`, SIMULATOR-TESTED). Both archived Faces are Limited U (not purchasable). Thumbnails: **NOT STARTED** (labelled colour preview only). |
| Asset service | **PARTIAL, UNIT/INTEGRATION-TESTED** | Tracking table, hash-addressed store, `tools/register_asset.js` (provenance required), AVAILABLE only while SHA-256 verifies (re-checked per serve), 404/MISSING logging all done. **No real asset file has been retrieved**: the committed manifest is empty, every asset is MISSING. |
| Compat backend | **PARTIAL, SIMULATOR-TESTED** | PlaceLauncher/Join/Negotiate/CharacterFetch/BodyColors/asset/settings stubs/analytics sinks; loopback-only, host-allow-list, no proxying. Formats INFERRED; never hit by the real client. Join script unsigned (acceptance UNKNOWN). |
| Game server | **PARTIAL, UNIT + SIMULATOR-TESTED** | (no standalone entry point: `GameServerManager` starts one per place on demand from the web app; verified `npm start`)  RakNet-style reliability/ordering, protocol 31, descriptor sync, legacy strings, SET_GLOBALS, ID_DATA, replication, move/spawn/respawn. Wire ids INFERRED. Authoritative server→client ID_DATA for the real client: **BLOCKED** on real-client capture. |
| Launcher + Play | **DONE, UNIT + SIMULATOR-TESTED** | Strict URI, hardcoded version/hash gate, working-copy only, no shell, loopback only, single-use 60-second tickets. Windows registration file generated, not exercised on Windows. |
| E2E without client | **DONE** | `tests/e2e-flow.test.js`: register→login→pages→buy/equip→Play→ticket→launcher (stub exe)→compat→game server→simulated client in a parsed place, for a synthetic fixture and the real Tabula Rasa. Stub exe/simulator ≠ real client. |
| Visual comparison | **DONE for what evidence allows** | `tools/visual_validate.js` compares browser-computed CSS and geometry with the recorded 2015 values (56 style + 20 geometry checks) and found/fixed real layout defects; `research/website/visual-validation.md`, `research/website/screens/`, `research/website/SOURCES.md`. **No pixel-perfect claim** (no pixel reference exists). |
| Windows real-client gate | **BLOCKED: WINDOWS REQUIRED** | Procedure in `docs/WINDOWS-REAL-CLIENT-VALIDATION.md`. Server→client ID_DATA: **BLOCKED: REAL CLIENT CAPTURE REQUIRED**. |

## Open items, in priority order
1. **Windows gate (owner):** run `docs/WINDOWS-REAL-CLIENT-VALIDATION.md` in a safe VM. Expect gates 4 (unsigned join script) and 9 (server ID_DATA) to be the hard ones; capture packets there, then replace `bloxen-profile-v0` with the real encoding.
2. **New historical evidence needed:** July-2015 shirts/pants/heads/free items (only two `catalog/json` captures exist), real asset files (every asset is MISSING), a pixel reference for 2015 pages, fonts/logo/sprites, 2015-era place files with independent provenance, genre/play statistics for games.
3. Cross-check the other 769 client files against a second archive (DeployHistory line and ReflectionMetadata already cross-checked).
4. Part-vs-part contact and BodyMover/Velocity need physics (`research/PHYSICS.md`), which needs real-client behaviour to be faithful.
