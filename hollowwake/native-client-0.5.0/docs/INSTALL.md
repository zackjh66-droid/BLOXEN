# Xbox installation — 0.5.6 integrated startup experiment

The private game-containing build includes the converted original 6.21 client, its selected native dependencies and Hollowwake's runtime. The public source-only CI fixture does not contain Fortnite. Do not confuse the two. Neither is a demonstrated lobby release.

1. Keep the working companion and previous package as rollback. Update only the separate `Hollowwake.NativeClientExperiment` app through Xbox Device Portal's application installation page.
2. Select the delivered x64 MSIX/APPX and the three supplied x64 dependencies: Microsoft.NET.Native.Framework 2.2, Microsoft.NET.Native.Runtime 2.2 and Microsoft.VCLibs 14. The CER is the public signing certificate, not an application dependency. No private signing key is included.
3. Sign in/link your Hollowwake account. Existing website accounts are preserved. A separate experimental app can require linking once; its protected session survives ordinary restarts until expiry/revocation.
4. Content download starts after linking or session restoration. Expect about 12.36 GB of compressed transfer and **26,449,827,840 expanded data bytes**, plus installed client code. Provision at least 27.6 GB of *app-accessible* storage. General disk free space does not establish an Xbox app's actual quota.
5. The progress panel separates received bytes from verified bytes/files. PAUSE (B) cancels safely; DOWNLOAD / VERIFY CONTENT retries or reuses independently verified files. No full ZIP is retained. Do not remove the installed app to retry: uninstalling can delete its downloaded data.
6. Boot code belongs inside the signed package. Importing boot DLLs into LocalState does not provide native package dependencies. No boot import is required for the private integrated build.
7. Once final verification passes, select LAUNCH NATIVE CLIENT once. This invokes `HWAttemptPackagedClient`, not another application or a recreated lobby. Preflight still blocks unresolved required imports. An explicit error from an unavailable optional desktop feature is not a working implementation of that feature. The new native D3D11/DXGI path attaches tagged client windows to the launcher surface; the controller bridge reads actual WinRT Gamepads. Both need physical game/Xbox verification.

## Focused test checklist

- Check that downloaded/verified data reaches 139 files and 26.45 GB on disk; report new bytes versus reused bytes. This establishes content installation only.
- Launch once. Record whether original Fortnite loading/rendering appears or the first exact error. Linking or a zero resolver count does not establish entry/loading/lobby success.
- If the app closes, reopen it while signed in. The pending attempt's sanitized diagnostics upload privately; the report ID is not an access credential. This avoids photographing hundreds of log pages.
- Retrieve raw `LocalState/native-launch.log`, `native-platform-imports.json`, `native-provider-imports.json`, `native-imports-unresolved.txt` and `managed-launch.log` through Device Portal's application file explorer when needed. Never share the session vault or account tokens.
- Any game-created logs are under `LocalState/GameClean/6.21/FortniteGame/Saved/Logs`.

## App or Game classification

Where the console offers it: highlight the installed app in Dev Home, press View (two overlapping rectangles), open View details, select App type: Game, press B and relaunch. OS wording can vary. This procedure is described in the references below; it has not been checked on physical hardware here. Game classification changes the resource budget; it does not implement desktop APIs or make an incompatible EXE work. Its necessity for this exact package remains unverified.

References: [DirectX and UWP on Xbox One](https://walbourn.github.io/directx-and-uwp-on-xbox-one/) and [Microsoft resource allocation](https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/system-resource-allocation).

Fortnite-compatible endpoint/login wiring, presence, original lobby/character rendering, locker/shop persistence and real Xbox graphics/input behavior remain incomplete or unverified. Matchmaking is disabled. Never enter Epic account credentials.
