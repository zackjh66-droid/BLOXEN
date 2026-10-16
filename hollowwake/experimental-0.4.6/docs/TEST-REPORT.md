# Hollowwake 0.4.6 verification

Hosted Windows run: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38924569278
Compiled commit: fe79f5103ad3b4f7a1e0b3ba7cb0d1b8dd19575b
Result: PASS. Physical Xbox 0.4.6 and original Fortnite execution: NOT TESTED.

## Passed checks

- Native x64 compilation, UWP .NET Native compilation, development signing and Windows signature verification. Signed package installed and activated on hosted Windows Server 2022.
- Combined original fixture mapped through the production dependency resolver: exact legacy decorated scalar/array allocation and deletion imports; 16 scalar constructors/destructors; 64 callback registrations and reverse execution; process exit callbacks in LIFO order; recursive locks and 8,000 synchronized updates; real critical sections, private heap growth, TLS values, timed waiting, a 24 KB stack probe and a caught structured exception through registered x64 unwind metadata.
- Earlier forward-only DLL name/ordinal resolution, fixed-base collisions, rejection of a non-relocatable executable, clock wrap, legacy allocator, initializer ordering/error short circuit, OS system information/version and TLS donor/thread checks.
- Packaged UWP memory-execution, message, clock, allocator, system-information, TLS and graphics API contracts. These are owned code tests, not Fortnite rendering.
- Real live HTTPS account linking: native code display, explicit website approval, polling, linked username, protected credential restoration after process restart, native logout/server revocation and website revocation detected by heartbeat. No credentials are included in deliverables.
- All six native menu buttons visible without scrolling in an 800x600 Windows window. Actual persisted owned-fixture fail-fast error is visible immediately in the compact diagnostic; Close is accessible. Windows screenshots are native-menu-windows.png, native-linking-windows.png and native-diagnostics-windows.png. The diagnostic screenshot is deliberately the real OWN fixture failure, not an Xbox/game launch result.
- Static APPX block-map verification: 145 blocks; all three native modules are x64/AppContainer; no missing exports in supplied VCLibs. Full details and certificate expiry are in native-dependency-check.json. Native/app/shared source identity is matched against the compiled commit.

## Failures corrected during development

Run 38923022198: fixture declaration conflicted with the SDK _onexit declaration; corrected to use SDK declaration.
Run 38923418678: modern compiler-generated array EH helpers and sized deletion were not part of the legacy ABI. Fixture now exercises raw legacy array allocation/deletion and disables sized deletion; no incompatible exception helper aliases were introduced.
Run 38923725605: fixture had no relocation directory and correctly failed the production non-relocatable-executable guard. Added an actual relocation-bearing pointer to the original fixture; retained the production guard.
Run 38923993081: combined startup tests passed, but the new diagnostic UI check constructed a PowerShell PropertyCondition with a string rather than a ControlType object. Corrected the test's constructor invocation. The final run passed.

## Evidence limits and remaining work

The user-reported Xbox 0.4.5 photo records msvcrt!??2@YAPEAX_K@Z in dbghelp.dll, return RVA 1221957. Static disassembly identifies new(16) and traces the entry point, C initializer and all 16 non-null C++ initializer roots. Their directly traced constructor imports are covered by this implementation. Bounded static analysis cannot resolve every indirect call or prove original dependency initialization completes. Remaining entry/error/cleanup imports are recorded in startup-coverage.json.

No downloaded EXE/DLL was executed in CI. The original Fortnite main entry and lobby remain unobserved. The same-account mapping design and backend prototype are not proof of native client login. Complete legacy C++ EH/CRT, module/thread/window/graphics behavior and native Fortnite authentication/transport remain unfinished. The old new-handler/bad_alloc ABI is not implemented; allocation failure deliberately produces a named fail-fast. That out-of-memory branch was not dynamically exercised in this run. Full dependency teardown/unload is incomplete. Physical Xbox controller behavior and update preservation still require user testing.

Website, accounts, private owner administration, original game archive and rollback releases were not modified. Game binaries/PAKs and private signing keys are excluded. The public certificate is included. Full archive import cannot fix missing APIs. No claim of next-boot lobby success is made.
