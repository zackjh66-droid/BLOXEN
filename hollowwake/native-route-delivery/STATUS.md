# Hollowwake native-client continuation — 2026-10-18

This is an implementation/source checkpoint, not a working Fortnite release.
The preserved working companion, website, accounts and rollback are unchanged.

## Neo comparison and adopted route

Read-only inspection of the supplied NeoXS 1.0.6 MSIX shows a UWP host
importing StartFortnite from a converted Fortnite game DLL. Its client is
10.40-CL-9380822, not Hollowwake's 6.21-CL-4526925. Its USER32 shim populates
export slots from the real operating-system USER32 provider. Neo contains no
packaged user32/gdi32/d3d11/dxgi/wldap32/winhttp/wininet/kernel32 provider.
This establishes an architectural lead, not that every Neo hook or Season 6
requirement has been reproduced. No Neo implementation code was copied.

Hollowwake retains its own conversion, dependency redirection, file namespace,
thread/startup handling and scoped networking. Its actual PackagedRoute now
uses real OS USER32 windows and native HWND D3D11/DXGI presentation. The launch
UI calls HWAttemptPackagedClient, which verifies the installed private game
and dependency hashes, invokes LoadPackagedLibrary, resolves the preserved
client-entry export, and runs it on the native client thread. Downloaded DLLs
in LocalState cannot satisfy this native route: code belongs inside the signed
installed package. Large data files remain separately imported and verified.

## Measured checks

The same 843 redirected import pairs were used throughout. Missing bindings
fell from the original 208 (70 main-image pairs) through 181/44, 172/37,
46/16, 22/0, to 0/0. A separate 838 unredirected pairs across 62 modules were
also checked: zero missing exports in the hosted Windows AppContainer.
An export's presence does not establish correct behavior or Xbox availability.

Original owned fixtures exercised real native windows/HDCs, paint and pixel
readback, OS message dispatch, native D3D11 hardware rendering and staging
readback, real HWND swapchain creation/Present, actual TCP traffic, a native
LDAP bind/search against an owned BER responder, actual WinHTTP/WinINet body
transfers, redirect refusal and endpoint restrictions. Real file-path expansion
and reads were checked against the configured content namespace. Public Windows
compilation, development signing and AppContainer checks passed in run
39094872423 before the separate private-input step began.

No original Fortnite or Neo executable was executed in these hosted tests.
Fortnite's entry point, original loading screen and lobby have NOT been reached
or verified. Windows AppContainer evidence is distinct from physical Xbox.

## Important remaining limitations

Private client input acquisition/packaging is a separate pipeline stage; its
result must be checked before claiming a game-containing package exists. The
original archive is preserved locally: SHA-256
bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5.
Original main EXE SHA-256:
9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478.

Signed content metadata describes 139 data files, expanded 26,449,827,840 bytes,
from the pinned 12,528,679,454-byte ZIP. The package-catalog fallback requires
fresh Hollowwake profile authentication and retains all content checks. Actual
archive availability/transfer on Xbox is unverified. Allow about 40 GB free.

Hollowwake device linking is not Fortnite login. Native client OAuth request
formats, endpoint routing and the account/profile adapter still need genuine
client integration and observed requests. The existing adapter prototype is
not evidence that this client can log in. No game credentials are supplied by
the launch command. Production Epic networking is denied by the configured
endpoint policy. Do not enter Epic credentials.

Characters, controller lobby menus, locker persistence and the admin-managed
shop are not confirmed. No match server exists. A package must not be described
as a finished Fortnite port on the basis of compilation or zero imports.

## Reproduction

Use the repository branch hollowwake/native-system-route and the Windows 2022
workflow .github/workflows/hollowwake-native-client-050.yml. Build.ps1 performs
the owned native/provider tests, compiles and signs the UWP package, and runs
AppContainer import/platform checks without executing the game. The private
stage accepts only hash-pinned build inputs, prepares converted code and
packages it through Build.ps1 -PrivatePreparedDirectory. Proprietary inputs
and signing private keys are excluded from this source download. Client inputs
must stay private; only recipient-encrypted private output may be uploaded.
