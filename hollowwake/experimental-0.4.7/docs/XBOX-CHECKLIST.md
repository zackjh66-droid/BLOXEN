# Xbox test: 0.4.7

This remains an unfinished client-loading experiment. It is not a working Fortnite lobby package.

1. Update the existing Hollowwake Client Experiment using the supplied APPX and all three Dependencies/x64 packages. Do not uninstall. Keep your existing boot files. Record any installation HRESULT.
2. Launch. Confirm your same Hollowwake username returns automatically and the six menu buttons fit. This tests launcher sign-in/layout, not Fortnite login. If session renewal is required, explicitly approve the device using your existing account.
3. Choose Attempt client boot once. If native checks were skipped after the previous crash, open Show launch log, select Enable checks and restart first. If it closes, reopen and select Show launch log. Photograph the compact summary. Select Full log and send the last MODULE/EXECUTE/CALLED_UNSUPPORTED/STOP/CRASH records, preferably as text from LocalState/native-launch.log. Do not send protected tokens or memory dumps.
4. MODULE acquired/export resolved means a compatibility module operation ran. MODULE optional export absent means a requested optional function was unavailable; returning null may permit UCRT's fallback. EXECUTE dependency lines alone do not establish main/lobby entry. A new failure after LoadLibraryExW identifies the next real blocker. There is no guarantee this build will finish original dependency initialization.
5. If an original loading screen or 3D lobby appears, capture it and record controller/menu/profile behavior separately. The Hollowwake cube background is launcher artwork and is not evidence of original rendering. Do not import the full 12.5 GB archive merely to fix another named unsupported API. The full content is necessary eventually, but missing APIs cannot be supplied by importing assets.

App-to-Game classification, if not already set: in Dev Home highlight Hollowwake Client Experiment, press View, select View details, set App type to Game, confirm A, return B and relaunch. This is documented resource classification, not an executable compatibility switch; console labels can vary. It is not proven mandatory for these initial loader checks. See INSTALL.md and its Microsoft reference.

The production website/accounts and working companion are unchanged. No matches are available. Physical Xbox behavior of 0.4.7 remains unverified until the user reports results.
