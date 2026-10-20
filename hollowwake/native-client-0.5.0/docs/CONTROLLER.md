# Controller integration

The real client imports XInput1_3 ordinal 2 (GetState) and ordinal 3 (SetState).
This map was checked against the preserved original DLL's export table, not
inferred from a different XInput version. HWXI13 resolves these through the
production ControllerBridge, which reads Windows.Gaming.Input.Gamepad.

The bridge supports four stable slots, all ordinary Xbox buttons, both sticks,
both analog triggers and the two XInput vibration motors. It retains canonical
device identities so collection reordering does not swap players. Removal
disconnects the slot and clears requested vibration. Reconnection occupies an
available slot; it does not manufacture a connected device. XInput packet
numbers change when the returned state changes.

Focus loss and XInputEnable(false) neutralize input and stop vibration. Focus
restoration reapplies the caller's requested output for still-connected pads.
Suspension stops output. Resumption consults the actual CoreWindow activation
mode. B/Escape cannot cancel linking while the native launch is in progress.
The XAML launcher already requests controller focus navigation rather than
requiring a pointer.

ControllerStateTest exercises production translation and lifecycle logic with
owned readings. It does not impersonate physical Xbox testing, test the WinRT
device driver, or establish that Fortnite consumes the resulting state.

Bluetooth enumeration/GATT imports belong to CEF's delay-loaded Web Bluetooth
feature. They are separate from this controller path. Microsoft documents
Bluetooth APIs as unsupported in Xbox UWP:
https://learn.microsoft.com/en-us/uwp/extension-sdks/uwp-limitations-on-xbox

After the client can execute, physical verification must cover menus, sticks,
triggers, vibration, guide/focus changes, suspend/resume and reconnect. No such
game/Xbox success is claimed by a hosted controller fixture.
