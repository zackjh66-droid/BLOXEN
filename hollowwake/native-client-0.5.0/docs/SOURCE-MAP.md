# Source map

App/MainPage.xaml.cs: actual launch action, restored project account, native preflight/launch, CoreWindow resize/activation/keyboard, paged diagnostics.
Native/Runtime.cpp: native import audit, hash checks, installed namespace, actual LoadPackagedLibrary/GetProcAddress/entry path, crash logging.
Native/PackagedNamespace.cpp: registered installed native dependency loader and module identity boundaries.
Native/WindowBridge.cpp: single-window state, messages, focus/capture, class/window metadata, geometry and virtual viewport.
Native/NetworkBridge.cpp: actual restricted OS DNS/socket APIs; no fabricated HTTP/login.
Native/DataFiles.cpp: actual UWP FromApp file IO; non-code LocalState data and registered installed-code aliases.
Native/ControllerBridge.cpp: real WinRT gamepad readings/vibration to XInput imports.
../runtime-native-route/NativeSystem.cpp and NativeProxyDispatch.cpp: bounded native OS-provider resolution and explicit bridge dispatch.
../client-runtime/graphics/: original CoreWindow/DXGI/D3D11 bridge, incomplete client object graph.
private-package/: hash-pinned static native image/dependency preparation, generated function proxies and true DATA forwarders, private signed packaging support.
tests/: actual production window/socket/data checks, native DATA import ABI fixture and UWP preflight/UI bounds checks. No downloaded binaries execute in tests.
Build.ps1/workflow.yml: hosted Windows compile/test/sign, private-input packaging attempt, separate outcome records.

The source archive includes the shared runtime-native-route and client-runtime source trees required by relative project references. Preserve their sibling layout under hollowwake/. Original game files, node_modules, credentials, build outputs and private keys are excluded.
