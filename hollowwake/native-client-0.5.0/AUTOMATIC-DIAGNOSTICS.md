# Private automatic startup reports

The updated application sends structured startup failures to the existing
Hollowwake backend after linked-device sign-in. The installed older package
does not acquire this behavior from a website deployment: an app update is
required. A restored session submits the saved import audit without requiring
another Fortnite launch.

Reports contain the package/build, exact missing DLL/export pairs, numeric
native errors and bounded startup stages. They do not contain raw logs,
passwords, bearer/refresh tokens, URLs, command lines, environment variables,
game files or personal files. Full raw logs stay in the app's LocalState.

Before entering the native launch path, the app writes a completion marker.
If the process terminates, the next signed-in startup captures the incomplete
attempt. Upload failures are queued locally and retried with a current device
session. A dead process cannot upload immediately. An uploaded report receives
a receipt ID; that ID alone grants no access.

`POST /api/device/diagnostics` requires a live linked-device bearer session.
The backend validates a 256 KiB bounded schema, rejects unknown fields, limits
attempts, deduplicates reports and binds ownership server-side. Browser reads
are limited to the signed-in account. The separate Sites owner control panel
has a Client failures tab, pagination, JSON export and visible-page refresh.
Its access restriction and signed server RPC remain mandatory.

Migration `0003_yummy_quentin_quire.sql` is additive and preserves accounts.
Backend `tests/diagnostics.test.mjs` covers linked-device authorization,
deduplication, account isolation, secret/raw-text rejection, throttling and
revocation. `DiagnosticSanitizerTest.cs` exercises the actual extraction code.

An announced entry call, successful upload, zero missing exports or passing
fixture does not establish that Fortnite executed or rendered its lobby.
Reviewing reports requires an active debugging session; this is not an
autonomous service that continuously fixes builds after a conversation ends.

# Content requirement

Boot inputs support dependency/initialization checks only. The original lobby
requires its game content, shaders and other required data as well. The private
full-content pipeline pins 139 data files (26,449,827,840 expanded bytes), the
converted original client and native dependencies inside the signed package.
It verifies original archive and individual file hashes, then audits the final
MSIX. It does not silently delete assets from opaque/encrypted PAKs.

All required content is necessary but not sufficient: unsupported Xbox APIs,
graphics/window behavior and the unfinished native login/presence integration
remain separate requirements. No original loading screen or lobby has been
demonstrated by these changes.
