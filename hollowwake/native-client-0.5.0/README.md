# Hollowwake native client 0.5.0 experiment

Separate package identity: Hollowwake.NativeClientExperiment. The working companion and immutable rollback releases are preserved. This package routes its actual launch button to HWAttemptPackagedClient; it no longer uses the LocalState manual mapper. It has NOT reached Fortnite loading or its lobby.

Native checks report exact unresolved imports across the selected dependency graph to LocalState/native-imports-unresolved.txt and native-launch.log. The launch screen can show logs without a cursor. Original client code is not executed when import preflight fails. CoreWindow resize/activation and keyboard events feed the native bridge. Selected buttons remain orange; existing background and account linking are retained.

The native socket bridge performs actual OS socket and DNS operations for a single configured backend hostname/port. Other destinations, UDP sockets, raw extension-function lookup and unimplemented alternate socket paths are rejected. It does not redirect libcurl HTTPS URLs, bypass certificate validation or authenticate Fortnite. Existing project tokens remain in the companion; none is passed into client code.

Game DLLs are NOT included in the public test artifact. A LocalState import cannot supply executable dependencies for this native loader. The private packaging plan/workflow must prepare converted, import-redirected game/dependency images inside the installed APPX. Do not install a dummy framework or assume a renamed EXE works. Client rendering, complete USER32 behavior, XMPP and HTTP/login integration remain incomplete.

See docs/BUILD.md, INSTALL.md, LIMITATIONS.md and TEST-REPORT.md. The existing supplied code inventory is statically prepared as 26 images plus 11 original import proxies. A native installed-code workflow and code/data file routing are implemented; private hosted packaging has not yet completed. Do not mistake the public original-code preflight APPX for a game-containing build.

Split content delivery is now wired into the launch action after successful runtime preflight. See ../content-delivery/README.md. Private signed game-containing packaging passed in run 38965324618; no original client execution was established, and no new diagnostic-only download is released.
