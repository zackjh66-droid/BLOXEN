# Xbox installation — native preflight experiment only

This separate identity is Hollowwake.NativeClientExperiment, displayed as Hollowwake Native Client Test. Keep the working Hollowwake companion installed. This package has no game payload and does not reach original loading or lobby. Installing it is optional; it is not another claimed lobby fix.

1. In Xbox Dev Mode, open Dev Home and enable Device Portal. Open the portal address shown on the Xbox from a browser on your own network.
2. In Device Portal's app installation page, choose the supplied Hollowwake_0.5.0.0_x64.appx. Add only the three dependencies under Dependencies/x64: Microsoft.NET.Native.Framework.2.2.appx, Microsoft.NET.Native.Runtime.2.2.appx and Microsoft.VCLibs.x64.14.00.appx. Do not select the ARM/x86 dependencies or install a CER as a dependency.
3. Install, then start Hollowwake Native Client Test. The public CER accompanies the download for signature/trust inspection; its private key is not included. Device Portal trust/install handling varies with the Xbox OS version.
4. Existing companion accounts remain on the same backend. The distinct experimental package has its own protected local session store and can require linking once. It does not delete the working app's account or data.
5. Diagnostics are paged with focusable buttons; the full native log and exact missing-import list are retained in this app's LocalState. Retrieve native-launch.log, native-imports-unresolved.txt and managed-launch.log from Device Portal's application file explorer. Do not share account tokens or the session vault.

## App or Game classification

The documented Dev Home procedure is: highlight the installed app, press View (two overlapping rectangles), open View details, choose App type: Game, then press B and relaunch. Menu wording can vary with Xbox OS. This setting changes the application resource budget. It does not implement desktop Windows APIs, change the PE ABI, fix the native loader or prove Fortnite compatibility. It has not been verified as necessary for this diagnostic package.

Public references: https://walbourn.github.io/directx-and-uwp-on-xbox-one/ and https://discussions.unity.com/t/uwp-xboxone-how-to-switch-from-app-to-game-mode/693702 . The procedure is documented by these sources; it was not physically tested during this hosted build.

## Optional test scope

Check that the separate app opens, linking/session restore still works, orange focus is visible, diagnostics Next/Previous and Close remain within the screen and logs are retrievable. A missing-import error is expected in this preflight artifact. It does not establish that importing more game files fixes compatibility. There is no request to repeatedly test it as a working lobby release.
