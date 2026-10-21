# Hollowwake 0.6.0 — interrupted verification recovery and controller menu

This is the integrated private-client package, retaining the 6.21-CL-4526925
modified client, runtime/dependencies, private Blob transport and 139-file data
inventory. No original loading screen/lobby is established by hosted checks.

User evidence: verification reached the end, the Xbox app closed, and subsequent
opens closed too. No exception code/module was available in Xbox Crash data.
That does not identify a confirmed faulting API. Source inspection found native
activation/suspend/resume callbacks outside the readiness guard, native binding
and preflight before game-data preparation, and automatic verification on every
open without a durable interruption marker. These are fixed; do not label them
a proven explanation of the specific hardware exception.

Normal launcher startup now performs no explicit native binding/preflight. All
native window/input callbacks require ready state. Actual Launch first requires
verified data, then binds graphics and performs the existing real import checks,
network configuration, one-use game account exchange and original entry call.
No checks are bypassed and no import success values are synthesized.

Boot/verification/native-startup checkpoints survive an abrupt process exit.
Interrupted operations pause automatic verification on reopening; linking,
logs and report upload remain available. The first upgrade with existing content
is also paused. Download / Verify explicitly resumes verification/transfer.
Existing files/accounts are never removed by recovery. Managed unhandled failures
are logged with HRESULT and a fixed phase, without URLs/tokens; exceptions are
not falsely marked handled. Uploaded CONTENT_FAILED code phase values are:
1 launcher, 2 content verification/download, 3 Windows storage verification,
4 native binding, 5 native entry request. The original exception still requires
hardware evidence if no managed record is produced.

Before any old file is reused, WinRT must independently find the expected path
and size, followed by SHA-256 verification. Missing files are downloaded from
Hollowwake's private Blob using authenticated short-lived leases and real HTTP
byte counts. Received-byte evidence is checkpointed at least every five seconds
during active progress. File size/SHA-256 is checked while committing downloads,
then a final complete rehash and independent WinRT open/read/size pass run after
transfers. A ready marker alone never establishes installation. All 26.45 GB of
selected data and six PAKs/signatures remain required; no full ZIP is retained.

Xbox mouse mode is disabled in the launcher (RequiresPointerMode WhenRequested,
page RequiresPointer Never, host PointerCursor null). D-pad and left-stick
navigation move focus; A activates buttons and B pauses linking/download as
before. During real native execution, the guest owns controller navigation;
original game controller behavior still needs Xbox testing.

Install 0.6.0 in place with the three included x64 dependencies. Do not uninstall
or clear LocalState. Keep 0.5.9 and the working companion. On reopening after the
update, first use Show launch log and note the uploaded report ID/actual storage
path. Resume Download / Verify once, observe real received bytes or an explicit
HTTP/error code, and wait for final storage verification. Attempt real Launch
only afterward. If the app closes again, reopen; recovery should keep it out of
automatic verification and native initialization. Return the report ID and the
last recorded phase. These outcomes remain physical Xbox tests.
