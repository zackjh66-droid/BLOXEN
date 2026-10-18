# Native startup checks, 2026-10-18

## Scope and current result

Original client execution: **not performed**. Original loading screen/lobby: **not reached**. Xbox execution of these changes: **not performed**. The user previously reported working companion linking on Xbox; that report does not establish Fortnite execution.

No new diagnostic-only Xbox download is being represented as a game build. The native client is stopped by the retained preflight with 0x8007007F. This failure is already reproduced in hosted Windows AppContainer and does not require another Xbox installation to identify.

| Fixed inventory | Task checkpoint | Current |
| --- | ---: | ---: |
| Redirected unique pairs (unchanged denominator 843) missing | 193 | 181 |
| Of those, main-client pairs missing | 55 | 44 |
| Unredirected unique pairs (unchanged denominator 838) missing on Windows AppContainer | 0 | 0 |

The original earlier baseline was 208 overall/70 main. Checkpoint records are retained; the comparison asserts the same names and denominators rather than dropping dependencies. Raw baseline-file hashes differ between Linux and Windows CRLF checkouts; the comparison additionally records a canonical JSON digest.

## Actual behavior exercised

- Production touch packet contents/units, foreground gating, buffer-size/error handling and lifetime; cursor callback/state failures; real CoreWindow stock cursor selection in UWP.
- Actual native keyboard mapping and window-station object/flags/buffer errors. No account/session names or SIDs requested/logged.
- Production system-command enable/prior-state/reset behavior; no popup/lobby menu renderer claimed.
- Production DXGI bridge with real D3D11 WARP render-target writes and staging readback, before/after resize. Window/factory relationships and unsupported exclusive/mode requests checked.
- Native factory1/factory2 virtual-window calls routed into real composition attachment and readback; actual ordinary Win32 HWND creation/rendering kept native; dispatch pointers restored afterward.
- Virtual-window native GetDC failed with Win32 error 1400. This demonstrates the missing GDI surface bridge, not a new observed Fortnite crash.
- Actual UWP AppContainer native-factory data-page protection/change/restore operation succeeded (HRESULT 0). No executable pages or foreign processes changed. This remains Windows evidence, not Xbox policy proof.
- Production entry-thread helper exercised native TLS thread notifications, independent TLS value, actual 1 MB reserved stack and STA initialization from a new thread while caller remained explicitly MTA. Unexpected ExitThread is rejected rather than reporting success.
- Existing real socket loopback/data, prohibited destination/port/UDP rejection, DATA export identity/writes, exact proxy exports, code/data file routing, content hashes/path rejection/cancellation and per-image WinMain IAT binding checks passed.
- Native runtime and C#/XAML x64 app compiled; development APPX signature verified. Public CI artifact contains logs/JSON only. The own-code APPX is not delivered as Fortnite.

## Failures retained and corrected

Early compilation failed because an SDK ANSI cursor macro reached a wide signature; replaced with explicit wide resources. A stale local shared graphics copy temporarily omitted the repository's existing adapter-unwrapping function; restored the existing implementation and added graphics paths to workflow triggers. A fixture incorrectly assumed native swap-chain creation necessarily fails for the virtual HWND; replaced that assumption with actual attachment and GPU-data checks. A thread fixture initially rejected Windows' documented implicit MTA; corrected it to require successful STA initialization and distinguish the explicit MTA caller. All were owned-code/check failures, not Fortnite launch results.

## Hosted evidence

Repository: zackjh66-droid/BLOXEN, branch hollowwake/native-system-route. Existing windows-2022 runner, SDK 10.0.19041, VS2022. No purchased resources, new external accounts, game-testing VM, Epic production traffic or downloaded game/launcher execution.

- Run 39085084992: touch/cursor/keyboard and composition wrapper; passed, 186 missing/48 main.
- Run 39085195964: system-command state; passed, 183/46.
- Run 39085817732: scoped native factory routing and actual AppContainer policy; passed, 183/46.
- Run 39086755691: native entry thread and native window-station queries; passed, 181/44.
- Run 39087001346: transactional native factory rollback; passed.
- Run 39087085074: unexpected thread-exit rejection and unchanged checkpoint comparison; passed, 181/44.

Final exact-code run and commit are recorded in RESULTS.json. Logs and JSON are retained under evidence/. Prior working releases, Sites/account data and original archive were preserved. Private signing keys and game payloads are excluded.
