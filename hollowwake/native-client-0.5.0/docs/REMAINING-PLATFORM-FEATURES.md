# Feature implementation versus import binding

The fixed original inventory remains 838 platform pairs. Additional provider
dependencies are audited separately. Do not remove a pair or count a typed
failure response as an implemented feature.

The last physical report used the earlier runtime and recorded 42 unavailable
platform pairs, 16 in the main image. It is not a measurement of this source.
Multimedia timers, network availability and D3D performance markers add five
real adapters. The separately built DSOAL/OpenAL providers implement three
DirectSound bindings and passed an owned Windows UWP audio behavior check.

The twelve desktop capability wrappers preserve actual native behavior when
an OS provider exists; otherwise they return typed failure and record the
unavailable operation. They do not implement absent IMM32, OpenGL, DWM or D3D9.

The new Mesa candidate connects six WGL imports to a real OpenGL-over-D3D12
renderer, translates native client DCs to the host CoreWindow surface and
attaches a real composition swap chain. Its owned package test must create and
unbind/delete a context, verify current context/DC identity, read rendered pixels
and present. Private inclusion requires that behavior receipt and exact provider
hashes. An additional provider-import audit remains mandatory. Neither compiling
the renderer nor this Windows test establishes Xbox execution or a Fortnite lobby.

Remaining feature requirements include:

* Four main-image IMM32 operations: context creation/destruction and input method
  description/file metadata. Native delegation does not supply a missing Xbox
  IME. A replacement must maintain a consistent context/composition lifecycle
  across the other IMM operations, not fabricate isolated handles.
* Main-image DwmFlush: its desktop compositor synchronization contract is not
  equivalent to returning S_OK or arbitrarily waiting for a vblank.
* CEF Direct3DCreate9Ex: a complete supported D3D9 runtime is required if requested.
  Microsoft's D3D9On12 is loaded by the native D3D9 runtime; it is not a standalone
  replacement for the missing factory/runtime.
* Nineteen classic Bluetooth/GATT delay imports in CEF. Microsoft explicitly
  documents these APIs as unsupported on Xbox UWP. Gamepad input uses the separate
  supported Gamepad API. Import binding alone cannot enable Bluetooth hardware APIs.
* Three delayed NDF operations launch/manage desktop Windows Network Diagnostics.
  Xbox UWP does not provide that desktop service and wizard. A generic network
  availability check is not equivalent to these APIs.

All 22 Bluetooth/NDF pairs are delay-only in the 26 inspected original images.
They remain named in the audit and follow Windows' delay-load failure mechanism
on an actual request. This is not proof that they are never used. Normal missing
bindings still block loading. Checks have not been removed to lower counts.

The project-account link is also not Fortnite login. Native client authentication,
TLS/endpoint adaptation and lobby/profile/presence behavior remain unproven and
incomplete. Fortnite's entry point, original loading screen and lobby have not
been demonstrated by the hosted fixtures.

Primary references:

* https://learn.microsoft.com/en-us/uwp/extension-sdks/uwp-limitations-on-xbox
* https://learn.microsoft.com/en-us/windows/win32/api/ndfapi/nf-ndfapi-ndfexecutediagnosis
* https://github.com/microsoft/D3D9On12
