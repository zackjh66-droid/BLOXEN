param([string]$OutputDirectory,[string]$PrivatePreparedDirectory,[string]$PrivateContentDirectory)
$ErrorActionPreference='Stop';$source=$PSScriptRoot;$repo=Split-Path $source -Parent
if($PrivateContentDirectory-and !$PrivatePreparedDirectory){throw 'Bundled content requires the real private prepared native client'}
if(Test-Path (Join-Path $source 'App\bundled-content.json')){throw 'Use a fresh source checkout; stale bundled content marker exists'}
if(!$OutputDirectory){$OutputDirectory=Join-Path $source 'artifacts'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
python (Join-Path $source 'tests\VerifiedDownloadTest.py') 2>&1|Tee-Object (Join-Path $OutputDirectory 'verified-download-test.log')
if($LASTEXITCODE){throw 'Pinned archive resume behavior failed'}
python (Join-Path $source 'tests\BundledContentTest.py') 2>&1|Tee-Object (Join-Path $OutputDirectory 'bundled-staging-test.log')
if($LASTEXITCODE){throw 'Bundled staging/hash behavior failed'}
python (Join-Path $source 'tests\FullContentMappingTest.py') 2>&1|Tee-Object (Join-Path $OutputDirectory 'full-mapping-test.log')
if($LASTEXITCODE){throw 'Full content mapping/audit behavior failed'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$vs=& $vswhere -latest -products '*' -property installationPath;$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $source 'Native\Native.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=false /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'native-build.log')
if($LASTEXITCODE){throw 'Native client integration compilation failed'}
$native=Join-Path $source 'App';New-Item -ItemType Directory -Force $native|Out-Null
Copy-Item (Join-Path $source 'Native\bin\x64\Release\HollowwakeClientRuntime.dll') $native
& dumpbin.exe /nologo /exports (Join-Path $env:SystemRoot 'System32\wldap32.dll') 2>&1|Tee-Object (Join-Path $OutputDirectory 'windows-wldap-ordinal-reference.log')
if($LASTEXITCODE){throw 'OS ordinal reference read failed'}
& ml64.exe /nologo /c ("/Fo"+(Join-Path $OutputDirectory 'StackProbeFixture.obj')) (Join-Path $source 'tests\StackProbeFixture.asm')
if($LASTEXITCODE){throw 'Original stack-probe ABI fixture assembly failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'tests\StackProbeTest.cpp') (Join-Path $OutputDirectory 'StackProbeFixture.obj') kernel32.lib runtimeobject.lib /Fe:"$OutputDirectory\StackProbeTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'stack-probe-build.log')
if($LASTEXITCODE){throw 'Stack-probe production resolver fixture compilation failed'}
& "$OutputDirectory\StackProbeTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'stack-probe-test.log');if($LASTEXITCODE){throw 'Actual stack-probe ABI/guard-page behavior failed'}
# Original entry fixture validates actual OS thread/TLS/COM/stack behavior.
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\NativeClientThread.cpp') (Join-Path $source 'tests\NativeClientThreadTest.cpp') kernel32.lib ole32.lib /Fe:"$OutputDirectory\NativeClientThreadTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'native-thread-build.log')
if($LASTEXITCODE){throw 'Native entry-thread compilation failed'}
& "$OutputDirectory\NativeClientThreadTest.exe" (Join-Path $OutputDirectory 'native-thread-results.json') 2>&1|Tee-Object (Join-Path $OutputDirectory 'native-thread-test.log');if($LASTEXITCODE){throw 'Native entry-thread behavior failed'}
# Focused production window/network checks; no game code or repeated graphics demos.
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\WindowBridge.cpp') (Join-Path $source 'tests\WindowStateTest.cpp') kernel32.lib user32.lib /Fe:"$OutputDirectory\WindowStateTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'window-test-build.log')
if($LASTEXITCODE){throw 'Window state test build failed'}
& "$OutputDirectory\WindowStateTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'window-test.log');if($LASTEXITCODE){throw 'Window state behavior failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\WindowBridge.cpp') (Join-Path $source 'tests\WindowInputTest.cpp') kernel32.lib /Fe:"$OutputDirectory\WindowInputTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'window-input-build.log')
if($LASTEXITCODE){throw 'Production pointer/display test compilation failed'}
& "$OutputDirectory\WindowInputTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'window-input-test.log');if($LASTEXITCODE){throw 'Production pointer/display behavior failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $repo 'client-runtime\graphics\CoreWindowBridge.cpp') (Join-Path $repo 'client-runtime\graphics\WindowedSwapChain.cpp') (Join-Path $repo 'client-runtime\graphics\NativeFactoryBridge.cpp') (Join-Path $source 'tests\SwapChainContractTest.cpp') (Join-Path $source 'Native\WindowBridge.cpp') kernel32.lib user32.lib d3d11.lib dxgi.lib windowsapp.lib /Fe:"$OutputDirectory\SwapChainContractTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'swapchain-contract-build.log')
if($LASTEXITCODE){throw 'Production swapchain contract compilation failed'}
& "$OutputDirectory\SwapChainContractTest.exe" (Join-Path $OutputDirectory 'swapchain-contract-results.json') 2>&1|Tee-Object (Join-Path $OutputDirectory 'swapchain-contract-test.log');if($LASTEXITCODE){throw 'Actual swapchain behavior check failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\NetworkBridge.cpp') (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'tests\NetworkTest.cpp') kernel32.lib ws2_32.lib runtimeobject.lib /Fe:"$OutputDirectory\NetworkTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'network-test-build.log')
if($LASTEXITCODE){throw 'Network test build failed'}
& "$OutputDirectory\NetworkTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'network-test.log');if($LASTEXITCODE){throw 'Network behavior failed'}
# Actual OS LDAP parser/allocation behavior via an original loopback server.
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\LdapProvider.cpp') (Join-Path $source 'Native\NetworkBridge.cpp') (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'tests\LdapProviderTest.cpp') kernel32.lib ws2_32.lib runtimeobject.lib /Fe:"$OutputDirectory\LdapProviderTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'ldap-provider-build.log')
if($LASTEXITCODE){throw 'LDAP production adapter compilation failed'}
& "$OutputDirectory\LdapProviderTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'ldap-provider-test.log');if($LASTEXITCODE){throw 'LDAP native provider/endpoint policy behavior failed'}
# Native Web transports must pass real socket/HTTP tests, not return constants.
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\WebProvider.cpp') (Join-Path $source 'Native\NetworkBridge.cpp') (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'tests\WebProviderTest.cpp') kernel32.lib ws2_32.lib runtimeobject.lib /Fe:"$OutputDirectory\WebProviderTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'web-provider-build.log')
if($LASTEXITCODE){throw 'Native Web adapter compilation failed'}
& "$OutputDirectory\WebProviderTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'web-provider-test.log');if($LASTEXITCODE){throw 'Native Web adapter behavior failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\ReadOnlyPlatform.cpp') (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'Native\DataFiles.cpp') (Join-Path $source 'tests\ReadOnlyPlatformTest.cpp') kernel32.lib runtimeobject.lib gdi32.lib windowsapp.lib /Fe:"$OutputDirectory\ReadOnlyPlatformTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'readonly-platform-build.log')
if($LASTEXITCODE){throw 'Read-only platform query compilation failed'}
& "$OutputDirectory\ReadOnlyPlatformTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'readonly-platform-test.log');if($LASTEXITCODE){throw 'Actual OS proxy query behavior failed'}
& (Join-Path $source 'tests\Test-DataExports.ps1') -OutputDirectory $OutputDirectory 2>&1|Tee-Object (Join-Path $OutputDirectory 'data-export-test.log')
if($LASTEXITCODE){throw 'Native DATA export fixture failed'}
& (Join-Path $source 'tests\Test-ClientProxies.ps1') -OutputDirectory $OutputDirectory -RuntimeLibrary (Join-Path $source 'Native\bin\x64\Release\HollowwakeClientRuntime.lib') 2>&1|Tee-Object (Join-Path $OutputDirectory 'client-proxy-build.log')
if($LASTEXITCODE){throw 'Exact dependency proxy build failed'}
if($PrivatePreparedDirectory){
 & (Join-Path $source 'private-package\Build-Proxies.ps1') -PreparedDirectory $PrivatePreparedDirectory -RuntimeLibrary (Join-Path $source 'Native\bin\x64\Release\HollowwakeClientRuntime.lib')
 Copy-Item (Join-Path $PrivatePreparedDirectory 'GamePayload\*') (Join-Path $source 'App') -Recurse -Force
 $privateManifest=Join-Path $source 'App\Package.appxmanifest';(Get-Content $privateManifest -Raw).Replace('Version="0.5.0.0"','Version="0.5.4.0"')|Set-Content -Encoding UTF8 $privateManifest
}
if($PrivateContentDirectory){
 python (Join-Path $source 'private-package\bundle_content.py') --prepared $PrivateContentDirectory --copy-to (Join-Path $source 'App') --manifest (Join-Path $source 'App\ContentPlan621.json')
 if($LASTEXITCODE){throw 'Private bundled content preparation failed'}
}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX /D_WIN32_WINNT=0x0A00 (Join-Path $source 'Native\DataFiles.cpp') (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'tests\DataFilesTest.cpp') kernel32.lib windowsapp.lib runtimeobject.lib /Fe:"$OutputDirectory\DataFilesTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'data-test-build.log')
if($LASTEXITCODE){throw 'Data route test build failed'}
& "$OutputDirectory\DataFilesTest.exe" (Join-Path $OutputDirectory 'OwnDataFixture') 2>&1|Tee-Object (Join-Path $OutputDirectory 'data-test.log');if($LASTEXITCODE){throw 'Actual data routing failed'}
& "$OutputDirectory\DataFilesTest.exe" (Join-Path $OutputDirectory 'OwnedBundledDataFixture') bundled 2>&1|Tee-Object (Join-Path $OutputDirectory 'bundled-data-test.log');if($LASTEXITCODE){throw 'Actual bundled-data routing failed'}
$csc=Join-Path $vs 'MSBuild\Current\Bin\Roslyn\csc.exe'
& $csc /nologo /langversion:7.3 ("/out:"+(Join-Path $OutputDirectory 'DiagnosticSanitizerTest.exe')) (Join-Path $source 'App\Services\DiagnosticSanitizer.cs') (Join-Path $source 'tests\DiagnosticSanitizerTest.cs') 2>&1|Tee-Object (Join-Path $OutputDirectory 'diagnostic-sanitizer-build.log')
if($LASTEXITCODE){throw 'Diagnostic sanitizer compilation failed'}
& (Join-Path $OutputDirectory 'DiagnosticSanitizerTest.exe') 2>&1|Tee-Object (Join-Path $OutputDirectory 'diagnostic-sanitizer-test.log');if($LASTEXITCODE){throw 'Diagnostic secret/field boundary tests failed'}
& $csc /nologo /langversion:7.3 /r:System.Net.Http.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll ("/out:"+(Join-Path $OutputDirectory 'ContentTransferTest.exe')) (Join-Path $source 'App\Services\ContentTransfer.cs') (Join-Path $source 'tests\ContentTransferTest.cs') 2>&1|Tee-Object (Join-Path $OutputDirectory 'content-test-build.log')
if($LASTEXITCODE){throw 'Shared content-transfer test compilation failed'}
& (Join-Path $OutputDirectory 'ContentTransferTest.exe') 2>&1|Tee-Object (Join-Path $OutputDirectory 'content-test.log')
if($LASTEXITCODE){throw 'Shared content-transfer behavior failed'}
& (Join-Path $source 'tests\Test-StartupImport.ps1') -OutputDirectory $OutputDirectory 2>&1|Tee-Object (Join-Path $OutputDirectory 'startup-import-test.log')
if($LASTEXITCODE){throw 'Startup import test failed'}
& $csc /nologo /langversion:7.3 /r:System.Net.Http.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll ("/out:"+(Join-Path $OutputDirectory 'IndexedContentTransferTest.exe')) (Join-Path $source 'App\Services\ContentTransfer.cs') (Join-Path $source 'App\Services\IndexedContentTransfer.cs') (Join-Path $source 'tests\IndexedContentTransferTest.cs') 2>&1|Tee-Object (Join-Path $OutputDirectory 'indexed-content-test-build.log')
if($LASTEXITCODE){throw 'Indexed content transfer compilation failed'}
& (Join-Path $OutputDirectory 'IndexedContentTransferTest.exe') 2>&1|Tee-Object (Join-Path $OutputDirectory 'indexed-content-test.log')
if($LASTEXITCODE){throw 'Actual range/decompression/resume/integrity tests failed'}
$cert=New-SelfSignedCertificate -Type Custom -Subject 'CN=HollowwakeDevelopment' -KeyExportPolicy NonExportable -HashAlgorithm SHA256 -KeyUsage DigitalSignature -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
Export-Certificate -Cert $cert -FilePath (Join-Path $OutputDirectory 'HollowwakeExperiment.cer')|Out-Null
$packageExtension=if($PrivatePreparedDirectory){'.msix'}else{'.appx'}
$project=Join-Path $source 'App\Hollowwake.csproj'
& $msbuild $project /t:Restore /p:Configuration=Release /p:Platform=x64 /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'restore.log');if($LASTEXITCODE){throw 'Managed restore failed'}
& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never "/p:AppxPackageExtension=$packageExtension" /p:AppxPackageSigningEnabled=true "/p:PackageCertificateThumbprint=$($cert.Thumbprint)" "/p:AppxPackageDir=$OutputDirectory\" /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'managed-build.log');if($LASTEXITCODE){throw 'Managed APPX build failed'}
$appx=Get-ChildItem -Recurse $OutputDirectory -File|Where-Object {$_.Name-like 'Hollowwake_*'-and $_.Extension-in @('.appx','.msix')}|Select-Object -First 1;if(!$appx){throw 'APPX not produced'}
Import-Certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeExperiment.cer') -CertStoreLocation 'Cert:\LocalMachine\Root'|Out-Null
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
& (Join-Path $sdk 'signtool.exe') verify /pa /v $appx.FullName 2>&1|Tee-Object (Join-Path $OutputDirectory 'signature.log');if($LASTEXITCODE){throw 'APPX signature verification failed'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$package=[IO.Compression.ZipFile]::OpenRead($appx.FullName)
try {
 if(!($package.Entries|Where-Object FullName -eq 'HollowwakeClientRuntime.dll')){throw 'Native runtime absent from package'}
 if(!($package.Entries|Where-Object FullName -eq 'AppxSignature.p7x')){throw 'Package signature missing'}
 if($PrivateContentDirectory){
  $contentPlan=Get-Content (Join-Path $source 'App\ContentPlan621.json') -Raw|ConvertFrom-Json
  if(!$package.GetEntry('bundled-content.json')){throw 'Bundled content mode marker absent'}
  foreach($record in $contentPlan.files){
   $entry=$package.GetEntry($record.path)
   if(!$entry-or $entry.Length-ne $record.bytes){throw 'Bundled content omitted or resized by packaging'}
   $stream=$entry.Open();$algorithm=[Security.Cryptography.SHA256]::Create()
   try{$actual=([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$algorithm.Dispose()}
   if($actual-ne $record.sha256){throw 'Packaged game content differs from inspected file hash'}
  }
  @{result='PASS signed package contains all pinned data files';files=$contentPlan.files.Count;trimmedFiles=@();gameExecuted=$false}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'bundled-content-audit.json')
 }
 if($PrivatePreparedDirectory){
  $plan=Get-Content (Join-Path $PrivatePreparedDirectory 'plan.json') -Raw|ConvertFrom-Json
  foreach($record in $plan.files){
   $entry=$package.GetEntry($record.packageRelativePath)
   if(!$entry-or $entry.Length-ne $record.bytes){throw 'Prepared native image omitted or resized by packaging'}
   $stream=$entry.Open();$algorithm=[Security.Cryptography.SHA256]::Create()
   try{$actual=([BitConverter]::ToString($algorithm.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$algorithm.Dispose()}
   if($actual-ne $record.sha256){throw 'Packaged game image differs from private preparation hash'}
  }
  foreach($proxy in $plan.proxyDlls){if(!$package.GetEntry($proxy.proxyDll)){throw 'Native import proxy missing from private APPX'}}
  $codeIndex=$package.GetEntry('package-code-index.json');if(!$codeIndex){throw 'Signed client code index missing'}
  $reader=[IO.StreamReader]::new($codeIndex.Open());try{$codePlan=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}
  if($codePlan.files.Count-ne 38){throw 'Unexpected installed client code count'}
  foreach($record in $codePlan.files){$entry=$package.GetEntry($record.path);if(!$entry-or $entry.Length-ne $record.bytes){throw 'Installed client code missing/resized'};$hash=[Security.Cryptography.SHA256]::Create();$stream=$entry.Open();try{$actual=([BitConverter]::ToString($hash.ComputeHash($stream))).Replace('-','').ToLowerInvariant()}finally{$stream.Dispose();$hash.Dispose()};if($actual-ne $record.sha256){throw 'Installed client code hash differs'}}
  @{result='PASS private signed APPX contents match prepared hashes';images=$plan.files.Count;proxies=$plan.proxyDlls.Count;fortniteExecuted=$false;publiclyUploaded=$false}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'private-package-audit.json')
 }
}finally{$package.Dispose()}
@{result='PASS compiled and development signature verified';xboxTested=$false;fortniteExecuted=$false;gamePayloadIncluded=[bool]$PrivatePreparedDirectory;allContentBundled=[bool]$PrivateContentDirectory;route='HWAttemptPackagedClient';nativePathWired=$true}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'package-results.json')
if(!$PrivatePreparedDirectory){
 & (Join-Path $source 'Test-NativePreflight.ps1') -PackagePath $appx.FullName -OutputDirectory $OutputDirectory
 python (Join-Path $source 'tests\compare_imports.py') $OutputDirectory 2>&1|Tee-Object (Join-Path $OutputDirectory 'import-comparison.log')
 if($LASTEXITCODE){throw 'Fixed inventory comparison failed'}
}
Get-FileHash $appx.FullName -Algorithm SHA256|Select-Object Path,Hash|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'package-hash.json')
