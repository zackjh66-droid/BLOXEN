# CLIENT — Windows Player 0.205.0.61876 (version-0d46087630eb46cd)

Status: acquired, inventoried, statically inspected. **Never executed by this project** (no safe Windows validation environment; see `docs/WINDOWS-REAL-CLIENT-VALIDATION.md`).

## Identity
* Release: 0.205.0.61876, build 2015-07-23T23:33:45-07:00 per RobloxAPI/build-archive (previous build version-8559dcf342a3424a / 0.205.0.61792; next version-9e549411a6e34c4e / 0.206.0.62042 on 2015-07-29).
* `RobloxPlayerBeta.exe` SHA-256 `384a4cb38de6977899e09e59c2136619fef521dc7b4adeb34d404945120c8a44` — **verified equal** to the required value.
* PE: x86 GUI, timestamp 2015-07-23T19:45:30Z, VMProtect-style packed sections, CompanyName "ROBLOX Corporation", FileVersion `0, 205, 0, 61876`.
* Authenticode: CN=ROBLOX Corporation, VeriSign Class 3 Code Signing 2010 CA, cert valid 2012-08-17→2015-09-09, countersigned 2015-07-23T19:51:54Z. Checked: file digest, signer signature, issuer chain links. NOT checked: root trust store, revocation, countersignature validity.
* Source: `github.com/KloBraticc/2015-Client` @ 7a0742c (created 2026-04-17, no licence; states it derives from `archive.roblonium.com`, not verified directly). Inventory: `preservation/manifests/client-0.205.0.61876.json` (771 files, 130,887,128 bytes).
* Cross-reference: only `ReflectionMetadata.xml` (sha256 32078d95…c3de) independently matches build-archive. Further cross-references to try: archive.org `roblox-clients-2006-2021`, archive.roblonium.com directly.

## Static evidence from the binary (STATIC strings; behaviour UNTESTED)
* Command-line (boost program_options): `--authenticationUrl/-a`, `--authenticationTicket/-t`, `--joinScriptUrl/-j`, `-script`, `-ticket`, `-avatar`, `-build`, `-testMode`, `-rbxdev`.
* Web endpoints referenced: `/Game/PlaceLauncher.ashx?request=RequestGame|RequestGameJob`, `/Game/Visit.ashx`, `/asset/?id=`, `/universes/get-info`, `/Game/MachineConfiguration.ashx`, `/game/validate-machine`, `/game/GetAllowedExperimentalFeatures`, `/Game/GamePass/GamePassHandler.ashx?Action=HasPass`, `/Game/LuaWebService/HandleSocialRequest.ashx?method=…`, `/Analytics/Measurement.ashx`, `/Error/Dmp.ashx`, `/my/friendsonline`, `/thumbs/*`.
* Reads `AppSettings.xml` beside the exe (`BaseUrl`, default `http://www.roblox.com`) — BLOXEN's launcher points a *working copy's* BaseUrl at the loopback compat server.
* Verifies `--rbxsig%…%` signatures on scripts. **UNKNOWN** whether an unsigned join script is accepted; BLOXEN's Join.ashx is unsigned and labelled INFERRED. If the real client rejects it, this is the first blocker to resolve in the Windows gate.
* Embeds RakNet 3.x (not the 4.x reference in quarantine); string "Network protocol mismatch. Please upgrade."

## Handling
Quarantine copy is read-only reference. The launcher requires an absolute path to a *separate working copy*, re-hashes the exe on each launch, and refuses other versions.
