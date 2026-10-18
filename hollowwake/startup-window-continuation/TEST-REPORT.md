# Actual results

Code commit: 79513a560f5a739ebd1dbeaa0f2493fa8e65bf77. Hosted Windows CI run: https://github.com/zackjh66-droid/BLOXEN/actions/runs/39090460653 (success).

The native runtime and C#/XAML UWP application compiled; the own-code package was development-signed and its signature verified. No private signing key is included. The public artifact retains evidence only; no diagnostic-only APPX is offered as the requested game.

## Same-inventory comparison

* Fixed 843 redirected pairs: 181 -> 172 unresolved this iteration (original baseline 208).
* Main client: 44 -> 37 unresolved this iteration (original baseline 70).
* Unredirected 838 pairs across 62 DLLs: zero missing exports on the hosted Windows AppContainer. This is export presence, not API behavior, complete dependency compatibility or Xbox support.
* No checks or inventory entries were removed. The retained comparison records the original baseline SHA-256 and names/importers.

## Functional checks actually performed

Original owned fixtures exercised production resolver code: actual icon/cursor creation, native resource metadata, destruction and shared icon loading; DrawState icon drawing with actual pixel readback; BMP decoding through the game content path with bitmap size/pixel checks; cursor decoding and default-size hotspot scaling; rejection of paths outside the content namespace and executable files; borrowed resource retention through ordinary/extended class registration and separate per-window WM_SETICON/WM_GETICON state. The real DisableProcessWindowsGhosting address was checked and invoked in the owned process; no deliberately hung-window test was performed.

Existing native thread, TLS/stack, native factory/swapchain rendering/readback/resize, pointer/touch/text, data-file, network restriction, proxy and startup fixtures passed in this run. Their individual scope and logs are retained under evidence/final-windows-run. None executed Fortnite. Real Xbox behavior remains unverified.

Two earlier iterations failed and were fixed: run 39089908615 rejected a narrow icon-resource constant passed to a wide API; run 39090033358 found an incorrect test assumption that file cursors retain their source dimensions instead of the OS default cursor size. The production decoder was not modified to manufacture those expected values. Run 39090174273 then passed; the later window/resource integration and final run also passed.

## Actual client status

HWPreflightClient remains 0x8007007F before loading/executing the game. Original entry point reached: NO. Code after original entry: NONE. Original loading screen/lobby: NOT REACHED. First actual failure: preflight refuses unresolved bindings. Do not describe BeginPaint or any other listed import as a post-entry failure; it has not been invoked in this native route.

The first main-image missing entry in the sorted inventory is USER32.dll!BeginPaint. Full remaining main names are in REMAINING-MAIN.txt, and all dependency importers are in evidence/final-windows-run/import-comparison.json. Painting/DC, dialogs, clipboard, window effects, input and LDAP work remains. Fortnite-compatible network/login configuration is not yet connected to a running client.

Original client SHA-256 was rechecked unchanged: 9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478. Working companion, deployed services/accounts, private admin authorization, original archive and rollback releases were preserved.
