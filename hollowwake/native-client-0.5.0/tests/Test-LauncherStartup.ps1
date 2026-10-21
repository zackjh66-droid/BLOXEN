param([Parameter(Mandatory=$true)][string]$PackagePath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$dependencies=@(Get-ChildItem (Join-Path (Split-Path $PackagePath) 'Dependencies\x64') -Filter '*.appx'|ForEach-Object{$_.FullName})
Add-AppxPackage -Path $PackagePath -DependencyPath $dependencies -ForceApplicationShutdown
$package=Get-AppxPackage -Name 'Hollowwake.NativeClientExperiment'
if(!$package){throw 'Signed private package not installed'}
$data=Join-Path $env:LOCALAPPDATA ('Packages\'+$package.PackageFamilyName+'\LocalState')
Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface IHwStartupActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr x,[MarshalAs(UnmanagedType.LPWStr)]string y,out uint z);[PreserveSig]int ActivateForProtocol(IntPtr x,out uint y);}
public class HwStartupActivation{public static uint Launch(string id){var m=(IHwStartupActivation)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")));uint p;Marshal.ThrowExceptionForHR(m.ActivateApplication(id,"",0,out p));return p;}}
'@
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
$results=@()
for($attempt=1;$attempt-le 2;$attempt++){
 $offset=0;$nativeLog=Join-Path $data 'native-launch.log'
 if(Test-Path $nativeLog){$offset=(Get-Item $nativeLog).Length}
 $startupLog=Join-Path $data 'launcher-startup.log'
 $before='';if(Test-Path $startupLog){$before=Get-Content $startupLog -Raw}
 $applicationPid=[HwStartupActivation]::Launch($package.PackageFamilyName+'!App')
 try{
  $deadline=[DateTime]::UtcNow.AddSeconds(50);$visible=$null
  do{
   if(!(Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)){throw "Signed private launcher exited during ordinary start attempt $attempt"}
   $desktop=[Windows.Automation.AutomationElement]::RootElement
   foreach($controlId in @('RetryButton','ConnectButton','LaunchButton')){
    $candidate=$desktop.FindFirst([Windows.Automation.TreeScope]::Descendants,(New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::AutomationIdProperty,$controlId)))
    if($candidate-and !$candidate.Current.IsOffscreen-and $candidate.Current.BoundingRectangle.Width-gt 0){$visible=$controlId;break}
   }
   if(!$visible){Start-Sleep -Milliseconds 250}
  }while(!$visible-and [DateTime]::UtcNow-lt $deadline)
  if(!$visible){throw 'Signed private launcher did not display an interactive account/setup/menu control'}
  for($tick=0;$tick-lt 16;$tick++){Start-Sleep -Milliseconds 500;if(!(Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)){throw 'Signed private launcher closed after rendering its menu'}}
  $stages=Get-Content $startupLog -Raw
  if($stages.Length-le $before.Length-or !$stages.Substring($before.Length).Contains('stage=page-loaded')){throw 'Current activation did not reach the actual page Loaded handler'}
  if(Test-Path $nativeLog){$stream=[IO.File]::Open($nativeLog,'Open','Read','ReadWrite');try{$stream.Position=$offset;$reader=[IO.StreamReader]::new($stream);$tail=$reader.ReadToEnd()}finally{$stream.Dispose()};if($tail.Contains('NATIVE_ENTRY calling')-or $tail.Contains('ATTEMPT_BEGIN')){throw 'Opening launcher unexpectedly attempted game execution'}}
  $results+=@{attempt=$attempt;interactiveControl=$visible;survivedAfterMenuSeconds=8;normalActivation=$true;gameEntryAttempted=$false}
 }finally{Stop-Process -Id $applicationPid -Force -ErrorAction SilentlyContinue}
}
@{result='PASS signed private launcher ordinary opening and reopening';packageVersion=$package.Version.ToString();runs=$results;scope='Windows AppContainer installation and ordinary activation of the actual game-containing deliverable';xboxTested=$false;fortniteExecuted=$false}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'launcher-startup-results.json')
Copy-Item $startupLog (Join-Path $OutputDirectory 'launcher-startup.log')
