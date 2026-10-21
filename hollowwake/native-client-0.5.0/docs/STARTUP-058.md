Hollowwake 0.5.8 — integrated client/storage investigation

This is the real-client experimental launch path, not a proven working lobby.
It retains the modified 6.21-CL-4526925 client and account-bound launch flow from
0.5.7. The signed package contains executable code, compatibility providers and
Hollowwake endpoint changes. Original pinned assets download separately; they
are not substituted for the modified program. No third-party launcher runs.

Changes:
- Handle HTTP 200 from a source which ignores ranges: stream the pinned original
  ZIP forward, extract only selected data, verify every file and the full archive
  SHA-256. No full ZIP is stored. A named Hollowwake user agent identifies requests.
  Hosted probes reproduced HTTP 200 where the old installer requires HTTP 206.
- Require an independent Windows Storage API open/size/read check for every
  selected data file after streamed SHA-256 verification, before marking ready.
- Keep the progress display pending until that second API agrees. Fail closed
  and remove the readiness marker if it disagrees; preserve existing files.
- Full log starts with the running app's exact identity, LocalState path and an
  independent PAK directory listing. Actual storage locations stay local; they
  are not uploaded as raw text.
- Record native crash module basename and instruction RVA, when resolvable,
  alongside exception code. Local raw logs also describe read/write/execute and
  fault target. The OS still receives the exception; no fake success/catch-and-
  continue behavior is added.
- Correct recognition of the existing entry-call-attempt marker in sanitized
  uploads. A call-attempt marker does not prove guest instructions ran.

Reason for these checks:
The user's Device Portal screenshot for the correct 0.5.7 package shows an empty
LocalState under Q:\Users\DefaultAccount. Its uploaded report claimed 139 files,
26,449,827,840 bytes, and no new transfer. These observations disagree. An
alternate current-user storage location is possible, not established. The new
runtime path and independent API checks are designed to distinguish that case
from incorrect readiness reporting. No storage repair is claimed before results.

The previous actual Xbox attempt reached module loading and the entry-call
announcement, then recorded 0xC0000005 at an ASLR address. The old report lacks
the module/RVA needed to identify the failing operation. 0.5.8 is not claimed to
fix that unidentified access violation or reach the original loading screen.

Xbox check, only after installing this integrated package:
1. Update the experimental app in place; preserve companion and existing data.
2. Restore/link the same Hollowwake account; let content verification run.
3. Open Show launch log -> Full log. Its first page gives the actual LocalState
   path. Compare that location with Device Portal's DefaultAccount view.
4. Missing data must cause a measured transfer (about 12.36 GB with ranges or 12.53 GB sequentially,
   26.45 GB expanded), or a clear storage/network error. No manifest-only ready.
5. After both checks succeed, Launch attempts the packaged real client once.
   If it closes, reopen while signed in to upload the crash report automatically.
   Module/RVA identifies the next startup failure. Keep full logs locally.
6. Only original game-rendered output establishes loading-screen/lobby progress.
   No matchmaking servers are provided. Matches are not available yet.

No game was executed in hosted checks. Owned Windows/UWP checks must be listed
separately from physical Xbox observations. Do not describe this as a finished port.

Sequential-source retry limitation: verified files remain installed, but a source
without byte ranges must resend earlier compressed bytes before the next missing
file. Received-byte progress includes those bytes; it does not claim they were
newly installed. Wrong/truncated streams never mark the installation ready.
