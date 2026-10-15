param([string]$PackagePath,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$result=[ordered]@{platform='GitHub Windows Server 2022';xboxExecution=$false;fortniteIncluded=$false;result='FAIL'}
try {
 $deps=@(Get-ChildItem (Join-Path (Split-Path $PackagePath) 'Dependencies\x64') -Filter '*.appx' -ErrorAction SilentlyContinue|ForEach-Object{$_.FullName})
 if($deps.Count){Add-AppxPackage -Path $PackagePath -DependencyPath $deps}else{Add-AppxPackage -Path $PackagePath}
 $installed=Get-AppxPackage -Name 'Hollowwake.RuntimeDiagnostic'
 Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface IDiagnosticActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr items,[MarshalAs(UnmanagedType.LPWStr)]string verb,out uint pid);[PreserveSig]int ActivateForProtocol(IntPtr items,out uint pid);}
public class DiagnosticActivation{public static uint Launch(string id){var type=Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C"));var manager=(IDiagnosticActivation)Activator.CreateInstance(type);uint pid;Marshal.ThrowExceptionForHR(manager.ActivateApplication(id,"",0,out pid));return pid;}}
'@
 $appProcessId=[DiagnosticActivation]::Launch($installed.PackageFamilyName+'!App');$result.activated=$true
 $logPath=Join-Path $env:LOCALAPPDATA ('Packages\'+$installed.PackageFamilyName+'\LocalState\diagnostic.json')
 $deadline=[DateTime]::UtcNow.AddSeconds(45)
 do{Start-Sleep -Seconds 2}while(!(Test-Path $logPath)-and [DateTime]::UtcNow-lt $deadline)
 if(!(Test-Path $logPath)){throw 'Diagnostic did not write its own log'}
 $first=Get-Content $logPath -Raw|ConvertFrom-Json;Start-Sleep -Seconds 7;$second=Get-Content $logPath -Raw|ConvertFrom-Json
 if($second.successfulPresents -le $first.successfulPresents -or $second.fatalError){throw 'No advancing successful D3D presentation'}
 $result.graphics='PASS: successful Present count advanced';$result.first=$first;$result.second=$second
 # Send only harmless keys to our own active original window; verify resulting counters, not just process activation.
 Add-Type -AssemblyName System.Windows.Forms
 $shell=New-Object -ComObject WScript.Shell;$null=$shell.AppActivate([int]$appProcessId);Start-Sleep -Milliseconds 500
 [System.Windows.Forms.SendKeys]::SendWait('{RIGHT}c p ');Start-Sleep -Seconds 7
 $third=Get-Content $logPath -Raw|ConvertFrom-Json;$result.keyboardInput=($third.inputEvents-gt $second.inputEvents);$result.afterInput=$third
 if(!$result.keyboardInput){throw 'Original diagnostic did not observe injected keyboard input'}
 Copy-Item $logPath (Join-Path $OutputDirectory 'diagnostic-windows.json')
 # Capture our own test VM screen for visual review, never a user desktop.
 Add-Type -AssemblyName System.Drawing
 $bounds=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds;$bitmap=New-Object System.Drawing.Bitmap $bounds.Width,$bounds.Height;$graphics=[System.Drawing.Graphics]::FromImage($bitmap);$graphics.CopyFromScreen($bounds.Location,[System.Drawing.Point]::Empty,$bounds.Size);$bitmap.Save((Join-Path $OutputDirectory 'diagnostic-windows.png'));$graphics.Dispose();$bitmap.Dispose()
 $result.hardwareControllerTested=$false;$result.audioAudiblyVerified=$false;$result.suspendResumeTested=$false;$result.result='PASS'
 Stop-Process -Id $appProcessId -Force
} catch{$result.error=$_.Exception.Message;throw}finally{$result|ConvertTo-Json -Depth 8|Set-Content (Join-Path $OutputDirectory 'diagnostic-test.json');$result|ConvertTo-Json -Depth 8|Write-Host}
