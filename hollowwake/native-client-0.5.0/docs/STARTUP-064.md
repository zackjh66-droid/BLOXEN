Hollowwake 0.6.4 experimental native startup integration

This source replaces the rejected 0.6.3 return-slot patch with private preparation
from normal execution of the exact original 6.21-CL-4526925 executable. It keeps
the original entry and startup calls. Original input SHA-256 remains
9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478.

The preparation verifies every recorded DIR64 relocation against the observed
original load delta, restores the loader's normal/delay import bindings and
removes only the invalidated bounded Authenticode overlay. It does not introduce
scanned-pointer relocation, guessed return targets, check/branch patches, fake
API success or an account authorization bypass. It records uncaptured BSS as a
limitation, rather than claiming the snapshot is complete process state.

The signed runtime compiles the prepared client size/SHA-256 and the measured
original image permission ranges into its launch path. After real package loading
it validates the mapped module ranges and applies those measured permissions
through actual OS operations. An unavailable/refused operation stops with its
name/range/error. Client entry is passed directly as the native OS thread start
with the original stack reservation and actual process context. Thread exit is
reported as a thread exit code; a return and ExitThread are not falsely distinguished.
The existing launch action, account ticket, data bridge, controller, graphics and
native dependency integration are retained. Matchmaking stays disabled.

Windows investigation runs 39663694739 and 39664595849 reached the original entry
and first initialization call-return in both prepared-DLL cases, then ran to the
30-second bound without a recorded access violation. The wait frames were inside
a USER32 dialog reached from original client code. This is not proof of rendering,
a loading screen, Fortnite-compatible account acceptance or a lobby. The original
source acquisition/capture/preparation and native Windows checks are separate
from UWP compilation/signature checks and physical Xbox behavior.

Xbox checklist after a signed package passes its integration checks:
- Keep the working companion installed. Update the existing experimental app;
  do not reset/uninstall it, so its existing GameClean content is retained.
- Install the exact signed x64 MSIX with its included dependencies/certificate
  through Device Portal. Do not mix older runtime/game DLLs with this package.
- Complete the existing account link and verify the selected content on disk.
- Select Launch once. Report whether an original loading screen or menu appears.
- If stopped, reopen Hollowwake to upload the preserved startup/crash report and
  provide its report ID. No hundreds of photographed log pages are needed.
- A failed measured permission transition must be reported with its Win32 error;
  do not disable checks or replace the error with success.

No Xbox success, original loading screen, lobby or shop is claimed by this document.
