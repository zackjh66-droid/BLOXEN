# Same account and controller navigation

Create/sign into your Hollowwake website account, enter the launcher code and explicitly approve your own device. Account creation signs the browser in; device approval is still mandatory. The launcher securely stores its own session, restores it automatically on later launches and validates it with the same backend. Expired/revoked sessions return to linking. Do not enter Epic credentials.

That same Hollowwake user is intended to own the game profile and public username. The preserved client621 prototype uses a persistent user-to-client-account mapping, not a shared test identity. Its delegated exchange is single-use,60seconds and bound to the linked parent session. Parent revocation/suspension invalidates client access. Native Fortnite exchange/HTTP integration is unfinished; production adapter is disabled. Signing into the launcher does not currently log the actual Fortnite client in.

Use D-pad/directional focus and A/confirm for the six visible menu rows. B cancels pending linking. RequiresPointerMode=WhenRequested disables default Xbox cursor emulation. No menu ScrollViewer exists. TV-safe logical canvas1280x720 uniformly scales to the available screen. Actual game swapchain remains at native window size outside this host scaling. Physical Xbox behavior requires the user test.

Microsoft references: https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.application.requirespointermode ; https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getsysteminfo ; https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/nf-sysinfoapi-getversion .
