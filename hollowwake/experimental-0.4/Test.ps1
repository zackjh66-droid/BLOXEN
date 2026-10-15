param([string]$PackagePath,[string]$OutputDirectory)
$ErrorActionPreference='Stop';$r=[ordered]@{platform='Hosted Windows 2022';xboxTested=$false;fortniteExecuted=$false;result='FAIL'}
try{
 $deps=@(Get-ChildItem (Join-Path (Split-Path $PackagePath) 'Dependencies\x64') -Filter '*.appx'|ForEach-Object{$_.FullName});Add-AppxPackage -Path $PackagePath -DependencyPath $deps
 $p=Get-AppxPackage -Name 'Hollowwake.ClientExperiment'
 Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface IHwActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr x,[MarshalAs(UnmanagedType.LPWStr)]string y,out uint z);[PreserveSig]int ActivateForProtocol(IntPtr x,out uint y);}
public class HwActivation{public static uint Launch(string id,string args=""){var m=(IHwActivation)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")));uint p;Marshal.ThrowExceptionForHR(m.ActivateApplication(id,args,0,out p));return p;}}
'@
 $pidApp=[HwActivation]::Launch($p.PackageFamilyName+'!App');$r.activated=$true;$data=Join-Path $env:LOCALAPPDATA ('Packages\'+$p.PackageFamilyName+'\LocalState');$deadline=[DateTime]::UtcNow.AddSeconds(45)
 while(!(Test-Path (Join-Path $data 'runtime-policy.txt'))-and[DateTime]::UtcNow-lt $deadline){Start-Sleep -Seconds 1}
 if(!(Test-Path (Join-Path $data 'runtime-policy.txt'))){throw 'Runtime policy log was not created'}
 $r.policy=([string](Get-Content (Join-Path $data 'runtime-policy.txt') -Raw)).Trim()
 foreach($n in @('runtime-policy.txt','native-launch.log','managed-launch.log')){if(Test-Path (Join-Path $data $n)){Copy-Item (Join-Path $data $n) (Join-Path $OutputDirectory $n)}}
 # Record a denied policy as an observed blocker, not as successful native execution.
 $r.originalDynamicCodeExecuted=$r.policy.Contains('memoryExecution=621');$r.windowMessageContract=$r.policy.Contains('windowMessageContract=621');$r.graphicsContract=$r.policy.Contains('graphicsContract=621');if(!$r.originalDynamicCodeExecuted-or !$r.windowMessageContract-or !$r.graphicsContract){throw 'Native memory/window/graphics contract failed; inspect runtime-policy.txt'};$r.result='PASS: installed, activated, policy outcome recorded';Stop-Process -Id $pidApp -Force
 $trapProcessId=[HwActivation]::Launch($p.PackageFamilyName+'!App','hollowwake-selftest-unsupported');$deadline=[DateTime]::UtcNow.AddSeconds(40)
 do{Start-Sleep -Seconds 1;$trapAlive=Get-Process -Id $trapProcessId -ErrorAction SilentlyContinue}while($trapAlive-and[DateTime]::UtcNow-lt $deadline)
 $trapLog=Get-Content (Join-Path $data 'native-launch.log') -Raw
 $r.unsupportedOriginalFixtureLogged=$trapLog.Contains('CALLED_UNSUPPORTED HOLLOWWAKE_ORIGINAL_FIXTURE!MissingFunction');$r.failFastProcessExited=!$trapAlive
 if(!$r.unsupportedOriginalFixtureLogged-or !$r.failFastProcessExited){throw 'Unsupported-import fail-fast/logging contract failed'}
 $pidApp=[HwActivation]::Launch($p.PackageFamilyName+'!App')
 & (Join-Path $PSScriptRoot 'Test-Link.ps1') -BackendOrigin 'https://hollowwake.jihnjohn10.chatgpt.site' -PackageFamilyName $p.PackageFamilyName -AppProcessId $pidApp -OutputDirectory $OutputDirectory
 $link=Get-Content (Join-Path $OutputDirectory 'native-link-ui-test.json') -Raw|ConvertFrom-Json;$r.accountLink=$link.result;if($link.result-ne 'PASS'){throw 'Native project-account linking regression failed'}
 $r.result='PASS: native contracts, fail-fast logs and project account linking';
}catch{$r.error=$_.Exception.Message;throw}finally{
 if($data-and(Test-Path $data)){foreach($n in @('runtime-policy.txt','native-launch.log','managed-launch.log')){if(Test-Path (Join-Path $data $n)){Copy-Item (Join-Path $data $n) (Join-Path $OutputDirectory $n)}}}
 $r|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory 'windows-test.json')}
