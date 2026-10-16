# NeoXS 1.0.6 static inspection and Hollowwake progress

Exact user source: https://content-cdn.neofn.dev/NeoXS_1.0.6.0_x64.msix
Download: HTTP200, no redirects, final host content-cdn.neofn.dev.
Bytes:210415579. SHA256:9d7ead048380ffd73d15afe1c53bd03c54a4ba186a93856bff652937c2c03ef8.
Expanded bytes:381036440; no archive path/link issues or nested archives identified. All5893 internal block-map hashes match. This verifies internal consistency, not publisher trust or malware absence.

The manifest declares x64 Windows.Universal, application NeoXS/Binaries/Win64/NeoXS.exe and Microsoft.VCLibs.140.00 minimum14.0.33519.0. The user's dependency matches that identity/version/x64. The package identity is Microsoft.MicrosoftEdge.Neo but its embedded self-issued certificate subject is CN=Ender, not a Microsoft signing certificate. No cryptographic signature/trust validation is claimed. Its permissions include codeGeneration, broadFileSystemAccess, internetClient and microphone. Permissions/names alone are not malware evidence.

The package contains a real-client integration architecture: an AppContainer UWP host directly imports StartFortnite from FortniteClient-Win64-Shipping.dll, plus NeoPatch installer/manifest APIs; separate UWKRNL32/UWPU32/other DLLs expose compatibility tables. Static code in the proxies walks loaded-module metadata and resolves/overrides function slots. Export presence is not proof of implemented behavior; fallback entries exist. No proxy DLL or original client was executed here.

The embedded game identifies itself as10.40-CL-9380822, Unreal4.23.0. It is not the original Season6 input6.21 or a Season7 version. Company/version strings are evidence of claims, not cryptographic authenticity. No original game hashes were matched to Epic references in this follow-up. No full PAK content is bundled; a manifest/install component is present.

A complete bounded export-table traversal is necessary: the generic PE parser stopped before the end of NeoPatch's12249 named exports. The full traversal confirms the required NeoPatch_* APIs are present and no packaged normal-import/export mismatch was identified. The incomplete parser output was corrected before reporting an alleged package failure. Signature and Xbox execution checks remain separate and unperformed.

This materially improves the technical lead beyond a generic public runner. It does not establish that this package reached a lobby on the user's console; only installation has been described clearly. No third-party DLLs, account tokens or proprietary patch code were copied into Hollowwake or redistributed.

Hollowwake now has an original, separately compiled native-system resolver and integrated import path. Hosted tests verify real OS startup-info/thread/CRT/string operations. All765 core/CRT import pairs inventoried from133 inspected x64 Season6 build files resolve on hosted Windows. This is not a full-client or Xbox test. Actual game startup, guest lifecycle/window/graphics semantics and native backend/login integration remain unfinished. Network providers are not newly enabled. No new console crash-test package has been delivered.

Completed source: /workspace/hollowwake/runtime-native-route . Successful hosted run: https://github.com/zackjh66-droid/BLOXEN/actions/runs/38936516506 . The site/accounts/private owner admin/original archive/working companion and rollback packages are unchanged.

No AV scanner/Authenticode validation utility was available locally; no VirusTotal upload or third-party file execution occurred. This is a static architecture inspection, not a complete malware verdict.

## Separate unsafe public-site candidate

Before the user supplied the exact CDN link, projectneofn.dev/download returned a different674242040-byte NeoLauncher-win-Setup.exe (SHA2561456106485d11600310fdd11555d94f2b534a4a7d9ed232cc3af6bd500a40363). Static listing found a RAR SFX that runs neo.vbs, which silently invokes neo.bat before neo.exe. The batch requests Defender exclusions, downloads an encoded external payload, hides a system-named directory and creates/runs a SYSTEM logon scheduled task. These are concrete high-risk downloader/persistence behaviors. Nothing was run and the external payload was not fetched. This is NOT the provided NeoXS MSIX; do not conflate the files or infer that the user's package has those commands.
