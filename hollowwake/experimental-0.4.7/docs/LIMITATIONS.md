# Remaining limitations

0.4.7 has not been tested on Xbox. User-reported0.4.5 stopped in original dbghelp at scalar operator new, return RVA1221957. New work covers the connected direct-call constructor imports and tests their behavior in an original mapped DLL; it does not establish original dependency completion, main entry or lobby rendering.

Old C++ exception/new-handler/RTTI and legacy FILE/CRT globals remain incompatible or unimplemented. Allocation failure is explicitly fatal; no old C++ exception ABI is faked. The stable x64 structured-exception handler binding is a distinct ABI. Private exit callback tables preserve caller ownership; full module unload, process/thread detach and loader-lock semantics remain incomplete.

Static trace coverage is bounded and cannot resolve every indirect call. Remaining entry/error/cleanup imports are listed in startup-coverage.json. Other129 dependency images can have additional platform requirements. Complete Windows module identity/loading, threads/TLS, window/input, audio/files/CEF and engine DXGI paths are missing or unverified. Unsupported paths fail fast with a named log.

Native Fortnite HTTP/login handoff, bootstrap/presence and validated assets remain unfinished. The prototype client621 backend remains disabled in production. Project accounts share authoritative intended profile/admin data, but website linking does not log the actual Fortnite client in.

Original Fortnite loading/lobby, characters, locker and game menus are not available yet. No matches. Full game content cannot repair missing APIs. Main menu fits without scrolling; long detailed logs still scroll. Xbox navigation/layout must be tested by the user. Windows captures and original-fixture tests do not demonstrate a working game port.

Existing site/accounts/private admin/data and rollback packages are preserved. Recovery email remains unavailable. Original game archive is unchanged; no downloaded game code is executed in hosted CI. This runtime is not a malware-analysis sandbox.
