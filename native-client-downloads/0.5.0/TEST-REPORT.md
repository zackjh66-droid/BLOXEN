# Native-client integration report — 2026-10-16

## Scope

Actual source changes and hosted Windows compilation/UWP preflight. No game/Neo binary was executed. No original loading screen/lobby or physical Xbox success is claimed. Earlier companion linking is recorded as user-reported Xbox success, separate from the native-client route.

## Implemented

- Separate Hollowwake.NativeClientExperiment package identity; working companion/website/backend/accounts and rollback not changed.
- Actual UI Launch_Click calls HWAttemptPackagedClient. It no longer calls the LocalState manual mapper. Linking restores a protected project session; focus goes to the native launch action, not the disabled legacy importer.
- Native installed-image/dependency registration, verified manifest hashes, LoadPackagedLibrary, original-entry export lookup and crash logging. OS-native loading owns TLS/DLL attach/unwind; this is not a manual success stub.
- Single-CoreWindow window/message/state bridge: 75 exact bindings, including 52 of the main image's 104 USER32 symbols. Fullscreen geometry, class/window data, focus/capture, activation, lifecycle, queue, monitor and text behavior is implemented. Unsupported behavior remains missing or returns a real error. A full thread queue now reports failure rather than success.
- Restricted real Winsock DNS/connect/data APIs and ordinal mapping; no fabricated login/network success or TLS validation bypass.
- Real FromApp file-I/O bridge between logical game paths and own LocalState data, with registered installed code aliases and executable-write/traversal denial.
- Private static preparation: 26 client/dependency images, 11 generated original proxies, original hashes preserved, explicit duplicate selection policy. The legacy MSVCRT _iob DATA import uses a true native PE forwarder. Assembly/C++ object-name collisions in proxy compilation were fixed.
- Diagnostics: exact DLL/symbol names, persistent logs, cursor-free pagination and orange focus. Hosted Windows UIA bounds/page-change check passed.

## Measured unresolved requirements

Among 843 redirected DLL/symbol pairs, the Windows AppContainer audit reports 208 missing: USER32 160, WINHTTP 15, WININET 9, WLDAP32 16, WS2_32 8. Seventy pairs are imported by the exact main EXE. `unresolved-imports-by-module.json` lists every name and importer. This is an explicit gate before client entry, not a claim that the executable was launched.

There are also 838 unredirected symbol pairs in 62 external OS libraries, inventoried in `unproxied-platform-imports.json`. They are not covered by the 843-symbol resolver test and have not been shown available in Xbox UWP. The graph's 73 external dependency names must not be treated as resolved platform support.

The original EXE imports `_get_narrow_winmain_command_line` from api-ms-win-crt-runtime-l1-1-0.dll. Our GetCommandLine wrapper does not prove that its conventional CRT startup receives the guest command line. Guest CRT state and main-image process assumptions need validation without corrupting the UWP host's CRT globals.

The exact 6.21 binary contains '-HTTP=WinInet is no longer valid'. No legacy WinInet switch is enabled. A verified libcurl/MCP endpoint binding and isolated short-lived /client621 grant handoff are still absent. Existing project profile/account-linking success is not Fortnite login evidence. The existing backend account/profile/locker/catalog prototype has not been validated against running client traffic or XMPP.

## Completed hosted checks

Run 38960305628, commit 5087e75690ec31602257eb66d021d6c068a1932c: native/managed x64 compile, .NET Native APPX build, development signature verification, real production window/socket/data behavior, UWP AppContainer import preflight and diagnostic UIA pagination/bounds passed. Game code not executed.

Run 38962983038, commit 7fefbcfe95c869917771993ebe4133ba98ebcf00: added original native DATA import fixture passed: same provider address, actual writes and function reads. Public APPX compile/sign/preflight passed. Private packaging failed despite overall workflow success; workflow-outcomes.json records it separately.

Focused proxy checks compile the actual 11 generated proxy specifications, inspect x64/AppContainer PE flags, match exact named/ordinal exports and verify the DATA forwarding target. These checks do not execute the game or establish API semantics for absent imports. Final delivery logs record the latest run and result.

## Private hosted packaging failure

The primary source refused pinned ranges (HTTP 200); the full transfer observed 5,505,024,000 bytes, SHA256 0b780b975b322f2bf53cd9f8a336ceb1138f89ee79f25ab23c48395fedbd8920, and was rejected as not a valid ZIP. This is not the preserved original 12,528,679,454-byte archive. The simplyblk RAR source failed verified Schannel HTTPS before providing a usable file; rebootfn returned 404. No checksum mismatch was bypassed and no TLS verification was disabled. The original local archive remains preserved.

The private packaging workflow and final content-hash audit are implemented. A completed signed APPX with game code has not been verified/delivered. The public APPX contains original project runtime/UI only. It is a preflight artifact, not a functioning Fortnite package.

## Delivery boundaries

See final-build-results.json and workflow-outcomes.json for exact source/build IDs. Build failures, including the caught proxy object overwrite, MSVC forwarded-export linking issue and ordinal-only audit-reader issue, are retained in the logs rather than described as successes. No private signing key or game/Neo payload is included in public artifacts. No live account/admin migration or service credential change was performed in this iteration.

## Final public build

Run 38964032207, commit 391281d63818a4c5d0aa2b7492d901ba6e05b538: PASS native/managed compile, all 11 original proxy DLLs with exact 843 exports, DATA address/write test, real production window/socket/data tests, signed APPX verification, UWP AppContainer preflight and paged diagnostics UIA bounds. It still finds 208 missing bindings. Private acquisition was skipped for this UI-only commit; the separately recorded private-input attempt remains a failure. No game execution or Xbox native-route test.
