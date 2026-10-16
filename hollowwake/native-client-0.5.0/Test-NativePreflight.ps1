param([string]$PackagePath,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$dependencies=@(Get-ChildItem (Join-Path (Split-Path $PackagePath) 'Dependencies\x64') -Filter '*.appx'|ForEach-Object{$_.FullName})
Add-AppxPackage -Path $PackagePath -DependencyPath $dependencies
$package=Get-AppxPackage -Name 'Hollowwake.NativeClientExperiment'
Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface IHwActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr x,[MarshalAs(UnmanagedType.LPWStr)]string y,out uint z);[PreserveSig]int ActivateForProtocol(IntPtr x,out uint y);}
public class HwActivation{public static uint Launch(string id,string args){var m=(IHwActivation)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")));uint p;Marshal.ThrowExceptionForHR(m.ActivateApplication(id,args,0,out p));return p;}}
'@
$id=[HwActivation]::Launch($package.PackageFamilyName+'!App','--native-preflight')
$data=Join-Path $env:LOCALAPPDATA ('Packages\'+$package.PackageFamilyName+'\LocalState')
$deadline=[DateTime]::UtcNow.AddSeconds(40)
while([DateTime]::UtcNow-lt $deadline){
 if(Test-Path (Join-Path $data 'native-launch.log')){if((Get-Content (Join-Path $data 'native-launch.log') -Raw).Contains('NATIVE_PREFLIGHT total=')){break}}
 Start-Sleep -Seconds 1
}
if(!(Test-Path (Join-Path $data 'native-imports-unresolved.txt'))){throw 'Integrated AppContainer native preflight did not produce exact-name report'}
foreach($name in @('native-launch.log','runtime-policy.txt','native-imports-unresolved.txt')){Copy-Item (Join-Path $data $name) (Join-Path $OutputDirectory $name)}
$native=Get-Content (Join-Path $data 'native-launch.log') -Raw
$expected=@(Get-Content (Join-Path $PSScriptRoot 'Native\ClientDependencyImports.generated.h')|Where-Object {$_-match '^ \{'}).Count
if(!$native.Contains('NATIVE_PREFLIGHT total='+$expected)){throw 'Unexpected dependency import inventory'}
if($native.Contains('NATIVE_ENTRY calling')){throw 'Forbidden game execution in hosted preflight'}
if(!(Get-Process -Id $id -ErrorAction SilentlyContinue)){throw 'Own preflight app unexpectedly exited'}
Stop-Process -Id $id -Force
@{result='PASS integrated UWP import preflight only';scope='Hosted Windows AppContainer; no game execution';xboxTested=$false;fortniteExecuted=$false;unresolvedCount=@(Get-Content (Join-Path $data 'native-imports-unresolved.txt')).Count}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'preflight-results.json')
