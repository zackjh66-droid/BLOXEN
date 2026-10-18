# Hollowwake runtime and homepage test report

Production source: `a08bfe6913ab947320cf81d7f41e5037e94ed876`.
Hosted Windows-2022 build: [39083159931](https://github.com/zackjh66-droid/BLOXEN/actions/runs/39083159931), job `117544139384`. Own-code compilation, x64 development signature verification, focused tests and AppContainer audits passed. Private game input acquisition/build was intentionally skipped. No new game-containing APPX was built or released this iteration.

## Exact inventory comparison

The same 843 unique redirected module/symbol pairs are retained. Before baseline SHA-256: `3c38213e7dd192a20df1361f356cccfd615652a90c831060a46754272c3cfd43`.

| Module | Before missing | After missing |
| --- | ---: | ---: |
| USER32.dll | 160 | 147 |
| WINHTTP.dll | 15 | 13 |
| WININET.dll | 9 | 9 |
| WLDAP32.dll | 16 | 16 |
| WS2_32.dll | 8 | 8 |
| **Total** | **208** | **193** |
| **Main client image subset** | **70** | **55** |

See `import-comparison.json` for exact new/remaining bindings and original importers, and `remaining-main-imports.txt` for the 55 main-image names. Count reduction means these names now bind to the implemented partial bridge or actual provider; it is not proof that every parameter/feature of that API is supported.

Implemented 15 baseline-missing names: 13 USER32 display/pointer/raw-mouse/message APIs and two read-only WinHTTP configuration queries. Additional raw-device registration enumeration is exposed, but was not a new missing pair in the fixed inventory. No success placeholder or removed check was used.

## Unredirected dependencies

All **838** fixed unredirected pairs across 62 DLL names resolved to actual OS exports in the hosted Windows AppContainer; 772 pairs have normal-import consumers, the rest are delay-only. This validates provider/export presence in that environment only. No resolved platform operation is automatically treated as behaviorally compatible. Retail Xbox may lack/reject these providers or operations; no Xbox result is claimed.

The first probe incorrectly used only `LoadPackagedLibrary` for OS modules and reported all 838 missing. This was a probe design failure, not proof of OS absence. The corrected implementation retains a loaded provider with a real loader reference or calls the actual OS loader restricted to System32. The report records the method/error for each symbol. This correction does not change the original 843-pair redirected inventory.

## Meaningful checks performed

- Original production CoreWindow state test: focus/capture, visibility/enabling, owner identity, viewport, message bounds and teardown.
- New production input/display test: actual forwarded packet contents, movement/button/wheel data, short-buffer errors, opaque-handle lifetime, foreground gating, registration rollback, host cursor callback results and error propagation; real CoreWindow text correlation with early/late character delivery, repeat invocation without duplicate text and non-BMP UTF-16 output. Fixtures call the actual production functions; they are not game execution or physical devices.
- Actual Windows read-only proxy configuration queries, exact OS addresses, allocator cleanup/error paths, and rejection of outbound HTTP APIs. No proxy settings logged.
- Existing real owned loopback socket exchange and destination/UDP rejection, native DATA forwarder writes, all 11 proxy DLL exports, actual file bridge read/write/enumeration and rejection, content extraction/hash/replacement/cancellation/auth checks, and per-image startup import rebinding.
- Own x64 APPX compilation/signature, integrated AppContainer import audit, accessible diagnostic pagination/UIA bounds.
- Offline Sites Worker build and **17/17** tests, preserving real authentication, explicit approval, polling, renewal/revocation and owner controls.
- Chromium desktop 1440×1000/mobile 390×844: real implemented homepage, no horizontal overflow, native chapter navigation, reveals/progress, reduced-motion fallback, no page errors and account-route isolation.
- Browser-driven mobile registration, login, browser session restoration, wrong-code then valid-code linking, explicit checkbox approval, native-style polling/profile read and device revocation against a disposable local Worker database.

## Failures retained and fixed

The keyboard implementation first failed MSVC C2375 because the forward declarations did not match the exported definitions. Matching the declarations fixed compilation. Failed runs `39083016462` and `39083101673` remain in repository history. The final run above passed all checks, including the actual text-queue test. A local browser script initially used an unsupported named import from the installed CommonJS Playwright package; changing it to the supported default import fixed the test harness. Neither failure was a successful game/Xbox test.

## Exact current startup blocker

`Launch_Click` still calls `HWPreflightClient` before transferring large assets or calling `HWAttemptPackagedClient`. **It returns `0x8007007F` / ERROR_PROC_NOT_FOUND for the 193 remaining redirected pairs**, despite zero missing unredirected exports on hosted Windows. No check was bypassed. For example the main image still requires BeginPaint/EndPaint/GetDC/ReleaseDC, clipboard APIs, cursor resources, MapVirtualKeyW/GetKeyboardLayout, dialog/menu/touch operations and 16 WLDAP ordinals. Dependency consumers add other USER32, HTTP/WinINet and network-provider operations. See exact-name files rather than treating all imports as interchangeable.

The minimum missing implementation for the next real loader attempt is genuine behavior for those required imported interfaces (or an independently proven client-compatible route), with their focused tests, followed by normal native dependency initialization and entry. Beyond that gate, complete D3D11/DXGI/Win32 presentation semantics, full controller/window input, per-client HTTPS endpoint/auth binding and verified 6.21 Fortnite account/MCP/presence exchanges remain unproven. Existing project account linking and server-side profile prototypes do not establish original Fortnite login, character rendering, locker persistence or lobby success.

No original game code was executed in this iteration. No original loading screen or lobby was reached. No physical Xbox testing of these changes occurred. The prior companion account-linking success is user-reported. Matchmaking remains unavailable. No screenshots portray a game lobby.

## Acquisition and deployment are separate

The preserved ZIP rehashed exactly to its recorded SHA-256; it is accessible locally and unchanged. Reboot's exact catalog and RAR destination are recorded in `reboot-destination.json`; its bytes were not downloaded or substituted. Archive acquisition does not fix runtime API gaps.

Echo's homepage was blocked by a Cloudflare 403 challenge; no exact scrolling match is claimed. The original Hollowwake homepage candidate and screenshots are local, not deployed. Sites version 10 remains live, with accounts and owner-only admin preserved. The mandatory Sites source/publishing helper remains absent; `home-scroll.patch` contains only presentation changes and the sanitized candidate includes the prior authenticated content patch. Deployment failure does not explain or resolve the native startup blocker.
