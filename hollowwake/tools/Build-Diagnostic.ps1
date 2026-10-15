# Builds/executes only original diagnostic/fixtures. Never executes downloaded game/loader code.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$repo=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $repo 'artifacts\runtime-diagnostic'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$projectDir=Join-Path $repo 'runtime-diagnostic';$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$msbuild=& $vswhere -latest -products '*' -find 'MSBuild\**\Bin\MSBuild.exe'|Select-Object -First 1
if(!$msbuild){throw 'MSBuild unavailable'}
& $vswhere -latest -products '*' -format json | Set-Content (Join-Path $OutputDirectory 'windows-tools.json')
foreach($shader in @(@('ColorVS.hlsl','vs_4_0','VSBlob'),@('ColorPS.hlsl','ps_4_0','PSBlob'))){
 & (Join-Path $sdk 'fxc.exe') /nologo /T $shader[1] /E main /Fh (Join-Path $projectDir ('Shaders\'+$shader[2]+'.h')) /Vn $shader[2] (Join-Path $projectDir ('Shaders\'+$shader[0]))
 if($LASTEXITCODE -ne 0){throw 'Original shader compilation failed'}
}
$certificate=New-SelfSignedCertificate -Type Custom -Subject 'CN=HollowwakeDevelopment' -HashAlgorithm SHA256 -KeyUsage DigitalSignature -KeyExportPolicy NonExportable -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
Export-Certificate -Cert $certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeDiagnostic.cer')|Out-Null
& $msbuild (Join-Path $projectDir 'RuntimeDiagnostic.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never /p:AppxPackageExtension=.appx /p:AppxPackageSigningEnabled=true "/p:PackageCertificateThumbprint=$($certificate.Thumbprint)" "/p:AppxPackageDir=$OutputDirectory\" /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'build.log')
if($LASTEXITCODE -ne 0){throw 'Native UWP diagnostic build failed'}
$appx=Get-ChildItem -Recurse $OutputDirectory -Filter '*Diagnostic*.appx'|Select-Object -First 1
if(!$appx){throw 'No diagnostic APPX produced'}
Import-Certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeDiagnostic.cer') -CertStoreLocation 'Cert:\LocalMachine\Root'|Out-Null
& (Join-Path $sdk 'signtool.exe') verify /pa /v $appx.FullName 2>&1|Tee-Object (Join-Path $OutputDirectory 'signature.log');if($LASTEXITCODE -ne 0){throw 'Signature failed'}
& (Join-Path $PSScriptRoot 'Test-Diagnostic.ps1') -PackagePath $appx.FullName -OutputDirectory $OutputDirectory
Get-FileHash $appx.FullName -Algorithm SHA256|Select-Object Path,Hash|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'package-hashes.json')
