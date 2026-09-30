# Windows real-client validation (the final gate) — NOT YET PERFORMED

> **REAL CLIENT EXECUTION = BLOCKED: WINDOWS REQUIRED** · **AUTHORITATIVE SERVER→CLIENT ID_DATA VALIDATION = BLOCKED: REAL CLIENT CAPTURE REQUIRED** (frozen in `STATUS.md`). The non-Windows side of BLOXEN is complete; this document is what remains.

Everything in this repository is **UNIT-TESTED or SIMULATOR-TESTED**. Nothing has been tested with `RobloxPlayerBeta.exe`. This document is the procedure for doing so; it must be run by a person on Windows, in a disposable environment, with approval. This project has never executed the binary and its automation must not.

## Rules (do not bypass)
* Use a **disposable Windows VM/Windows Sandbox snapshot** with no personal data, no saved browser sessions and no Roblox login. Keep Windows Defender, Firewall, SmartScreen and UAC **enabled**; do **not** add AV exclusions. If Defender flags the file, stop and record it — do not override.
* A matching SHA-256 and a valid signature are provenance evidence, not a safety guarantee (the binary is a 2015 x86 executable with known-unpatched vulnerabilities). Keep the VM off your LAN where possible, or host-only networking.
* Never enter real Roblox credentials. BLOXEN accounts are local-only. Never copy Roblox cookies.
* Compat/game-server ports bind to loopback only. Do not expose them. BLOXEN refuses non-loopback binds unless `BLOXEN_ALLOW_REMOTE=1`; don't set it.

## Setup
1. Install Node ≥ 22.13 in the VM. Clone this repo. `node tools/fetch_places.js tabularasa crossroads-2007-client` (verifies SHA-256).
2. Obtain the client tree from `KloBraticc/2015-Client` @ `7a0742c` (`July 23 (0.205.0.61876)/`; see `preservation/manifests/client-0.205.0.61876.json` for all 771 paths + sizes). **Copy** it to a working directory, e.g. `C:\BLOXEN\client-working-copy\version-0d46087630eb46cd`. Keep the original untouched.
3. Verify (before anything runs): `Get-FileHash RobloxPlayerBeta.exe -Algorithm SHA256` must equal `384a4cb38de6977899e09e59c2136619fef521dc7b4adeb34d404945120c8a44`; `Get-AuthenticodeSignature` should show ROBLOX Corporation (the certificate expired 2015-09-09 — expect "NotTimeValid"/expiry warnings unless timestamp-verified; record exactly what you see).
4. Copy `src/launcher/launcher.config.example.json` → `src/launcher/launcher.config.json` and set `clientDir` (absolute, the working copy), `compatBase` `http://127.0.0.1:8081`, `logFile`. `node src/launcher/launcher.js --verify` must print `OK`.
5. Register the protocol for the current user only: create `C:\BLOXEN\launch.cmd` containing `@node C:\BLOXEN\repo\src\launcher\launcher.js %1` , run `node src/launcher/launcher.js --print-reg C:\BLOXEN\launch.cmd > bloxen.reg`, read it (HKCU only), import it.
6. `npm start` (web :8080, compat :8081, game servers on demand, loopback).

## Gate sequence — record PASS/FAIL + logs for each; stop at the first FAIL
| # | Gate | Where to observe |
|---|---|---|
| 1 | Browser: register → login → Games → Tabula Rasa → Play | web, `launch_tickets` row |
| 2 | Launcher accepts URI, hashes exe, redeems ticket, spawns client with `-a -t -j` | `launcher.log` (ticket never logged) |
| 3 | Client starts, reads `AppSettings.xml` BaseUrl = compat | `compat_log` table (`SELECT * FROM compat_log`) |
| 4 | Client requests PlaceLauncher / Join.ashx / Negotiate | `compat_log`. **Unknown: does the client accept the unsigned INFERRED join script?** If it rejects (`rbxsig`), document; needs a signing-compatible approach that does not forge Roblox credentials — assess before proceeding. |
| 5 | RakNet connect to the game-server port | capture with Wireshark on loopback (Npcap) |
| 6 | Protocol 31 sync; descriptor sync (332 classes / 968 properties / 320 events / 182 types) | server log |
| 7 | SET_GLOBALS accepted (22 containers, first class 231) | server log |
| 8 | Client emits ID_DATA | server log / capture |
| 9 | **Authoritative server ID_DATA accepted** — replace `bloxen-profile-v0` ids with the real layout (the open hard problem; see research/PROTOCOL.md) | capture diff |
| 10 | Workspace + place geometry replicated; visible Part | screenshot |
| 11 | Player object → avatar → character (body colours; MISSING hats absent) | screenshot |
| 12 | Camera + input → movement → respawn | video |
| 13 | "Playable" — only for geometry-only places; scripted behaviour needs the (not started) script runtime | — |

Missing assets requested by the client appear in `missing_asset_log` and return 404; that is expected and must not be "fixed" with substitutes presented as historical.

## Reporting
Commit results under `docs/validation-results/<date>.md` with: VM image, Defender state, hash output, each gate's verdict, logs (redacted), captures' SHA-256. Only then may STATUS.md change any item to REAL-CLIENT-TESTED.
