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
foreach($name in @('native-launch.log','runtime-policy.txt','native-imports-unresolved.txt','native-platform-imports.json','native-graphics-policy.json','native-window-probe.json')){Copy-Item (Join-Path $data $name) (Join-Path $OutputDirectory $name)}
$native=Get-Content (Join-Path $data 'native-launch.log') -Raw
$platform=Get-Content (Join-Path $data 'native-platform-imports.json') -Raw|ConvertFrom-Json
$graphicsPolicy=Get-Content (Join-Path $data 'native-graphics-policy.json') -Raw|ConvertFrom-Json
$windowProbe=Get-Content (Join-Path $data 'native-window-probe.json') -Raw|ConvertFrom-Json
if($windowProbe.hresult-lt 0){throw ('Native window route failed operation='+$windowProbe.operation+' Win32='+$windowProbe.win32Error+'; game not executed')}
if(!$windowProbe.nativeWindowCreated-or !$windowProbe.nativeDc-or !$windowProbe.paintPixelReadback-or !$windowProbe.ownMessageDispatched-or !$windowProbe.nativeSwapChain-or !$windowProbe.gpuPixelReadback){throw 'Native window route lacks actual behavior evidence'}
if($graphicsPolicy.hresult-lt 0){throw ('Actual AppContainer factory bridge policy failed HRESULT='+$graphicsPolicy.hresult)}
if($platform.total-ne 838-or $platform.pairs.Count-ne 838){throw 'Unredirected inventory changed or audit incomplete'}
if($platform.gameExecuted-or $platform.xboxTested){throw 'Invalid platform audit scope claim'}

$expected=@(Get-Content (Join-Path $PSScriptRoot 'Native\ClientDependencyImports.generated.h')|Where-Object {$_-match '^ \{'}).Count
if(!$native.Contains('NATIVE_PREFLIGHT total='+$expected)){throw 'Unexpected dependency import inventory'}
if($native.Contains('NATIVE_ENTRY calling')){throw 'Forbidden game execution in hosted preflight'}
if(!(Get-Process -Id $id -ErrorAction SilentlyContinue)){throw 'Own preflight app unexpectedly exited'}
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$desktop=[Windows.Automation.AutomationElement]::RootElement
$dialog=$null;$deadline=[DateTime]::UtcNow.AddSeconds(20)
do{$dialog=$desktop.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::AutomationIdProperty,'ClientDiagnostics')));if(!$dialog){Start-Sleep -Milliseconds 250}}while(!$dialog-and[DateTime]::UtcNow-lt $deadline)
if(!$dialog){throw 'Native diagnostic dialog missing'}
$button=$dialog.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::NameProperty,'Missing imports')))
if(!$button){throw 'Missing imports accessible button absent'}
([Windows.Automation.InvokePattern]$button.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern)).Invoke();Start-Sleep -Milliseconds 350
$summary=$dialog.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::AutomationIdProperty,'FailureSummary')))
$before=$summary.Current.Name
$next=$dialog.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::NameProperty,'Next page')))
$previous=$dialog.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::NameProperty,'Previous page')))
if(!$next-or !$previous){throw 'Paged diagnostics buttons missing'}
([Windows.Automation.InvokePattern]$next.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern)).Invoke();Start-Sleep -Milliseconds 350
if($summary.Current.Name-eq $before){throw 'Accessible Next page action did not change diagnostic content'}
$bounds=$dialog.Current.BoundingRectangle
foreach($element in @($summary,$next,$previous)){ $rect=$element.Current.BoundingRectangle;if($rect.Width-le 0-or $rect.Height-le 0){throw 'Diagnostic element has no visible bounds'};if($rect.Top-lt $bounds.Top-or $rect.Bottom-gt $bounds.Bottom-or $rect.Left-lt $bounds.Left-or $rect.Right-gt $bounds.Right){throw 'Diagnostics overflowed dialog bounds'}}
@{result='PASS Windows UIA pagination and content/button bounds';physicalXboxControllerTested=$false;customScrollViewerUsed=$false}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'diagnostics-ui-results.json')
Stop-Process -Id $id -Force
@{result='PASS integrated UWP import preflight only';scope='Hosted Windows AppContainer; no game execution';xboxTested=$false;fortniteExecuted=$false;unresolvedCount=@(Get-Content (Join-Path $data 'native-imports-unresolved.txt')).Count}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'preflight-results.json')
