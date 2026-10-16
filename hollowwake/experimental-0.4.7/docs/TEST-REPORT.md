# Hollowwake 0.4.7 verification

Current compiled commit: 917c8fb0e3286aa0fc30b1bb47d736928ff585b7
Final run: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38928368201
Result: PASS on hosted Windows Server 2022. Xbox 0.4.7 and original Fortnite execution NOT TESTED.

The physical Xbox 0.4.6 photo records libraryloader LoadLibraryExW called by original ucrtbase.dll, return RVA214922. No original Fortnite main entry or lobby execution is observed. 0.4.7 is unverified on physical Xbox.

## Focused module implementation checks

Original FixtureModules.dll is mapped/relocated through the same production dependency mapper. It imports the exact api-ms-win-core-libraryloader-l1-1-0 DLL contract and executes the new module operations during DLL_PROCESS_ATTACH. Tests cover real dynamically obtained FLS allocation, stored/read values and cleanup callback; real clock and critical-section functions; existing guest-module exports by name and ordinal; verified forwarded exports; current/main and from-address module identity; actual filenames and A/W truncation; borrowed/acquired handles and reference releases; missing optional functions returning null; invented contracts, arbitrary paths, unsupported flags, non-null hFile, invalid handles and double release rejected. No downloaded binary or game constructor is copied into this fixture.

Existing combined startup tests are retained: 16 scalar constructors/destructors, raw array allocation/delete, 64 reverse-order callbacks, process LIFO callbacks, recursive locks with 8,000 synchronized updates, private heap growth, TLS, real timed wait, 24 KB stack probing and a caught structured exception through registered unwind metadata. Forward-only DLL/name/ordinal/collision, initializer, allocator, clock wrap, real system-information/version and donor/thread checks are preserved.

Windows package checks compile native x64 UWP and .NET Native source, development-sign and verify the APPX, install/activate it, exercise native runtime contracts, real fail-fast/error recovery, all six menu buttons without scrolling, and the compact error dialog. Live HTTPS integration uses separate owned QA accounts to verify code display, explicit website approval, polling, linked username, protected restart sign-in, logout/server revocation and website revocation detected by native heartbeat. Existing users/accounts are preserved. Neither project linking nor the owned-code graphics check is Fortnite login/rendering.

## Build failures and corrections

38927065743: UWP API-family headers rejected LCMapStringW/GetOEMCP and hid desktop loader constants. Removed unavailable API aliases; represented documented guest flag ABI values as constants. This does not expose native desktop loading in UWP.
38927349193: RtlCaptureContext was also unavailable in the UWP declarations. Removed that alias; it remains unsupported, not fabricated.
38927556718: all native/module/startup/signing/UI/live-linking tests passed. Before final delivery, module filename reporting was corrected to preserve the actual selected hash-pinned path when duplicate DLL basenames exist. 38927951094 passed with that path correction. A final source review then moved ERROR_PROC_NOT_FOUND assignment after diagnostic logging so logging cannot clobber the guest result. The final run 38928368201 also passed, including a logger that deliberately changes last-error during MODULE records.

## Limits

The static UCRT trace follows bounded direct calls and conditional branches and records unresolved indirect calls; it includes error/detach branches and cannot prove success-path completeness. Module facades are restricted compatibility namespaces; they expose actual SDK functions or existing real bridges but are not native Windows module handles. Optional query failure returns null; static required unsupported calls still fail fast. Arbitrary/dynamically new modules, complete reentrant initialization and module teardown remain unsupported. Old C++ EH/RTTI/CRT layouts, full Windows thread/window/graphics/audio/file/CEF behavior and native Fortnite login/HTTP/presence remain incomplete. The client621 backend prototype stays disabled in production. Legacy out-of-memory new-handler ABI remains explicitly fatal and was not dynamically tested in this run.

No downloaded game code is executed on the hosted runner. No original loading/lobby/locker/shop or Xbox success is claimed. Main code and PAKs are privately imported on the user's console; full assets cannot repair missing APIs. The archive, live site, existing accounts, owner-only administration and rollback releases are unchanged. Public signing certificate and framework dependencies are included; private signing keys are not exported. Actual Windows screenshots are labeled native-*-windows.png; the diagnostic capture shows a real owned-fixture failure, not a Fortnite/Xbox result.

Static delivery verification: all 146 APPX block-map blocks match; three native modules are x64/AppContainer; no imports are missing from the supplied VCLibs exports. Public certificate expires 2027-10-16 16:10:37 UTC. Native/app/shared source identity matches 59 files in the compiled commit. Exact sizes and SHA256 values are in ARTIFACTS.json.
