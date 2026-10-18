# Hollowwake: native startup continuation

This is implementation source and actual hosted check evidence. It is not a finished Fortnite port or an Xbox game release. No original Fortnite loading screen, character or lobby has run in these checks. The working companion, accounts, Sites deployments and rollback packages were not changed.

## Implemented in the actual native launch path

- CoreWindow touch packets, stock cursor selection and actual native keyboard-layout queries; single-window system-command state.
- Actual native process window-station queries. These expose genuine Windows objects/results, not a fabricated Xbox desktop session.
- A real composition swap-chain wrapper preserves guest window and factory relationships, forwards actual rendering/buffers/presentation, and rejects unavailable exclusive/display modes.
- The D3D11 device's native adapter/factory route now reaches the registered CoreWindow composition attachment for the virtual game window. Real host HWNDs retain native behavior. Protected data dispatch slots are changed only with VirtualProtectFromApp; executable pages are excluded. Partial installation rolls back new slots; existing devices are preserved. Xbox policy acceptance remains unknown.
- The actual HWAttemptPackagedClient entry uses a fresh OS thread and the pinned executable's 1,048,576-byte stack reservation. Native loader TLS notifications are retained. COM is left for client initialization. The launch worker waits for real return; unexpected thread exit is an error. Client exceptions continue to OS handling and existing persistent crash logging.
- The launch command requests windowed D3D11 at the real bound viewport. Acceptance by Fortnite is untested. No project credential or shared account password is added.

MainPage.xaml.cs already routes the launch button through import preflight, authenticated content download/hash verification, then HWAttemptPackagedClient. New changes are connected to that path. Checks remain in place: preflight still refuses original entry while required bindings are missing.

## Remaining exact failure

HWPreflightClient returns HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND), 0x8007007F. Against the unchanged 843 redirected pairs, missing bindings decreased from this task's 193 to 181; main-client missing pairs decreased from 55 to 44. The earlier original baseline was 208 overall/70 main. See evidence and REMAINING-MAIN.txt. Resolver availability alone does not establish behavior.

The 44 main bindings include 28 USER32 and 16 WLDAP32 ordinal imports. Main USER32 gaps cover a GDI paint/DC surface, clipboard ownership/data, dialogs/resources/icons, clipping/global input, and layered/window-region behavior. A native GetDC on the current virtual HWND failed with ERROR_INVALID_WINDOW_HANDLE (1400) in an original fixture. A genuine HDC/presentation bridge or a compatible native-window route is required; returning a fabricated handle cannot satisfy native GDI consumers. Native LDAP provider availability and restricted endpoint behavior also remain unresolved.

The fixed unredirected inventory contains 838 pairs across 62 modules. All exports resolve on hosted Windows AppContainer. Their Windows availability does not establish Xbox availability or compatible behavior. Non-main redirected dependencies remain incomplete too; they are not hidden to allow entry.

Login is a separate unfinished integration. Native sockets only permit the configured project host/port. They do not rewrite the statically linked HTTP library's URLs or TLS hostname. A cURL hook reference was inspected as untrusted source; all three checked public signatures had zero matches in the exact pinned executable. This does not prove no matching implementation exists. No speculative address, certificate-verification bypass, shared login or Epic production connection was introduced. The preserved client621 backend adapter has not been connected to an executing Fortnite client. Project account linking is not proof of Fortnite login. Matchmaking remains unavailable.

## Hosted reproduction

Use the existing repository branch hollowwake/native-system-route and workflow .github/workflows/hollowwake-native-client-050.yml (windows-2022, installed VS2022/UWP SDK 10.0.19041). All compilation in this iteration used that hosted runner; the user needs no local tools. Build.ps1 builds the native runtime, generated import proxies, C#/XAML app and development-signed x64 APPX and runs focused owned-code checks. Private keys remain non-exportable in the ephemeral runner certificate store. Public artifacts contain logs/JSON only, not that diagnostic app or proprietary inputs.

A commit marked [no-private-inputs] skips reacquiring game inputs. The private-package scripts support a separate signed package with the preserved converted/dependency images and data-only authenticated content transfer. Previous private packaging/hash audit succeeded, but this iteration did not repeat or release game-containing packaging because its native startup preflight is already known to fail. LocalState boot imports cannot satisfy executable dependencies for LoadPackagedLibrary.

No new Xbox installation is requested. A package stopped by these already-reproduced failures would not be a meaningful lobby test. Existing rollback installations remain preserved.
