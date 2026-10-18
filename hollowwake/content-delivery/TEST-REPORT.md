# Hollowwake split delivery and startup — verification report

All work is in zackjh66-droid/BLOXEN, branch hollowwake/native-system-route. Dealbot/Project Zero was not modified or used as source. Existing website v10, accounts, owner-only admin and rollback releases are unchanged.

## Tests actually completed

- Node backend: 13/13 pass (content-all-auth-tests.log). Registration, linking, persistence, revocation, browser/device separation and authenticated content manifest policy execute against disposable SQLite. Two stale assertions expecting human-code typo lockouts were corrected to the already-existing retry policy; persisted device-issuance throttling is still tested.
- Sites candidate: 17/17 pass (sites-content-tests.log), including real bundled Worker/D1-emulation registration/linking, content endpoint authentication, revocation and existing admin/profile prototype tests. Built from an offline candidate with only three production files changed. No production D1 writes or deployment.
- Hosted Windows run 39080840139, commit 605fe8629cc3028df6d818c5ac0395c9becca68e: native/UWP compilation, development signature verification, all 11 generated dependency proxies (843 exports), real production file/window/socket checks, content extraction/hash/path/code/replacement/cancel/auth checks, image-local startup import rebinding, AppContainer import preflight and UIA diagnostic pagination/bounds passed. Own fixtures only; no downloaded game code executed.
- Final hosted Windows run 39081091409, commit 4f715dfc87828c0183f2a58ac3be190013c13c06: same checks passed after error-log integration. The WinMain test changes an actual imported function call in an original DLL and verifies that the provider is unchanged; it is not proof of full EXE CRT initialization or Xbox behavior.
- Private metadata generation: 139 data files, 26,449,827,840 expanded bytes, from the preserved SHA-pinned original ZIP. No game data is published. Full original archive is retained. Native inputs are still 26 prepared images/11 proxies, not a source port.

## Private packaging: distinguish the two results

Historical run 38965324618, commit b5867b9f14f5dfe492d5c27dbc97cae03585fa8f: BOTH build phases passed. A private development-signed game-containing x64 APPX was produced and every prepared image hash/proxy presence was audited. It was not executed or publicly uploaded; its ephemeral runner is not a private download service.

Current content-flow private attempt 39080697539, commit 1fcb0a50e2306a9b531d565cb1fddffd309e5d02: public compilation/checks passed; private input acquisition failed. The primary HTTP transfer was rejected as an invalid/truncated ZIP. Alternate source simplyblk failed verified Schannel HTTPS; rebootfn returned 404. No pin or TLS check was bypassed. The final startup-only run intentionally skipped another unchanged game-input download. Public workflow success does not override privateInputPackaging=failure in its outcome record.

No new diagnostic-only APPX is released as the requested game. This iteration's CI artifacts contain evidence only, not APPXs or proprietary payloads.

## Current failing operation

HWPreflightClient returns HRESULT 0x8007007F (ERROR_PROC_NOT_FOUND), before LoadPackagedLibrary and before the content transfer. 208 of 843 redirected symbol pairs remain unavailable (USER32 160, WINHTTP 15, WININET 9, WLDAP32 16, WS2_32 8); 70 occur in the main image. Exact names and importing modules are retained in unresolved-imports-by-module.json. There are another 838 unredirected symbol pairs across 62 OS DLLs whose Xbox availability is not verified.

The current missing requirements include correct raw input/window/paint/display semantics, remaining native HTTP and related dependency interfaces, full DXGI/D3D11 object/presentation compatibility, conventional CRT/EXE startup assumptions beyond the one rebound import, and verified Fortnite endpoint/authentication/presence integration. The root /client621 service is an account/profile/catalog protocol prototype, not a running-client login result. No original loading screen, character or lobby has rendered. Matchmaking remains disabled. Physical Xbox behavior is unverified; earlier companion linking remains user-reported Xbox success.

## Hosting limitation

The authenticated content endpoint and real Sites patch are ready and locally tested, but not deployed. The installed Sites instructions require scripts/site-workflow.mjs for source preparation/push/packaging. That script is absent from this executor and was unavailable at the tested cloud skill-resource paths. No unsupported source/deployment path or replacement account backend was used. Existing live URL remains https://hollowwake.jihnjohn10.chatgpt.site .
