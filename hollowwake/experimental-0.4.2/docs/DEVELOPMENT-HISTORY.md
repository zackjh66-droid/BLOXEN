# Focused failures and corrections

These were hosted Windows development results, not Xbox tests or Fortnite launches.

- The first signed build placed native DLLs in a subdirectory that the C# P/Invoke path did not resolve. Native runtime and packaged TLS donors were moved to the package root. Subsequent installed-package activation reached the actual memory policy fixture.
- The original fail-fast fixture terminated during application activation before the activation API returned a usable process ID. A one-shot LocalState test trigger and explicit log/process-exit assertions replaced the unreliable activation-argument path. The successful test requires the actual `CALLED_UNSUPPORTED HOLLOWWAKE_ORIGINAL_FIXTURE!MissingFunction` log; it does not accept an arbitrary crash.
- Builds 38810612674 and 38811796792 hit `Start-Process: Cannot process request because process (...) has exited` in the deliberately fast-failing modified-hash fixture. The fixture was changed to use System.Diagnostics.Process with captured output and a required exit code of 3. The modified file is restored in finally. This fixes the harness race without relaxing hash rejection.
- Byte-identical copies of a TLS donor were replaced by separately linked modules with distinct PE export-directory names/hashes. An installed-package test now loads two donors and checks both module handles and OS-assigned TLS indices differ. This is distinct from desktop fixture TLS tests.
- Interrupted native startup could prevent access to logs on every restart. The new pending marker skips native initialization after an interrupted startup and preserves the managed log viewer. The hosted test simulates the marker and requires the rescue path, then restores normal startup.
- The first hosted Python range request to the archive received HTTP 403. Chromium and a request explicitly identifying `Hollowwake/0.4` returned the correct HTTP 206 range. The importers now send that user agent. The verified range decompressed to the pinned file hash; all 131 offsets/sizes/hashes were separately checked against the existing immutable ZIP. Xbox networking/import remains unverified.

Successful run 38812239400 verified the TLS/startup fixes and full native account regression. The release run linked in WindowsVerification/verification-summary.json repeats the checks after adding the explicit importer user agent. Neither workflow executes downloaded game code. No game launch screenshot or physical Xbox success is claimed.

## 0.4.1 — user-reported Xbox failure

The supplied Xbox photos show thirteen `api-ms-win-crt-*` modules rejected with `dependency cannot relocate`. The original 0.4.0 loader required relocation information for every image moved off ImageBase. Those thirteen modules are export-only forwarders: no entry point, executable sections, imports, TLS or unwind data. All their exports forward to ucrtbase.

0.4.1 validates those constraints and all export/name/ordinal/forward-string bounds before treating a shim as immutable metadata. It does not relocate, execute or initialize that shim. Forwarded symbols still resolve through the existing allowlisted target DLL implementation. Fixed executable images without relocation support remain rejected. The shared metadata predicate and actual production dependency resolver are exercised by original compiler-built fixtures, including two forwarder images with the same unavailable preferred base and an executable negative fixture.

The log viewer now puts named failures from the latest attempt first, and allocation/protection/load failures include Windows error numbers where supported. Earlier `UNIMPLEMENTED` import records still describe real gaps; a forwarder fix is not a complete Windows runtime or lobby.

The first 0.4.1 hosted run (38857148476) compiled the production UWP runtime, then failed to compile the separate resolver fixture harness because Windows' `min` macro expanded `std::min`. Adding `/DNOMINMAX /DWIN32_LEAN_AND_MEAN` to the harness compiler flags corrected that build configuration; no assertion or relocation guard was removed. Run 38857287671 passed the focused fixture and installed-package regressions.
