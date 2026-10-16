param([string]$OutputDirectory,[string]$PrivatePreparedDirectory)
$ErrorActionPreference='Stop';$source=$PSScriptRoot;$repo=Split-Path $source -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $source 'artifacts'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$vs=& $vswhere -latest -products '*' -property installationPath;$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $source 'Native\Native.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=false /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'native-build.log')
if($LASTEXITCODE){throw 'Native client integration compilation failed'}
$native=Join-Path $source 'App';New-Item -ItemType Directory -Force $native|Out-Null
Copy-Item (Join-Path $source 'Native\bin\x64\Release\HollowwakeClientRuntime.dll') $native
& dumpbin.exe /nologo /exports (Join-Path $env:SystemRoot 'System32\wldap32.dll') 2>&1|Tee-Object (Join-Path $OutputDirectory 'windows-wldap-ordinal-reference.log')
if($LASTEXITCODE){throw 'OS ordinal reference read failed'}
# Focused production window/network checks; no game code or repeated graphics demos.
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\WindowBridge.cpp') (Join-Path $source 'tests\WindowStateTest.cpp') kernel32.lib /Fe:"$OutputDirectory\WindowStateTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'window-test-build.log')
if($LASTEXITCODE){throw 'Window state test build failed'}
& "$OutputDirectory\WindowStateTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'window-test.log');if($LASTEXITCODE){throw 'Window state behavior failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'Native\NetworkBridge.cpp') (Join-Path $repo 'runtime-native-route\NativeSystem.cpp') (Join-Path $source 'tests\NetworkTest.cpp') kernel32.lib ws2_32.lib runtimeobject.lib /Fe:"$OutputDirectory\NetworkTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'network-test-build.log')
if($LASTEXITCODE){throw 'Network test build failed'}
& "$OutputDirectory\NetworkTest.exe" 2>&1|Tee-Object (Join-Path $OutputDirectory 'network-test.log');if($LASTEXITCODE){throw 'Network behavior failed'}
if($PrivatePreparedDirectory){
 & (Join-Path $source 'private-package\Build-Proxies.ps1') -PreparedDirectory $PrivatePreparedDirectory -RuntimeLibrary (Join-Path $source 'Native\bin\x64\Release\HollowwakeClientRuntime.lib')
 Copy-Item (Join-Path $PrivatePreparedDirectory 'GamePayload\*') (Join-Path $source 'App') -Recurse -Force
 $privateManifest=Join-Path $source 'App\Package.appxmanifest';(Get-Content $privateManifest -Raw).Replace('Version="0.5.0.0"','Version="0.5.1.0"')|Set-Content -Encoding UTF8 $privateManifest
}
$cert=New-SelfSignedCertificate -Type Custom -Subject 'CN=HollowwakeDevelopment' -KeyExportPolicy NonExportable -HashAlgorithm SHA256 -KeyUsage DigitalSignature -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
Export-Certificate -Cert $cert -FilePath (Join-Path $OutputDirectory 'HollowwakeExperiment.cer')|Out-Null
$project=Join-Path $source 'App\Hollowwake.csproj'
& $msbuild $project /t:Restore /p:Configuration=Release /p:Platform=x64 /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'restore.log');if($LASTEXITCODE){throw 'Managed restore failed'}
& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never /p:AppxPackageExtension=.appx /p:AppxPackageSigningEnabled=true "/p:PackageCertificateThumbprint=$($cert.Thumbprint)" "/p:AppxPackageDir=$OutputDirectory\" /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'managed-build.log');if($LASTEXITCODE){throw 'Managed APPX build failed'}
$appx=Get-ChildItem -Recurse $OutputDirectory -Filter 'Hollowwake_*.appx'|Select-Object -First 1;if(!$appx){throw 'APPX not produced'}
Import-Certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeExperiment.cer') -CertStoreLocation 'Cert:\LocalMachine\Root'|Out-Null
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
& (Join-Path $sdk 'signtool.exe') verify /pa /v $appx.FullName 2>&1|Tee-Object (Join-Path $OutputDirectory 'signature.log');if($LASTEXITCODE){throw 'APPX signature verification failed'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$package=[IO.Compression.ZipFile]::OpenRead($appx.FullName)
try {if(!($package.Entries|Where-Object FullName -eq 'HollowwakeClientRuntime.dll')){throw 'Native runtime absent from package'};if(!($package.Entries|Where-Object FullName -eq 'AppxSignature.p7x')){throw 'Package signature missing'}}finally{$package.Dispose()}
@{result='PASS compiled and development signature verified';xboxTested=$false;fortniteExecuted=$false;gamePayloadIncluded=[bool]$PrivatePreparedDirectory;route='HWAttemptPackagedClient';nativePathWired=$true}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'package-results.json')
if(!$PrivatePreparedDirectory){& (Join-Path $source 'Test-NativePreflight.ps1') -PackagePath $appx.FullName -OutputDirectory $OutputDirectory}
Get-FileHash $appx.FullName -Algorithm SHA256|Select-Object Path,Hash|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'package-hash.json')
