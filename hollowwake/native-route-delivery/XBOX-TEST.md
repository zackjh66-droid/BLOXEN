# Hollowwake 0.5.1 native-client Xbox test

This separate experimental APPX contains the converted original Season6 client,
26 verified game/dependency images, and11 Hollowwake proxy providers. It is not
a confirmed working lobby. Keep the working companion and rollback installed.
No Epic credentials should be entered.

1. Download the private install ZIP and extract it. In Xbox Device Portal,
   open Apps > Add, select Hollowwake_0.5.1.0_x64.appx and add the three included
   Dependencies/x64 APPXs as dependencies. Install using the development trust
   procedure that worked for your earlier package. HollowwakeExperiment.cer is
   the public certificate; no signing private key is supplied.
2. Start Hollowwake.NativeClientExperiment. Link your own Hollowwake account.
   This tests companion linking only, not Fortnite's original login protocol.
3. Boot DLLs are already installed: do not import them into LocalState. Complete
   the full game-content download/import and verification. This selects139 data
   files totaling26,449,827,840 bytes from the12,528,679,454-byte pinned ZIP.
   Allow about40GB free initially. An exact archive can be supplied at
   LocalState/Original-6.21.zip through an authorized app-file import method.
   Large file upload/source availability has not been verified on Xbox. If
   content verification fails, record that error; native entry was not attempted.
4. Choose the actual client-launch action once. Record whether an original
   Fortnite loading screen appears, any exact first failure, or an app closure.
   In native-launch.log, NATIVE_LOAD precedes native dependency/TLS loading;
   NATIVE_ENTRY identifies the call into the preserved original EXE entry.
   Neither marker alone establishes a rendered loading screen or lobby.
5. After a closure, reopen the app and view launch logs. Retrieve
   LocalState/native-launch.log, managed-launch.log and
   native-imports-unresolved.txt through Device Portal's app file explorer.
   Game logs, if created, are under GameClean/6.21/FortniteGame/Saved/Logs.
   Do not share account/session secrets from other files.

If your Dev Home exposes App details > App type > Game, Game classification can
request the game resource allocation. Current menu labels/steps have not been
verified on hardware here. It does not supply missing desktop APIs or establish
compatibility. Microsoft documents resource allocations at:
https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/system-resource-allocation

Known limitations: native Xbox launch, HWND presentation/input and game login
are unverified. Fortnite-compatible authentication/profile integration remains
incomplete. Characters, locker, shop and lobby menus are not confirmed. There
are no match servers. This build must not be described as a completed port.
