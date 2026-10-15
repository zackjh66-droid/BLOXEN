# Builds only the independent Hollowwake account companion, never Fortnite.
[CmdletBinding()]
param([string]$BackendOrigin = '', [string]$CertificateThumbprint, [string]$OutputDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$uri = $null
if ($BackendOrigin) {
    $uri = [Uri]$BackendOrigin
    if (!$uri.IsAbsoluteUri -or $uri.Scheme -ne 'https' -or $uri.AbsolutePath -ne '/' -or $uri.Query -or $uri.Fragment -or $uri.UserInfo) { throw 'Supply a real HTTPS origin with no path or query.' }
}
if (!$OutputDirectory) { $OutputDirectory = Join-Path $repo 'artifacts\uwp' }
New-Item -ItemType Directory -Force $OutputDirectory | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio 2022 with UWP C#/.NET Native tools and Windows SDK 10.0.19041.0.' }
$msbuild = & $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
if (!$msbuild) { throw 'MSBuild not found.' }
if (!$CertificateThumbprint) {
    $certificate = New-SelfSignedCertificate -Type Custom -Subject 'CN=HollowwakeDevelopment' -FriendlyName 'Hollowwake development only' -HashAlgorithm SHA256 -KeyUsage DigitalSignature -KeyExportPolicy NonExportable -CertStoreLocation 'Cert:\CurrentUser\My' -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3','2.5.29.19={text}')
    $CertificateThumbprint = $certificate.Thumbprint
} else { $certificate = Get-Item "Cert:\CurrentUser\My\$CertificateThumbprint" }
if ($certificate.Subject -ne 'CN=HollowwakeDevelopment' -or !$certificate.HasPrivateKey) { throw 'Certificate publisher must match the package manifest and have a local private key.' }
# Export public certificate ONLY. Never export PFX/private keys.
Export-Certificate -Cert $certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeDevelopment.cer') | Out-Null
$configPath = Join-Path $repo 'uwp\Hollowwake\backend-config.json'
$originalConfig = [System.IO.File]::ReadAllText($configPath)
try {
    @{BackendOrigin=$(if ($uri) { $uri.GetLeftPart([System.UriPartial]::Authority) } else { '' });AllowLocalHttp=$false;DevelopmentOnly=$true;FortnitePortAvailable=$false} | ConvertTo-Json | Set-Content -Encoding UTF8 $configPath
    $project = Join-Path $repo 'uwp\Hollowwake\Hollowwake.csproj'
    & $msbuild $project /t:Restore /p:Configuration=Release /p:Platform=x64 /nologo 2>&1 | Tee-Object (Join-Path $OutputDirectory 'restore.log')
    if ($LASTEXITCODE -ne 0) { throw 'NuGet restore failed. See restore.log.' }
    & $msbuild $project /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=true /p:UapAppxPackageBuildMode=SideloadOnly /p:AppxBundle=Never /p:AppxPackageExtension=.appx /p:AppxPackageSigningEnabled=true "/p:PackageCertificateThumbprint=$CertificateThumbprint" "/p:AppxPackageDir=$OutputDirectory\" /nologo 2>&1 | Tee-Object (Join-Path $OutputDirectory 'build.log')
    if ($LASTEXITCODE -ne 0) { throw 'UWP build failed. See build.log. No working package is implied.' }
    $packages = Get-ChildItem -Recurse $OutputDirectory -Filter 'Hollowwake_*.appx'
    if (!$packages) { throw 'Build completed without an APPX; inspect output.' }
    $sdkBin = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
    $signtool = Join-Path $sdkBin 'signtool.exe'
    # Trust only this ephemeral development certificate on the disposable build runner.
    Import-Certificate -FilePath (Join-Path $OutputDirectory 'HollowwakeDevelopment.cer') -CertStoreLocation 'Cert:\LocalMachine\Root' | Out-Null
    foreach ($package in $packages) {
        & $signtool verify /pa /v $package.FullName 2>&1 | Tee-Object (Join-Path $OutputDirectory 'signature-verification.log')
        if ($LASTEXITCODE -ne 0) { throw 'Package signature verification failed.' }
    }
    foreach ($package in $packages) { & (Join-Path $PSScriptRoot 'Test-Uwp.ps1') -PackagePath $package.FullName -OutputDirectory $OutputDirectory -BackendOrigin $BackendOrigin }
    $runtime = Get-Content (Join-Path $OutputDirectory 'windows-runtime-test.json') -Raw | ConvertFrom-Json
    if ($runtime.packageDeployment -ne 'succeeded' -or $runtime.activation -ne 'succeeded' -or !$runtime.processStillRunning) { throw 'Windows installation/activation test failed.' }
    if ($BackendOrigin) {
        $native = Get-Content (Join-Path $OutputDirectory 'native-link-ui-test.json') -Raw | ConvertFrom-Json
        if ($native.result -ne 'PASS') { throw 'Native linking verification did not pass; see native-link-ui-test.json.' }
    }
    $packages | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash | ConvertTo-Json | Set-Content (Join-Path $OutputDirectory 'package-hashes.json')
    Write-Host 'Companion APPX build completed. Fortnite is not included. Xbox runtime validation is still required.'
} finally { [System.IO.File]::WriteAllText($configPath, $originalConfig) }
