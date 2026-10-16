# 0.4.7 guest module loading

The physical Xbox 0.4.6 screenshot records `api-ms-win-core-libraryloader-l1-1-0.dll!LoadLibraryExW` called by the original `ucrtbase.dll`, return RVA214922 (0x3478a), during DLL_PROCESS_ATTACH. Static inspection confirms the call at RVA0x34784 uses flags0x800 (LOAD_LIBRARY_SEARCH_SYSTEM32). Its candidate-name table includes api-ms-win-core-fibers-l1-1-1, api-ms-win-core-synch-l1-2-0 and kernel32, with optional function-pointer caching. The photo does not identify which name was first requested. No downloaded code is executed in hosted checks.

Hollowwake is both its own UWP launcher/account UI and an experimental in-process compatibility runtime. It does not start an ordinary desktop EXE as a separate supported Xbox process. It maps the pinned original image and dependencies and supplies selected ABI-compatible APIs. The loader failure happens before original Fortnite lobby execution.

## Implemented

ModuleLoader.cpp adds LoadLibraryA/W/ExA/ExW; GetModuleHandleA/W/ExA/ExW; GetModuleFileNameA/W; GetProcAddress; FreeLibrary. Guest module handles never enter OS GetProcAddress/LoadLibrary. Four explicitly named public-API facades represent kernel32, kernelbase and the two traced API contracts. Their function pointers call real supported SDK functions or existing Hollowwake bridges; these are not native Windows HMODULEs and do not promise every Windows export. Undocumented or unsupported optional exports return null with ERROR_PROC_NOT_FOUND instead of a non-null fail-fast thunk. Required static imports still receive named fail-fast thunks when not implemented.

Already mapped, hash-pinned guest DLLs retain their actual image-base identity. Exports support names, ordinals and verified forwarders without mapping a new module from an optional query. From-address identity and actual private-import file paths are supported. Borrowed handles do not increment counts; load/GetModuleHandleEx acquisitions have tracked references. Runtime dependency images remain internally pinned for the lifetime of this experimental process. FreeLibrary releases an acquired reference; it does not unmap pinned code or claim complete DLL detach/unload support.

Module names are restricted to bounded ASCII basenames with an optional .dll suffix. Absolute paths, traversal, non-null hFile and unsupported flags are rejected. SEARCH_SYSTEM32 selects only the explicit API facades; no imported DLL is sent to the OS loader. SEARCH_DEFAULT_DIRS/APPLICATION_DIR select this restricted known namespace, not arbitrary Windows search paths. Unmapped or incompletely initialized modules fail with an error rather than fabricated initialization. Dynamic loading of additional hashed images and full reentrant initialization/unload semantics remain unfinished.

A/W name/filename variants, truncation, reference handling and absence/errors are implemented. Facades have no fabricated system filename: requesting one returns ERROR_NOT_SUPPORTED. The main executable is registered with its real pinned import path and mapped bounds for identity and filename queries; lookup of its own exports is not implemented. Unsupported Windows DLLs and non-ASCII module names remain outside this restricted implementation.

Additional compatible public SDK bindings cover FLS, localization, critical sections, debug query, current process/thread IDs, system time, pointer encoding, timing and supported unwind/file-handle operations. LCMapStringW, GetOEMCP and RtlCaptureContext are NOT aliased to incompatible or unavailable UWP declarations. Other unsupported imports remain explicitly unsupported.

## Focused tests

The original FixtureModules.dll imports the exact libraryloader API-set name and calls the production bridge during DLL_PROCESS_ATTACH. It dynamically obtains and calls actual FLS operations, verifies stored values and an actual FLS cleanup callback, obtains a real clock function, initializes and uses a critical section, calls a relocated original fixture export by name and ordinal, verifies a forwarded export, checks image identity/from-address resolution and filenames, exercises references and A/W aliases, and rejects missing optional functions, an invented contract, arbitrary paths, invalid handles/flags and double release. This is a real original-code fixture, not Fortnite or a fabricated success return. Actual pass/fail evidence is in TEST-REPORT.md and WindowsVerification.

## What remains

A module-lookup fix is not a finished Windows-on-UWP environment. Legacy C++ exception/CRT layout compatibility, full threading/lifecycle/input/graphics/audio/files/CEF behavior and native game authentication/HTTP/presence remain incomplete. Existing Hollowwake account linking and profile mapping are preserved; they do not establish original Fortnite login. Main/lobby/locker rendering remains unobserved. No next-boot or lobby guarantee is made.
