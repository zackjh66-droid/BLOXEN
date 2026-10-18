# Hollowwake window/resource startup continuation

This is source work toward the original 6.21-CL-4526925 client. It is unfinished and is not an installable working Fortnite port.

The original game entry point has not been reached. No game code ran after it. HWPreflightClient still refuses the native packaged route with HRESULT 0x8007007F (ERROR_PROC_NOT_FOUND); this is the first observed failure, not a post-entry crash.

## Completed implementation

* Actual Microsoft USER32 CreateIconIndirect, DestroyCursor, DestroyIcon, DrawStateW, GetIconInfo and LoadIconW exports, using genuine native resource/DC/module handles.
* LoadImageW and LoadCursorFromFileW use the existing Hollowwake game-data path mapper. Outside paths and executable aliases are rejected before decoding. Windows errors are preserved.
* Actual OS DisableProcessWindowsGhosting process setting. This is not an empty substitute; the test verifies the native address and calls it in the owned test process. No deliberately hung-window behavior test was performed.
* Class registration retains borrowed icon, small-icon, cursor and background brush handles. The default window procedure implements separate per-window WM_SETICON/WM_GETICON slots and returns the prior handle. This does not claim shell icon presentation on Xbox.

Custom cursor creation/decoding is tested; presenting arbitrary native custom cursors through CoreWindow remains unsupported. GetDC/BeginPaint are not redirected to Windows with the virtual HWND.

## Build and evidence

Use the existing GitHub Actions workflow on branch hollowwake/native-system-route. Build.ps1 builds the production runtime and C#/XAML project, signs the own-code package in the hosted Windows runner, verifies its signature, runs owned functional fixtures and the Windows AppContainer import inventory. No game DLL is executed in that runner. The own-code package is not published as a requested game release while preflight fails.

The source overlay ZIP contains changed own source, workflow/build instructions and retained test evidence. Apply it over the previous source bundle, or use the current repository branch directly. It includes no game files, certificate private keys, credentials or account data. The installed/private-code packaging and separately verified content paths are preserved. Working companion, website/backend, accounts and rollback releases were not deployed or replaced.

## Exact remaining implementation

The retained import inventory names all missing functions and their importers. The window paint/DC group needs an actual GDI-compatible drawable surface and presentation/invalidation behavior; the existing virtual HWND cannot be passed to native GetDC (the measured Windows error is 1400). Clipboard, dialogs, input injection/clipping/hotkeys, window regions/effects and LDAP remain incomplete. CEF/Vivox and other dependency imports must also be implemented or proved supported without substituting successful dummy returns.

The existing restricted native Winsock bridge is not the Fortnite authentication integration. Original client endpoint/configuration and TLS routing plus a version-compatible account exchange/profile/catalog/presence path are still required. Hollowwake refresh tokens remain outside guest code. No production Epic login or credentials are used. No lobby, locker, shop or matchmaking result is claimed.

There is no evidence that a few binary edits suffice or that this route is impossible. The compatibility implementation is incomplete. A licensed, complete compatibility runtime or legally available game source with appropriate platform support would be an alternative; neither has been established here.
