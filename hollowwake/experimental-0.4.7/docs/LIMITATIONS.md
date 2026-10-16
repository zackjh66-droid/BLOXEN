# Remaining limitations

0.4.7 is unverified on Xbox. User-reported 0.4.6 stopped at original ucrtbase LoadLibraryExW, return RVA214922. Module-namespace work targets this optional API lookup sequence; it does not establish original dependency completion, main entry or lobby rendering.

LoadLibrary and GetProcAddress bridges support four documented capability facades and already mapped hash-pinned modules. Optional missing functions return null. New dynamic-image loading, arbitrary file search, full reentrant initializer/loader-lock semantics and dependency detach/unload remain unfinished. Already mapped modules stay pinned until this experimental process ends. The restricted namespace is not the whole Windows loader. See MODULE-LOADING.md for exact flags and behavior.

Old C++ exception/new-handler/RTTI and legacy FILE/CRT globals remain incompatible or unimplemented. Allocation failure is explicitly fatal; no old C++ exception ABI is faked. The stable x64 structured-exception handler is a distinct ABI. Static analysis is bounded and includes unresolved indirect calls and error/detach branches. Other dependencies can impose further requirements. Complete threads/TLS, window/input, audio/files/CEF and engine DXGI paths are missing or unverified. Required unsupported imports still fail fast with named logs.

Native Fortnite HTTP/login handoff, bootstrap/presence and validated assets remain unfinished. The client621 backend prototype remains disabled in production. Project accounts share intended authoritative profile/admin data, but website linking does not log the original Fortnite client in.

Original Fortnite loading/lobby, characters, locker and game menus are not available yet. No match servers. Full game content cannot repair missing APIs. The native menu fits without scrolling; detailed logs can scroll. Physical Xbox controller behavior must be tested by the user. Windows captures and owned-fixture tests do not demonstrate a working game port.

Existing site/accounts/private admin/data and rollback packages are preserved. Recovery email is unavailable. The original game archive is unchanged; no downloaded game code is executed in hosted CI. This runtime is not a malware-analysis sandbox.
