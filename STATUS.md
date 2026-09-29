# BLOXEN STATUS

BLOXEN is an **unofficial, non-commercial preservation project**, not affiliated with Roblox Corporation.
Test labels: UNIT-TESTED · SIMULATOR-TESTED · REAL-CLIENT-TESTED. **No item is REAL-CLIENT-TESTED.** BLOCKED is per task.
Suite: `npm test` (all green at last run) · e2e: `npm run e2e`.

| Area | State | Detail |
|---|---|---|
| Research docs (`research/*.md`) | **DONE (first pass)** | Source map, provenance, client, protocol, website, catalog, games, assets. Gaps listed inside each. |
| Client acquisition + verification | **DONE** / execution **BLOCKED** | Hash, PE, Authenticode, manifest done; never executed (needs Windows VM + approval). |
| Place importer (.rbxl/.rbxlx) | **DONE, UNIT-TESTED** | quarantine→SHA-256→parse→tree→services/props/refs→inert scripts→asset ids→compat report. 125/125 archive files parse. `rbxlx.md` spec unread. |
| Game candidates | **PARTIAL** | 9 curated; 4 stored (3 accepted places + 1 alt); Happy Home and ROBLOX HQ accepted but not yet fetched into store. All provenance INFERRED; no 2015-era place found. |
| Script intake/classification | **PARTIAL** | Inventory + static classification (`analyze.js`); `REPLICATED_OK` list must be kept aligned with `world.js` (NOT enforced by test). 4 "post-2015-feature" files not individually identified. |
| Script runtime | **NOT STARTED** | No place is claimed playable; all are GEOMETRY-ONLY. |
| Website pages (16 types) | **PARTIAL** | All render, INTEGRATION-TESTED. Faithful to archived text/structure; **not pixel-verified** (no 2015 reference; font, logo, sprites, thumbnails missing). |
| Accounts/sessions/security | **DONE, TESTED** | scrypt, hashed sessions, CSRF, XSS/SQLi/traversal/redirect tests, throttling. Email, settings beyond password/blurb, favourites UI minimal. |
| Catalog | **PARTIAL** | 44 real archived items from two captures; no shirts/pants/heads; 7+ chunks unread; thumbnails/content MISSING. |
| Inventory/ownership/avatar/equip | **DONE, TESTED** | Wearable hats/faces/T-shirts equip; body colours reach spawned character (SIMULATOR-TESTED). Shirts/pants/packages not exercised for lack of items. Thumbnails: **NOT STARTED** (no renderer; labelled colour preview only). |
| Asset service | **PARTIAL** | Tracking table + 404/MISSING logging done; no real asset files stored. |
| Compat backend | **PARTIAL, SIMULATOR-TESTED** | PlaceLauncher/Join/Negotiate/CharacterFetch/BodyColors/asset/settings stubs/analytics sinks; loopback-only, host-allow-list, no proxying. Formats INFERRED; never hit by the real client. Join script unsigned (acceptance UNKNOWN). |
| Game server | **PARTIAL, UNIT + SIMULATOR-TESTED** | RakNet-style reliability/ordering, protocol 31, descriptor sync, legacy strings, SET_GLOBALS, ID_DATA, replication, move/spawn/respawn. Wire ids INFERRED. Authoritative server→client ID_DATA for the real client: **BLOCKED** on real-client capture. |
| Launcher + Play | **DONE, UNIT + SIMULATOR-TESTED** | Strict URI, hardcoded version/hash gate, working-copy only, no shell, loopback only, single-use 60-second tickets. Windows registration file generated, not exercised on Windows. |
| E2E without client | **DONE** | `tests/e2e-flow.test.js`: register→login→pages→buy/equip→Play→ticket→launcher (stub exe)→compat→game server→simulated client in a parsed place, for a synthetic fixture and the real Tabula Rasa. Stub exe/simulator ≠ real client. |
| Visual comparison | **PARTIAL** | `research/visual/*.png`, notes in WEBSITE.md; 2 real defects found and fixed. |
| Windows real-client gate | **BLOCKED (needs Windows VM + approval)** | Procedure in `docs/WINDOWS-REAL-CLIENT-VALIDATION.md`. |

## Open items, in priority order
1. Run the Windows gate (owner). Expect gates 4 (unsigned join script) and 9 (server ID_DATA) to be the hard ones.
2. Read remaining catalog chunks; find shirts/pants/heads/free items; verify hand-transcribed fields.
3. Find the July-2015 DeployHistory entry; cross-check client files against a second archive.
4. Independent cross-checks for place bytes; fetch Happy Home/ROBLOX HQ.
5. Safe script runtime design.
6. Pixel reference for 2015 pages if any becomes available; fonts/logo/sprites.
