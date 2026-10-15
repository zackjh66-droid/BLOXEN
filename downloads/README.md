# Hollowwake 0.3 delivery report — 2026-10-15

## Delivered scope

This release is the independent Hollowwake x64 UWP companion, own-account website/backend and owner-private project administration. **No actual Fortnite client or lobby is included. The main requested Fortnite port is unfinished.**

Version 0.2 and its signing certificate/download branch remain intact. The pre-refresh website/native source is privately snapshotted under rollback/0.2; the public website's saved version 4 can be redeployed without deleting the database. Accounts, auth signing secret and existing D1 binding are preserved. New migration 0001 only adds five administration tables. Original 6.21.zip remains unchanged with SHA-256 `bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5`, rechecked this turn.

## Visual changes

Original monochrome H mark with a diagonal negative-space wake; new native icons/splash, website wordmark/favicon and Control branding. Original generated floating-island fan illustration inspired by Season 6 Loot Lake, a short translucent purple bottom gradient, clearer typography/cards/buttons and explicit focus visuals. Original Epic game files/artwork are not redistributed. These backgrounds are illustrations, not rendered-client evidence.

## Working project features

The public Sites website and same-origin HTTPS backend remain at https://hollowwake.jihnjohn10.chatgpt.site. Better Auth 1.7.10 handles own-account registration/login/passwords/cookies and the first-party device authorization grant. QR contains the public code only, not the secret device code/token. Explicit account/device approval, enforced polling, single-use expiry, server revocation and protected persistent native sign-in remain implemented. Matchmaking stays unavailable with “Matches are not available yet.”

Hollowwake Control is a separate random-address **owner-private** Site. Its address is supplied privately in the delivery message, not linked from the public website or published source identity. Use the same ChatGPT owner account to enter it. Final admin RPC requires a trusted signed-in Sites identity as well as private access; identity-less service bypass cannot administer it. The private server signs requests to the existing backend; its HMAC secret stays only in both server environments. Catalog/shop editing, project locker grants/full-locker snapshots, suspensions/restoration, device/session revocation and audit use persistent database records. See ADMIN.md for the complete supported scope and limits. No in-game cosmetics, Epic entitlements or Fortnite kicks are claimed.

## Verification

- User-reported Xbox result: the user installed version 0.2 and said linking worked. This is **user-reported companion installation/linking**, not independently observed Xbox testing or Fortnite verification. Version 0.3 needs the user's Xbox test.
- Root Node/SQLite: 11 integration tests pass. Worker/D1: 10 test groups pass, including real maintained-library auth, racing device redemption, persistence/revocation, unauthorized admin calls, signed-request replay/staleness, optimistic edit races, per-account locker isolation and suspensions. Private proxy unit test passes CSRF, HMAC signing, identity requirement and redirect rejection.
- Public website: 14 real deployed Chromium desktop/mobile scenarios pass, zero page errors. They cover registration/login, explicit device approval/ownership, polling/profile, reused-code error, saved username/reload, renewal, revocation, logout, invalid code, offline/reconnection, rate limit and unavailable recovery. Final logo-only deployment receives a separate exact-byte/visual check.
- Admin live browser testing before final identity-only hardening exercised persistent create/edit/reload/delete and audit against the real backend, plus anonymous denial and CSRF rejection. It exposed a 390px layout overflow, corrected by allowing the mobile grid/nav to shrink. Final private authorization/layout checks pass: service bypass alone and caller-forged identity are denied, all seven sections fit 390px and the editor opens/closes. Interactive owner ChatGPT sign-in was not performed by the agent. Owner must use the real login rather than a service bypass.
- Windows build/sign/install/native UI results and exact delivered package hash are retained with the release. A preliminary final-icon build's linking check returned HTTP429 and was explicitly UNVERIFIED; the later run 38789040228 passed after the existing window. The build gate now fails if native verification is not PASS. Delivered Windows run 38789040228 passed signature verification, installation, activation, account approval/polling, protected restart sign-in, native logout and browser revocation detection. Read the exact final JSON/logs rather than inferring test success from an earlier green workflow.
- No original game/third-party launcher executed. No actual Xbox attached. No hardware-controller, native visual screenshot, App/Game resource allocation or actual Fortnite lobby test was performed by the agent.
- Root, Worker and Control npm audits reported zero known vulnerabilities at inspection time. This is dependency hygiene, not a malware guarantee for the untouched game archive.

An admin QA exception accidentally displayed a private platform service credential in this conversation. The harness now redacts exceptions and final admin code rejects identity-less service administration. The credential was not written into delivered source/reports/packages. Production server HMAC/auth secrets and private signing keys were never exported.

## Exact remaining technical requirements

The original signed 6.21-CL-4526925 EXE remains x64 desktop code: 821 normal + 260 delayed imports, 28-byte static TLS template and desktop USER32/CEF/PhysX/CRT/anti-cheat assumptions. The public compatibility candidate is unchanged at commit 84bb0f71a698ea5d2733d1e02c1caa96a33b1c01; its own status still records missing CoreWindow/presentation wiring and no real game-binary tests. A credible verified runtime or authorized Fortnite/game-engine/platform/middleware source port is absent. x64, APPX packaging, a Windows compiler and Dev Home Game mode do not supply it.

After launch/render compatibility is established, a separately validated client adapter must implement this build's identity/token, MCP QueryProfile/ClientQuestLogin, profile revisions/equipment, cloud storage, catalog/keychain and whatever presence/party responses the actual client enforces. Existing project accounts/inventory records are not that protocol. No compatible original lobby adapter or match server has been completed. No original loading scene, character renderer, locker menus or controller navigation inside Fortnite can currently be delivered or claimed.

Recovery/verification email remains blocked: no authorized mail-service account/sending domain is configured. The UI/API explicitly reports email delivery unavailable. All compilation is hosted; no user computer or manual compilation is required. See XBOX.md for installation, optional documented App→Game steps and certificate-change/update limits.


## Delivered package identity

`Hollowwake_0.3.0.0_x64.appx`, 1,274,170 bytes, SHA-256 `9e0614dd9b98b96200af21bc0faf188a490497e7c122a80eaa3d76e63875f625`. Windows signature verification reports zero errors. The package includes no Fortnite executable or lobby. The release ZIP includes all three x64 dependencies and its matching public CER. Eighteen native project files match the exact hosted source commit `c998b5ccaa41e4713c2224849fe4e55c506fae78`. The dev certificate is valid from 2026-10-15 to 2027-10-15; no private key is exported. Native own-account UI tests passed in run 38789040228.
