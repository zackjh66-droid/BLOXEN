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

For applying the standalone source bundle to an isolated copy of the0.4.7 source: place this directory beside experimental-0.4.7 and client-runtime; replace only that copy's Native/Runtime.cpp, Native/Native.vcxproj, Native/ModuleLoader.cpp and Native/ModuleLoader.h and Native/Dependencies.cpp with their matching *.integration files. Preserve the working release. The changes are already applied on the investigation branch. Do not publish another client package as lobby-ready based on these tests.

## Dynamic loading integration follow-up

The initial native resolver handled initial import-table binding, but the production guest module namespace still recognized only four fixed core facades. That meant UCRT's later LoadLibrary/GetProcAddress requests could fail even when NativeSystem.cpp found the corresponding OS exports. The investigation branch now also integrates ModuleLoader.integration.cpp/.h and NativeContracts.generated.h. The latter contains only the thirty public core API contract names inventoried from the exact input. A provider must be present before a native contract facade is created. Unknown contracts, arbitrary paths and network libraries are not admitted. Facades retain guest handles and route all dynamic exports through the same production callback; dynamic LoadLibrary resolution cannot leak a raw OS loader operation into the guest. Legacy allocation queries use the same family as static binding.

The focused namespace test exercises this production code with actual Windows startup information, a real thread reading its input, WinRT string allocation/release and system legacy allocation. A test fixture provides no archive dependency images: absent-dependency callbacks are scaffolding, not simulated successful loading. The test also checks nested loader resolution, missing optional exports, unknown API contracts, invalid paths, raw host handle rejection and reference underflow. These checks do not exercise Fortnite initialization, UI or Xbox behavior.

A finished lobby package still requires successful full guest initialization and main-entry execution, complete guest thread lifecycle notifications, sufficient window/input/graphics behavior, and a version-specific native client login/network handoff into the Hollowwake services. Existing project account linking cannot establish those results. No new installable client package is declared ready by this follow-up.

Follow-up verification: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38938644202 at source commit f56ee6085cf0c78e900a110fe460fbad0734d2e8. The production dynamic namespace tests passed, the static core/CRT availability check remained765/765, and the integrated UWP native DLL rebuild finished with zero errors. No Fortnite or Neo execution, Xbox test or APPX release occurred.

## Guest thread lifecycle implementation

GuestThreads.cpp/ThreadImports.cpp are integrated before the native fallback in the production import resolver. Dependencies.integration.cpp retains each real donor's InitializeGuestThread export and registers each module's validated TLS callbacks and entry point. Guest startup is serialized with dependency PROCESS_ATTACH completion; every TLS donor is seeded before the first guest thread callback. Attach uses dependency order (TLS callbacks before DllMain), detach uses reverse module order (DllMain before TLS callbacks). A module requesting DisableThreadLibraryCalls is tracked; requests for modules with static TLS fail rather than silently suppressing necessary initialization.

CreateThread forwards real OS security attributes, stack size, flags and thread ID, including CREATE_SUSPENDED. Normal return and ExitThread invoke the registry cleanup. When the single native legacy CRT family is available, _beginthread/_endthread and _beginthreadex/_endthreadex also use entry/exit adapters around that same native CRT. No archive CRT is run by the hosted tests. System threadpool, foreign callback threads, APC entry and other ways of entering guest code are NOT covered by these adapters. This is not a complete replacement for the Windows loader lock or full guest lifecycle support.

Actual verification: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38940346087 at 66d994babb9908bfacc45b71fd1193152794d68d passed suspended/resumed thread execution, serialization against in-progress initialization, TLS seed and attach/detach order, paired cleanup after return/ExitThread, native-family _beginthreadex/_endthreadex, and notification disabling. The integrated native UWP DLL compiled with zero errors. _beginthread/_endthread have source implementations but were not separately exercised. These checks ran original callbacks/tasks on hosted Windows, not game code or an Xbox.

## Native image-loader investigation

native-image/convert_image.py is an original, bounded PE32+ conversion experiment suggested by the inspected Neo architecture. It creates a separate private DLL copy with a StartHollowwakeClient export targeting the original EXE entry, preserves existing exports/forwarders, imports, TLS and unwind information, and leaves original input unchanged. It adds an original leaf DllMain that returns TRUE without starting the original EXE during loading. This permits explicit startup after loading and establishes the OS process-attach state needed for subsequent thread notifications. It is not game initialization or an API compatibility implementation.

