param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$source=$PSScriptRoot;$repo=Split-Path $source -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $source 'artifacts'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$vs=& $vswhere -latest -products '*' -property installationPath;$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $source 'Native\Native.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=false /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'native-build.log')
if($LASTEXITCODE){throw 'Native client integration compilation failed'}
$native=Join-Path $source 'App\Native';New-Item -ItemType Directory -Force $native|Out-Null
Copy-Item (Join-Path $source 'Native\bin\x64\Release\HollowwakeClientRuntime.dll') $native
# Build the original TLS donor using the same official toolchain/API-family checks.
& (Join-Path $repo 'client-runtime\Build-Checks.ps1') -OutputDirectory (Join-Path $OutputDirectory 'focused-checks')
Copy-Item (Join-Path $OutputDirectory 'focused-checks\HollowwakeTlsDonor.dll') $native
$cert=New-SelfSignedCertificate -Type Custom -Subject 'CN=HollowwakeDevelopment' -KeyExportPolicy NonExportable -HashAlgorithm SHA256 -KeyUsage DigitalSignature -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
Export-Certificate -Cert $cert -FilePath (Join-Path $OutputDirectory 'HollowwakeExperiment.cer')|Out-Null
$project=Join-Path $source 'App\Hollowwake.csproj'
& $msbuild $project /t:Restore /p:Configuration=Release /p:Platform=x64 /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'restore.log');if($LASTEXITCODE){throw 'Managed restore failed'}
& $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never /p:AppxPackageExtension=.appx /p:AppxPackageSigningEnabled=true "/p:PackageCertificateThumbprint=$($cert.Thumbprint)" "/p:AppxPackageDir=$OutputDirectory\" /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'managed-build.log');if($LASTEXITCODE){throw 'Managed APPX build failed'}
$appx=Get-ChildItem -Recurse $OutputDirectory -Filter 'Hollowwake_*.appx'|Select-Object -First 1;if(!$appx){throw 'APPX not produced'}
Import-Certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeExperiment.cer') -CertStoreLocation 'Cert:\LocalMachine\Root'|Out-Null
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
& (Join-Path $sdk 'signtool.exe') verify /pa /v $appx.FullName 2>&1|Tee-Object (Join-Path $OutputDirectory 'signature.log');if($LASTEXITCODE){throw 'APPX signature verification failed'}
& (Join-Path $source 'Test.ps1') -PackagePath $appx.FullName -OutputDirectory $OutputDirectory
Get-FileHash $appx.FullName -Algorithm SHA256|Select-Object Path,Hash|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'package-hash.json')
