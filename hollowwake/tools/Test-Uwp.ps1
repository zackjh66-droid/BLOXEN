# Tests only our independently authored companion, never the Fortnite archive.
[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$PackagePath,[Parameter(Mandatory=$true)][string]$OutputDirectory,[string]$BackendOrigin)
$ErrorActionPreference='Stop'
$result=[ordered]@{platform='GitHub Windows Server 2022';xboxExecution=$false;packageDeployment='not_attempted';activation='not_attempted';lobbyIncluded=$false}
try {
  $dependencies=Get-ChildItem (Join-Path (Split-Path $PackagePath) 'Dependencies\x64') -Filter '*.appx' | ForEach-Object { $_.FullName }
  Add-AppxPackage -Path $PackagePath -DependencyPath $dependencies
  $result.packageDeployment='succeeded'
  $installed=Get-AppxPackage -Name 'Hollowwake.Companion'
  Add-Type @'
using System;
using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IApplicationActivationManager {
 [PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string appUserModelId,[MarshalAs(UnmanagedType.LPWStr)]string arguments,uint options,out uint processId);
 [PreserveSig]int ActivateForFile(IntPtr itemArray,[MarshalAs(UnmanagedType.LPWStr)]string verb,out uint processId);
 [PreserveSig]int ActivateForProtocol(IntPtr itemArray,out uint processId);
}
public class HollowwakeActivation {
 public static uint Activate(string id) {
  var t=Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C"));
  var m=(IApplicationActivationManager)Activator.CreateInstance(t);uint pid;int hr=m.ActivateApplication(id,"",0,out pid);Marshal.ThrowExceptionForHR(hr);return pid;
 }
}
'@
  $appProcessId=[HollowwakeActivation]::Activate($installed.PackageFamilyName+'!App')
  $result.activation='succeeded';$result.processId=$appProcessId
  Start-Sleep -Seconds 4
  $result.processStillRunning=[bool](Get-Process -Id $appProcessId -ErrorAction SilentlyContinue)
  if ($BackendOrigin -and $result.processStillRunning) { & (Join-Path $PSScriptRoot 'Test-NativeLink.ps1') -BackendOrigin $BackendOrigin -PackageFamilyName $installed.PackageFamilyName -AppProcessId $appProcessId -OutputDirectory $OutputDirectory }
  # Activation alone does not prove rendering, focus, account linking or Xbox behavior.
} catch {
  $result.error=$_.Exception.Message
  $result.hresult=('0x{0:X8}' -f $_.Exception.HResult)
  if ($result.packageDeployment -eq 'not_attempted') {$result.packageDeployment='failed'} else {$result.activation='failed'}
} finally {
  $result | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $OutputDirectory 'windows-runtime-test.json')
  $result | ConvertTo-Json -Depth 4 | Write-Host
}
