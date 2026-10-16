# Hollowwake native-system runtime investigation

Status: original implementation compiled and linked into the existing UWP native runtime; owned-code tests passed on hosted Windows. No APPX is delivered by this branch. Original Fortnite main entry/lobby, physical Xbox behavior, complete guest lifecycle/window semantics and native Fortnite account/network integration remain unverified or incomplete. The working companion, site/accounts/admin, game inputs and rollback branches are unchanged.

## Evidence that changed the route

User-supplied NeoXS_1.0.6.0_x64.msix was downloaded privately from https://content-cdn.neofn.dev/NeoXS_1.0.6.0_x64.msix without executing it. It contains a UWP native host importing StartFortnite from a converted client DLL, import proxy DLLs and installer/patch APIs. Embedded client metadata identifies ++Fortnite+Release-10.40-CL-9380822 / Unreal4.23, not the requested6.21. Neo binaries, assets, disassembly and source were not copied into this implementation or public repository. Its presence is architecture evidence, not proof of this code's Xbox execution. The source project remains targeted at the privately inspected6.21 input.

## Implemented change

NativeSystem.cpp resolves supported imports to actual exports in already-loaded OS modules rather than manually recreating every available core API. It reads bounded x64 loader metadata in the current process and uses the OS GetProcAddress operation, including OS-managed export forwarding. Four core/CRT provider modules plus the WinRT provider are considered. No archive DLL path enters OS loading. There is no token, capability, process identity or access-control change. OS API refusal/error semantics are retained. This is not a malware sandbox.

Legacy MSVCRT selection checks and pins a complete basic allocation/state family. When selected, every msvcrt import stays in that provider; a missing export cannot fall back to a modern host-CRT allocation/state function. Otherwise the prior explicit bridge remains the fallback. Presence of the native legacy runtime on Xbox is unverified. The hosted fixture explicitly loads the installed Windows system CRT before the check; no downloaded CRT is executed.

The actual import path is integrated in Runtime.integration.cpp and Native.integration.vcxproj: guest module-handle adapters run first, native legacy selection precedes the old CRT bridge, and native core export lookup precedes missing-import thunks. Network providers remain outside this resolver. No original client traffic to Epic is enabled. Guest module identity/window/graphics remain with the existing explicit adapters; their completeness is not implied by core export availability. Full guest thread notifications, main-image/process virtualization and old-library initialization semantics still need end-to-end validation.

## Tests actually performed

Successful run: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38936516506
Compiled commit: dd2f2d64f87ded0af0c075564339702d03fba76a

- GetStartupInfoW resolved to the real OS export, called through that pointer and matched direct SDK output with structure canaries.
- Real native thread creation, OS wait and exit-code collection executed an input-reading owned task.
- Real system MSVCRT malloc/realloc/free preserved data without crossing allocation families.
- Actual WinRT HSTRING creation/read/release through resolved functions.
- Missing exports, invalid ordinals, arbitrary DLL paths and native network lookup rejected.
- UWP API-family compilation and integration into the native UWP DLL passed with zero errors.
- Static demand extracted from133 x64 build images:765 core/legacy-CRT DLL/symbol pairs. All765 resolved on hosted Windows after adding the correct WinRT provider. This is address availability on Windows; it is NOT765 behavioral tests, a complete import inventory for all subsystems, or Xbox availability/proof of Fortnite startup. Unused/error-path imports are included.

First run passed functionality but UWP headers hid loader metadata types. Original x64 layout prefixes were added, with desktop static assertions against public Windows SDK layouts. A later availability check found21 WinRT API pairs were being queried in the wrong provider; WinRT SDK/provider binding was added and tested. Errors are retained in the private research logs. No failure was passed on to the user as an Xbox test package.

## Reproduce using hosted tools

Use the included guarded workflow.yml on a standard windows-2022 GitHub runner with the official UWP tools and SDK already used by Hollowwake. The branch https://github.com/zackjh66-droid/BLOXEN/tree/hollowwake/native-system-route includes all surrounding source. Build-Checks.ps1 compiles the owned test, runs it, checks APP-family compilation and rebuilds the integrated native DLL. It never downloads or executes a game. No larger runner or paid service is used.

For applying the standalone source bundle to an isolated copy of the0.4.7 source: place this directory beside experimental-0.4.7 and client-runtime; replace only that copy's Native/Runtime.cpp and Native/Native.vcxproj with the two *.integration files. Preserve the working release. The changes are already applied on the investigation branch. Do not publish another client package as lobby-ready based on these tests.
