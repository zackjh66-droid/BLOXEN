# Hosted native-route builds

Use the existing `zackjh66-droid/BLOXEN` branch `hollowwake/native-system-route` and `.github/workflows/hollowwake-native-client-050.yml`. The authorized standard Windows 2022 runner uses VS2022 v143 UWP C++, public Windows SDK 10.0.19041.0, .NET Native 2.2 and the declared Microsoft UWP NuGet package. No user PC compilation, proprietary console SDK or paid VM is required.

The owned public checks compile with:

```powershell
powershell -NoProfile -File hollowwake/native-client-0.5.0/Build.ps1 -OutputDirectory artifacts/native-client-050
```

That public package contains owned fixtures/runtime only; it is not the requested game delivery. Checks run original code exercising production timer cancellation/lifetimes, annotation dispatch, network operations, executable identity, file routing, downloaded-byte verification and UWP layout/import audits. Game code is not executed on the hosted runner. Development keys are nonexportable and remain on the ephemeral runner; only the public certificate is exported. Signing timestamps/keys prevent byte-identical reproducibility.

## Reviewed providers

Commit-message tags `[audio-uwp]` and `[mesa-uwp]` request the corresponding hosted provider builds. `[providers-only]` compiles the runtime/providers without rebuilding the owned app; that mode cannot establish packaged audio/WGL behavior. Normal full checks plus `[audio-uwp]` or `[mesa-uwp]` run the corresponding functional package check.

`audio/prepare_audio.py` pins DSOAL/OpenAL source archives and retains their licenses/corresponding source. The DLLs target WindowsStore, use UWP CRT dependencies and real packaged loading. `audio/Test-AudioPackage.ps1` repackages only the owned app, supplies all its x64 frameworks, and tests real PCM mixing, endpoint operations and module dispatch. Hardware endpoint failures are recorded separately from loopback mixing. No audible-output or Xbox claim follows from compilation.

`mesa/prepare_mesa.py` pins public Mesa UWP, official DirectX headers and the portable WinFlexBison tools. Only the public CoreWindow/D3D12 route is selected; the optional GPL swap allocator and GDK/XDK branches are disabled. Mesa's original placeholder GDI operations are replaced by the validated CoreWindow contract. Compilation, WGL/native-handle integration, shader validation, presentation and Xbox behavior remain separate requirements.

Each provider build emits `provider-imports.json`: actual DLL hashes plus every normal/delayed import/export. These **additional** dependencies do not disappear into the original 838-pair client inventory. A green workflow step with `continue-on-error` is not sufficient: inspect receipts, `workflow-outcomes.json`, functional reports and final failure gates.

## Private client preparation and signing

Preserve the original archive and all rollback outputs. Work on a new staging copy. The main image must match SHA-256 `9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478`; do not execute supplied launchers or scripts.

```powershell
python private-package/prepare.py <verified-inputs>/6.21 <new-private-prepared-dir>
powershell -NoProfile -File Build.ps1 -PrivatePreparedDirectory <new-private-prepared-dir> -PrivateAudioDirectory <reviewed-built-audio-dir> -OutputDirectory <private-output-dir>
```

Current preparation selects 26 game images and 18 generated import proxies. With the runtime and two audio implementations, the signed code index contains 47 files. Passing the separate Mesa behavior check permits `-PrivateMesaDirectory <built-mesa-dir>`, adds its three compiled DLLs, the exact unmodified Microsoft DXIL redistributable, and licensed source/notices, and creates a 51-file `audio-mesa` index. The shader/context/readback/presentation receipt and all provider hashes must match; a missing or failed receipt prevents inclusion. Both variants retain the original client inventory and audit additional provider imports separately. Private packaging refuses absent or receipt-hash-mismatched audio DLLs and audits all installed code hashes. The signed package also contains the inventory of actual licensed-provider imports. These additional pairs are audited separately before native client loading; they do not replace or shrink either original inventory. A converted EXE/export/manifest is not proof that its entry can execute correctly.

Game images are kept in the installed signed package. Downloaded data lives under this app's `LocalState/GameClean/6.21`, with original logical paths translated by the file-access bridge. The separate indexed download installs 26.45 GB of data from approximately 12.36 GB of selected compressed entries. It requires room for missing expanded files plus a 1 GiB reserve and reports actual I/O failures. Actual Xbox quotas/free-space behavior remains unverified.

Private game-containing outputs must remain private. Hosted delivery encrypts them for the established recipient; never upload plaintext game packages or signing keys to public CI artifacts. The 0.5.5 rollback is preserved. New 0.5.6 packaging is conditional on the current private build/sign/content checks completing; source implementation or public fixture success is not a delivered game package. A package is not a demonstrated Fortnite port until the original client/lobby is observed on Xbox.

The Mesa candidate also requires Microsoft's unmodified `DXIL.dll` shader
validator. `Build-Mesa.ps1` obtains the pinned official Microsoft.Direct3D.DXC
1.8.2407.12 NuGet package, verifies package/file SHA-256 and Microsoft
Authenticode, retains `LICENSE-MS.txt`, `LICENSE-LLVM.txt` and the explicit
redistributable-file list, and includes its actual imports in the additional
inventory. This particular DLL has no AppContainer image flag: the inventory
reports that fact and accepts only its exact pinned bytes, not arbitrary desktop
DLLs. Owned UWP loading, actual validator-object creation and GPU behavior must
pass. The Microsoft DLL is never patched or re-signed. Its license terms apply
to that component; no Microsoft source or private SDK is supplied.
