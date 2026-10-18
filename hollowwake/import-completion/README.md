# Hollowwake main-client import completion — 2026-10-18

This is completed source and test evidence, **not a completed Fortnite port or a new game-ready APPX**. Work remains in `zackjh66-droid/BLOXEN`, branch `hollowwake/native-system-route`. Dealbot/Project Zero was not modified. Existing companion, live Sites deployment, accounts, owner-only admin and rollback releases are preserved.

## Implemented groups

- Virtual fullscreen display enumeration/current-mode queries derive width/height from the actual bound CoreWindow viewport. The surface reports its virtual identity explicitly; physical display mode changes, unknown refresh rates, registry writes and extra monitors are not invented.
- Real host pointer observations and relative mouse deltas flow into the production message queue. Implemented x64 raw-mouse registration, input/device enumeration, opaque packet handles, two-phase buffer queries, lifetime cleanup, button/wheel events, foreground gating and registration rollback. Unsupported keyboard/HID/background raw capture and unavailable hardware metadata return explicit errors. This is partial raw input, not a complete HID stack.
- Cursor position/visibility use real host dispatcher callbacks. Failed/unsupported host operations retain errors and do not change the stored state. The event-queue mutex is not held while waiting for cursor visibility callbacks.
- `TranslateMessage` consumes text from actual CoreWindow CharacterReceived events associated with guest key messages, preserving scan/repeat/extended/context state. It handles early/late text and UTF-16 pairs without posting duplicate characters. IME composition, arbitrary caller-constructed key translation and complete Win32 keyboard-layout APIs are not implemented.
- Main-image read-only WinHTTP configuration queries bind to the retained Microsoft system provider with exact ABI, allocations and errors. No HTTP request/session APIs are exposed by that adapter; this does not implement Fortnite authentication.
- The actual launch preflight now audits all 838 fixed unredirected platform pairs, separately from the existing 843 redirected pairs. It retains already loaded OS providers or uses the real OS loader restricted to System32; it searches neither GameClean nor arbitrary archive paths. Every missing export/provider still fails preflight. Export presence is not proof of API semantics or Xbox support.
- Full platform audit JSON is available through the existing controller-paged Full log view and `LocalState/native-platform-imports.json`.

## Content and private installation

The existing split-content/native-package implementation remains in use. Converted code, matching dependency images and proxy DLLs belong in the signed private package; executable code is not loaded from LocalState. Authenticated data manifests verify paths, sizes and SHA-256 before extracting data only into GameClean, connected through the file bridge. No proprietary inputs, private manifests, credentials or private signing keys are in this source delivery.

The build workflow compiles/signs the own-code app for AppContainer checks but uploads evidence only. Private preparation/build instructions remain in `../native-client-0.5.0/README.md`, `Build.ps1` and the private-package source. No new diagnostic-only game release is being offered. A meaningful real-client package still requires the missing compatibility implementation and platform validation described in the report.

## Archive source

The preserved 12,528,679,454-byte original `6.21.zip` was rehashed this iteration: SHA-256 `bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5`, matching the recorded original. The main client pin remains `9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478`. These are inspection pins from a community archive, not Epic authenticity certificates.

Reboot's catalog lists Windows 6.21 with primary CL-4526925 (also mentions CL-4531851/4535631). Actual availability points to `https://builds.rebootfn.org/windows-6.21.rar`; HEAD returned 200, 10,869,300,932 bytes, no redirect observed. This RAR was not downloaded/substituted because the preserved ZIP is accessible. Therefore no hash/content equivalence to the ZIP is claimed. See `reboot-destination.json`.

## Website candidate

`home-scroll.patch` applies **only** home presentation changes to the current Sites source: chapter sections, sticky anchor navigation, reading progress, restrained reveals/background motion, native scrolling and reduced-motion fallback. It preserves account forms and routes. `sites-candidate/` is a sanitized offline source candidate, including the prior authenticated-content endpoint patch; it contains no live database, credentials or Site source Git metadata.

Echo returned Cloudflare's 403 challenge on both tested hostnames. The implementation is original Hollowwake scrolling, not a verified exact copy of Echo. Local Chromium checks cover 1440×1000 and 390×844, overflow, reveals/progress, reduced motion, preserved account navigation and real registration/device approval/revocation through a disposable local Worker database. Screenshots show implemented local pages, not a live deployment or design mockup.

The live site remains https://hollowwake.jihnjohn10.chatgpt.site (version 10). Sites' required `scripts/site-workflow.mjs` helper is absent from the available installation. Its skill says: “The helper owns Git preparation, ordered commands, commit/push, and packaging.” No unsupported replacement publishing workflow was used. The existing deployment/accounts are unchanged; the patch is buildable/tested locally but not deployed.

## Reproducibility

Use the existing hosted Windows-2022 workflow `.github/workflows/hollowwake-native-client-050.yml`. `[no-private-inputs]` runs own-source compilation/signature, actual input/file/socket/startup tests and AppContainer resolution checks without fetching/executing game code. `Build.ps1` writes exact-name reports and `tests/compare_imports.py` checks the immutable before/after inventories. Preserve the sibling source folder layout under `hollowwake/`.

For the offline website candidate: first run `node hollowwake/import-completion/Restore-CandidateAssets.mjs` to restore the shared public background/font from the sibling app assets (deduplicated in the source ZIP). Screenshots are delivered separately from the source archive. Its existing lockfile, `npm run build` and `npm test` build/test the real Worker/auth service. Browser check scripts target only the disposable preview; they do not access production accounts. Hosted publication still requires the missing Sites helper; no user's local compilation is required or requested.
