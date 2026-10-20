param([string]$PackagePath,[string]$ProviderDirectory,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
# Repackage only the owned public fixture, never game-containing private inputs.
if(!$PackagePath){$PackagePath=(Get-ChildItem $OutputDirectory -Recurse -Filter '*.appx'|Where-Object Name -like 'Hollowwake_*'|Select-Object -First 1).FullName}
if(!$PackagePath){throw 'Owned UWP fixture package missing'}
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
$staging=Join-Path $env:RUNNER_TEMP 'HollowwakeOwnedWglPackage'
& (Join-Path $sdk 'makeappx.exe') unpack /p $PackagePath /d $staging /o
if($LASTEXITCODE){throw 'Owned fixture unpack failed'}
if((Test-Path (Join-Path $staging '6.21'))-or (Test-Path (Join-Path $staging 'native-package-manifest.json'))){throw 'Refusing wgl fixture test with proprietary code'}
foreach($name in @('HWMGL.dll','libgallium_wgl.dll','libglapi.dll')){Copy-Item (Join-Path $ProviderDirectory $name) (Join-Path $staging $name)}
foreach($name in @('AppxSignature.p7x','AppxBlockMap.xml','[Content_Types].xml')){Remove-Item -LiteralPath (Join-Path $staging $name) -Force -ErrorAction SilentlyContinue}
$appx=Join-Path $env:RUNNER_TEMP 'HollowwakeOwnedWgl.appx'
& (Join-Path $sdk 'makeappx.exe') pack /d $staging /p $appx /o
if($LASTEXITCODE){throw 'Owned wgl fixture packaging failed'}
$certificate=Get-ChildItem Cert:\CurrentUser\My|Where-Object {$_.Subject-eq 'CN=HollowwakeDevelopment'-and $_.HasPrivateKey}|Sort-Object NotBefore -Descending|Select-Object -First 1
if(!$certificate){throw 'Existing nonexportable CI signing key unavailable'}
& (Join-Path $sdk 'signtool.exe') sign /fd SHA256 /sha1 $certificate.Thumbprint $appx
if($LASTEXITCODE){throw 'Owned wgl fixture signing failed'}
$installed=Get-AppxPackage -Name 'Hollowwake.NativeClientExperiment';if($installed){Remove-AppxPackage $installed.PackageFullName}
$dependencies=@(Get-ChildItem (Join-Path (Split-Path $PackagePath) 'Dependencies\x64') -Filter '*.appx'|ForEach-Object {$_.FullName})
if($dependencies.Count-lt 3){throw 'The owned fixture framework dependencies are missing'}
Add-AppxPackage -Path $appx -DependencyPath $dependencies
$package=Get-AppxPackage -Name 'Hollowwake.NativeClientExperiment'
Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface IHwWglActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr x,[MarshalAs(UnmanagedType.LPWStr)]string y,out uint z);[PreserveSig]int ActivateForProtocol(IntPtr x,out uint y);}
public class HwWglActivation{public static uint Launch(string id){var m=(IHwWglActivation)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")));uint p;Marshal.ThrowExceptionForHR(m.ActivateApplication(id,"--wgl-preflight",0,out p));return p;}}
'@
$data=Join-Path $env:LOCALAPPDATA ('Packages\'+$package.PackageFamilyName+'\LocalState\native-wgl-probe.json')
Remove-Item $data -Force -ErrorAction SilentlyContinue
$pidWgl=[HwWglActivation]::Launch($package.PackageFamilyName+'!App');$deadline=[DateTime]::UtcNow.AddSeconds(45)
try {
 while(!(Test-Path $data)-and[DateTime]::UtcNow-lt $deadline){Start-Sleep -Milliseconds 300}
 if(!(Test-Path $data)){throw 'Actual UWP wgl probe produced no result'}
 Copy-Item $data (Join-Path $OutputDirectory 'native-wgl-probe.json')
 $providerAudit=Join-Path (Split-Path $data) 'native-provider-imports.json';if(Test-Path $providerAudit){Copy-Item $providerAudit $OutputDirectory}
 $result=Get-Content $data -Raw|ConvertFrom-Json
 if($result.hresult-lt 0-or !$result.pixelReadback-or !$result.compositionPresent-or !$result.currentContextAndGuestDc-or $result.gameExecuted-or $result.xboxTested){throw ('Actual UWP wgl provider check failed: '+(Get-Content $data -Raw))}
 Write-Output 'PASS actual packaged provider loading, WGL context/DC, GPU pixel readback and composition presentation; not game/Xbox or original lobby proof'
}finally{Stop-Process -Id $pidWgl -Force -ErrorAction SilentlyContinue}
