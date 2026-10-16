# Native route build (0.5.0 public preflight / 0.5.1 private inputs)

This is unfinished compatibility work, not a working Fortnite port. The actual Launch button calls HWAttemptPackagedClient. No hosted game execution or Xbox success is claimed.

The included GitHub Actions workflow uses the authorized existing public repository's standard windows-2022 runner. It needs VS2022 v143 UWP C++, Windows SDK 10.0.19041.0, x64 VsDevCmd, .NET Native 2.2 and Microsoft.NETCore.UniversalWindowsPlatform 6.2.14. The hosted image supplies the compiler/SDK; NuGet restores the declared Microsoft package. Python 3 is required for original proxy generation. pefile 2024.8.26 is used only for private static preparation, not supplied by the game.

From an x64 VS developer environment, the reproducible public command is:

    powershell -NoProfile -File hollowwake/native-client-0.5.0/Build.ps1 -OutputDirectory artifacts/native-client-050

No user PC compilation is requested: this command is already executed in the provided hosted workflow. Public output includes x64 APPX, x64 framework dependencies, public CER, signature verification and focused test logs. The self-signed private key is nonexportable and remains on the ephemeral runner; it is not delivered. Builds are reproducible in procedure, not byte-identical: certificates, package timestamps and signatures change.

## Private native dependency preparation

The native OS loader cannot execute a DLL imported into LocalState. Code must be in the installed signed package. Data may be in this app's LocalState/GameClean/6.21; original code-file aliases resolve only to registered installed images.

1. Preserve the original game ZIP. Read and hash selected inputs into a separate private directory. Never execute archive launchers, scripts or DLLs during preparation.
2. `private-package/fetch_inputs.py App/boot-inputs.json <private-input-dir>` attempts exact pinned range requests, then a full safe ZIP or the documented RAR alternatives. A 206/Content-Range mismatch is rejected. Full mirror inventory, bounds, traversal/link checks and every selected file's observed SHA-256 must pass. A matching mirror does not prove Epic provenance. TLS certificate verification remains enabled.
3. `private-package/prepare.py <private-input-dir>/6.21 <new-private-prepared-dir>` closes normal/delay imports over the supplied local DLL inventory, selects differing main-adjacent copies explicitly, converts a copy of the EXE to a native DLL and redirects required platform imports to original generated proxy DLLs. It prepares 26 images and 11 proxies. Dynamic dependencies, OS API-set availability and all runtime semantics are not thereby established.
4. Invoke Build.ps1 with `-PrivatePreparedDirectory <new-private-prepared-dir> -OutputDirectory <private-output-dir>`. It compiles original proxies and packages the prepared images, manifest and runtime as version 0.5.1.0. The final audit verifies every prepared image's size/hash and proxy presence inside the signed APPX.
5. Keep private outputs private. The workflow deliberately never uploads the private game APPX to public Actions artifacts. The public download contains no game payload and cannot reach the lobby.

The most recent private hosted fetch failed; see TEST-REPORT. Private packaging is implemented but an end-to-end signed game-containing package has not been verified or delivered. Do not mistake the workflow's continue-on-error overall success for private packaging success; workflow-outcomes.json records separate outcomes.

## Native launch sequence and remaining gates

Launch checks the restored Hollowwake session through the existing project API, binds the real SwapChainPanel/CoreWindow, configures the network destination and calls HWAttemptPackagedClient. The native code audits exact imports, checks installed main/payload hashes, registers installed module aliases, calls LoadPackagedLibrary and obtains StartHollowwakeClient. Original entry is unreachable while required imports are missing. The OS route owns TLS, DLL lifecycle and unwind; the manual mapper is not used by Launch.

The CRT startup import `_get_narrow_winmain_command_line` is an additional identity concern: it uses an OS CRT API set rather than our GetCommandLine proxy. Correct guest command-line/CRT state needs an original conventional-CRT fixture and a proven isolation/override approach before claiming authentication flags reach Fortnite. No host CRT globals are overwritten.
