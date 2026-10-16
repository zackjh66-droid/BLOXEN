# Hollowwake0.4.6 verification —2026-10-16

Hosted source build/sign/install/activation passed: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38919276770 . Exact compiled source f80377664814428986de55675ddb5d368828eb26 . Standard public Windows2022 runner; no paid resource, separate game-testing VM or physical Xbox was used.

Production dependency loader executed an original owned fixture DLL: GetSystemInfo called during real DLL_PROCESS_ATTACH with structure canary checks, page/address/processor fields compared against the real host. The next original dbghelp initializer was statically identified as GetVersion; its new GetVersionExW-based compatibility result matched the actual desktop legacy GetVersion in the same test process. Existing allocator/initializer/forwarder/clock and TLS fixtures passed. No archive-provided code was executed in hosted CI.

Signed UWP installation and native memory/TLS/window/graphics/clock/allocator/system-information contracts passed. Named unsupported-call fail-fast logs and managed startup recovery passed. The actual native account UI passed real HTTPS code display/approval/polling, linked username, protected automatic sign-in after process restart, logout and server-side revocation. These test accounts are separate from existing users.

All six menu buttons were simultaneously visible within the800x600 Windows test window without scrolling and each accepted focus. Geometry evidence is menu-fit.json; actual UWP screenshots are included. The menu no longer contains a ScrollViewer. Xbox cursor emulation is disabled with the documented RequiresPointerMode=WhenRequested setting. No physical Xbox controller navigation is claimed; the user must test D-pad/A/B behavior. The detailed diagnostic log can still scroll.

Six unchanged account-adapter prototype tests passed locally: own linked identities, cross-account rejection, username/account mapping, profile persistence/revisions, delegated exchange, token separation and revocation. This fixture adapter is not connected to the running game and remains disabled in production.

The latest user-reported physical Xbox result is0.4.4 original dbghelp stopping at GetSystemInfo return RVA1221867. This supports progression past the older _initterm stop, not original main entry or lobby rendering. 0.4.6 itself has NOT been tested on Xbox.

One early run was cancelled to include the statically proven next GetVersion initializer. Another run failed C2039/C2065 because the legacy GetVersion export is unavailable in UWP headers. The successful build uses supported GetVersionExW and documented packed version semantics; no arbitrary version constant or missing-API success was substituted. Logs are preserved.

Original Fortnite main entry, loading screen/lobby, character rendering, game navigation/locker and native game authentication/HTTP handoff remain unfinished. Many Windows ABI/thread/exception/file/audio/graphics paths and client backend responses are incomplete. Full archive import cannot repair API gaps. No next-boot lobby guarantee is made. Matches remain unavailable.

Website, accounts, owner-only admin, database, source archives and0.4.4 rollback are unchanged. Website download page may still list0.4.4; use the delivered0.4.6 link for this test. Your existing Hollowwake account is the intended game-profile identity. Link once; automatic launcher sign-in continues while valid. No Epic credentials or shared game login is used. Recovery email remains unavailable.
