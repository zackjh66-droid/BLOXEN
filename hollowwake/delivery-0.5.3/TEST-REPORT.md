# Hollowwake 0.5.3 test and delivery report — 2026-10-19

Package code commit: 5253a766e9a7b166c006d106568421d3f75772d4.
Authorized standard Windows build: https://github.com/zackjh66-droid/BLOXEN/actions/runs/39287610703 .
No paid runner or game-testing VM was started.

## Delivered package

- Hollowwake_0.5.3.0_x64.msix: 127,528,103 bytes.
- SHA-256: dee18a3953095127a7d39399838037de536d2267c5cc5b329aec82d911f44057.
- Identity: Hollowwake.NativeClientExperiment; version 0.5.3.0; x64; publisher CN=HollowwakeDevelopment.
- Includes 26 converted native images and 11 compatibility proxies. It includes the actual original-client integration, not a screenshot/recreated lobby or a companion-only payload.
- The original main image was measured as 9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478; its converted packaged copy is 3730f8c0ef1a7b4441f7f1dc5d86aa84b6a2ab5a689c50bf8e51374c4524dc27.
- Windows signtool verified the private MSIX development signature. The delivered CER matches that build's public certificate; no private key is exported.
- Recipient-encrypted hosted artifact was authenticated/decrypted privately and every prepared image was independently checked again against the package manifest. No launcher/scripts, signing keys or recipient private keys occur in the delivered package.

## Checks actually performed

Windows native and C#/XAML builds succeeded. The public owned UWP AppContainer preflight, integrated metadata parse, diagnostic dialog pagination/bounds and focused production runtime checks succeeded. This preflight never loaded Fortnite. Redirected bindings were 0 unresolved out of 843; unredirected OS-loader export presence was 0 missing out of 838 on hosted Windows. These are Windows counts, not Xbox API availability or semantic completeness.

Production downloader tests used a real local HTTP server and original 20 MiB fixture: exact 16 MiB ranges, interrupted compressed-stream recovery, actual deflation, final SHA checks, ignored-range rejection with no full-ZIP fallback, corrupt-data rejection, account authorization before transfer/commit, no credential/cookie forwarding and canceled-file cleanup. Stored empty and deflated empty members were tested. An earlier hosted attempt rejected the archive's valid empty stored placeholder; this was fixed with a specific zero-byte case while still rejecting nonempty zero-compressed members. The corrected complete workflow passed.

A real HTTPS data-only range from the preserved archive mirror returned exact 206/Content-Range and decompressed to the pinned subtitle SHA. This was not a full game download through an Xbox app. The preserved 12,528,679,454-byte original archive was rehashed locally: SHA-256 bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5. All 139 signed-index paths/extents and size records reproduced from that exact original archive. No original archive was modified or game code executed.

The complete 26.45 GB automatic download has not been tested on Xbox or end-to-end in a hosted signed UWP app. Backend/account code and Sites deployments were not changed for this update. Prior backend integration tests and user-reported Xbox companion linking are earlier evidence, not new Fortnite login tests. Direct hosted HTTP health rechecks in this session returned 403; no new deployment success or outage conclusion is inferred from that result.

## What has not been demonstrated

Fortnite's entry point was not reached or executed in the hosted checks. No subsequent Fortnite loading screen, lobby scene, character, controller menu, locker, catalog or match was demonstrated. Xbox execution of this package is unverified. The previous user Xbox report showed 42 missing unredirected bindings; the new diagnostic pipeline is intended to collect their exact names privately rather than asking for hundreds of photos. Windows counts do not resolve that Xbox evidence.

The actual client still needs its remaining Xbox platform requirements and version-compatible original login/presence/profile integration proven. Existing Hollowwake account endpoints do not establish Fortnite login. Native launch retains required import/content checks; no checks were bypassed, dummy success exports added or lobby success claimed.

## Contents and storage

The smaller installer automatically downloads missing selected data after approved account sign-in. It transfers approximately 12.36 GB and installs 26.45 GB of selected data; allow approximately 27.6 GB free app storage plus app/frameworks. The new workflow does not store the original ZIP on Xbox. Verified files persist across pauses/restarts. It resumes interrupted reads during the same process; an unfinished file restarts after app restart.

The largest six PAKs alone contain 25.33 GB. They have not been stripped without a dependency map and runtime evidence. Omitted source files are inventoried in thin-content-selection.json; that selection is not proof that every dynamic native dependency has been discovered. Original files, old website/accounts/admin and working rollback packages remain preserved.

Use AUTOMATIC-CONTENT.md and INSTALL.txt for the current flow. Older native-client-0.5.0/docs files describe historical checkpoints.