The exact private6.21 image has nonzero data where another section header would normally be placed. The converter initially refused to overwrite it, then was extended to append export data and the leaf entry stub to the existing final RX section under strict extent, protection and overlay checks. All prior raw section bytes, RVAs, file offsets, protections and90 named exports were independently checked with pefile and direct byte comparisons. No modified game bytes are in this source bundle or public repository. The modified copy is no longer covered by its original Authenticode data; the invalidated signature directory/checksum are explicitly cleared. No authenticity/pristine-distribution claim is made.

A first owned-code fixture with a zero DLL entry loaded and ran its original code, but failed the expected thread-notification check. That failure was retained, and the converter now provides the leaf DllMain. The fixture tests actual OS loading of an ORIGINAL generated EXE, its preserved exports/forwarder, relocated pointers, static TLS on another real thread, native unwind lookup and module identity. This is a focused executable-loading test, not a graphics demo or a game launch.

Native image conversion alone does not make a Windows client compatible with Xbox. The native route also requires packaged dependency/proxy integration, sufficient UWP window/input/graphics compatibility, and the native game authentication/network handoff. Microsoft documents LoadPackagedLibrary searching the installed package dependency graph; an arbitrary DLL imported into LocalState is not thereby a packaged native module. Reference: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-loadpackagedlibrary . The current importer/manual mapper and this native-loader experiment are distinct routes. Neither is declared lobby-ready.

## Adapter/factory graphics path

CoreWindowBridge.integration.cpp/.h and GuestImports.integration.cpp replace their matching files in client-runtime/graphics on the investigation branch. Native factory adapter enumeration previously leaked raw native factory objects through adapter GetParent, bypassing the UWP swap-chain interception. Adapter/base-adapter and enumerated-output wrappers now retain the correct bridge parent identity. A private original COM interface unwraps only our adapter before the real D3D11CreateDevice call; passing a guest adapter proxy directly into D3D11 would be incorrect. Device creation is now independent of window binding; presentation still requires an actual CoreWindow or composition attachment.

This is bounded support through IDXGIFactory1, IDXGIAdapter1 and IDXGIOutput. Newer factory/adapter/output interfaces are not advertised. Complete native device/context/swap-chain parent traversal is still not wrapped. Native calls that are unsupported under AppContainer can still fail. This does not establish full UE4 renderer compatibility.

Final verification: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38942511443 at9431750ef876f16406db354fc2516a5b45c9fb5b. Actual adapter/base factory COM identity, output-parent identity (one real output present), native unwrapping and matching successful native/wrapped adapter device creation were checked. A real WARP device uploaded a buffer, copied it to staging and read back all input bytes. The native EXE/DLL fixture passed after the leaf entry fix, all prior namespace/thread tests passed, and the integrated UWP runtime built with zero errors. These were Windows API/resource checks, not a rendered game screenshot, a CoreWindow presentation test, a Fortnite launch or Xbox execution.

## Remaining measurable gaps

The current main-image inventory contains104 USER32 imports;12 match explicit hooks in WindowBridge.cpp and92 do not. Some may be unused/optional/error paths; this is NOT proof that every missing API is needed before the lobby. The native fallback deliberately does not enable the45 inventoried Winsock imports or original production service traffic. System threadpool/foreign guest entry and the full renderer COM graph remain incomplete.

A static search of this exact input confirmed libcurl and AUTH_PASSWORD/AUTH_TYPE string markers, but found no named curl_easy_setopt export or verified direct CURLOPT_URL-shaped call target. Optimization can remove these patterns; no guessed game function address was installed. Connecting a project account still requires a verified native request/login binding and an independently scoped game credential, not a project refresh token passed into an unrelated service. Existing website linking is not proof that Fortnite can authenticate.

No actual game execution, original loading screen, lobby or Xbox port success is claimed. The original downloaded archive and original EXE remain unchanged. Modified private game-image prototypes and all Neo binaries remain outside public deliverables. Working companion/site/accounts/admin and rollback releases are not changed by this branch.
