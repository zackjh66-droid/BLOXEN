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
Add-Type -AssemblyName System.Drawing
$captures=Join-Path $OutputDirectory 'WindowsScreenshots';New-Item -ItemType Directory -Force $captures|Out-Null
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
  # Capture the real running app's window pixels. No HTML recreation, fixture
  # menu, synthetic lobby or game-success inference is used for these images.
  $window=$candidate
  while($window-and $window.Current.ControlType-ne [Windows.Automation.ControlType]::Window){$window=[Windows.Automation.TreeWalker]::ControlViewWalker.GetParent($window)}
  if(!$window){throw 'Could not locate actual launcher window for screenshot'}
  $bounds=$window.Current.BoundingRectangle
  $screen=$desktop.Current.BoundingRectangle
  $left=[int][Math]::Max($bounds.Left,$screen.Left);$top=[int][Math]::Max($bounds.Top,$screen.Top)
  $width=[int]([Math]::Min($bounds.Right,$screen.Right)-$left);$height=[int]([Math]::Min($bounds.Bottom,$screen.Bottom)-$top)
  if($width-lt 320-or $height-lt 200-or $width-gt 8192-or $height-gt 8192){throw 'Actual visible app screenshot bounds invalid'}
  $bitmap=[Drawing.Bitmap]::new($width,$height);$graphics=[Drawing.Graphics]::FromImage($bitmap)
  try{$graphics.CopyFromScreen($left,$top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $captures ('Hollowwake-'+$package.Version.ToString()+'-Windows-open-'+$attempt+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
  $stages=Get-Content $startupLog -Raw
  if($stages.Length-le $before.Length-or !$stages.Substring($before.Length).Contains('stage=page-loaded')){throw 'Current activation did not reach the actual page Loaded handler'}
  if(Test-Path $nativeLog){$stream=[IO.File]::Open($nativeLog,'Open','Read','ReadWrite');try{$stream.Position=$offset;$reader=[IO.StreamReader]::new($stream);$tail=$reader.ReadToEnd()}finally{$stream.Dispose()};if($tail.Contains('NATIVE_ENTRY calling')-or $tail.Contains('ATTEMPT_BEGIN')){throw 'Opening launcher unexpectedly attempted game execution'}}
  $results+=@{attempt=$attempt;interactiveControl=$visible;survivedAfterMenuSeconds=8;normalActivation=$true;gameEntryAttempted=$false;screenshot=('Hollowwake-'+$package.Version.ToString()+'-Windows-open-'+$attempt+'.png');capture='Actual desktop pixels of running signed private package window'}
 }finally{Stop-Process -Id $applicationPid -Force -ErrorAction SilentlyContinue}
}
# Exercise the actual startup boundary with a real WinRT I/O failure. No app
# switch, fake HRESULT, game-success marker or modified account is introduced.
$policy=Join-Path $data 'runtime-policy.txt';$backup=Join-Path $data ('owned-policy-backup-'+[Guid]::NewGuid().ToString('N'));$ownedDirectory=$false;$applicationPid=0
try{
 if(Test-Path $policy -PathType Leaf){Move-Item -LiteralPath $policy -Destination $backup}
 New-Item -ItemType Directory -Path $policy|Out-Null;$ownedDirectory=$true
 $applicationPid=[HwStartupActivation]::Launch($package.PackageFamilyName+'!App');$deadline=[DateTime]::UtcNow.AddSeconds(50);$failureButton=$null
 do{
  $failureButton=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'StartupRetryButton'))
  if($failureButton-and !$failureButton.Current.IsOffscreen){break}
  if(!(Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)){throw 'Launcher exited instead of presenting managed startup failure'}
  Start-Sleep -Milliseconds 250
 }while([DateTime]::UtcNow-lt $deadline)
 if(!$failureButton-or $failureButton.Current.IsOffscreen){throw 'Actual managed initialization failure was not presented'}
 $failureText=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,'StartupFailureText'))
 if(!$failureText-or $failureText.Current.Name-notmatch 'startup-policy-write'-or $failureText.Current.Name-notmatch '0x[0-9A-F]{8}'){throw 'Actual startup failure stage/HRESULT missing from visible UI'}
 Start-Sleep -Seconds 3;if(!(Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)){throw 'Launcher exited after presenting initialization error'}
 Remove-Item -LiteralPath $policy;$ownedDirectory=$false;if(Test-Path $backup){Move-Item -LiteralPath $backup -Destination $policy}
 $failureButton.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();$deadline=[DateTime]::UtcNow.AddSeconds(50);$recovered=$false
 do{
  foreach($id in @('RetryButton','LaunchButton','ConnectButton')){
   $e=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,$id))
   if($e-and !$e.Current.IsOffscreen){$recovered=$true;break}
  }
  if(!$recovered){Start-Sleep -Milliseconds 250}
 }while(!$recovered-and [DateTime]::UtcNow-lt $deadline)
 if(!$recovered){throw 'Restoring real storage and invoking Retry did not recover the launcher'}
 $results+=@{scope='Real managed startup I/O error and actual Retry UI';injectedCondition='Owned empty directory occupies runtime-policy.txt';visibleStageAndHresult=$true;survivedFailureSeconds=3;retryRecovered=$true;gameEntryAttempted=$false}
}finally{
 if($applicationPid){Stop-Process -Id $applicationPid -Force -ErrorAction SilentlyContinue}
 if($ownedDirectory-and (Test-Path $policy -PathType Container)){Remove-Item -LiteralPath $policy}
 if(Test-Path $backup){Move-Item -LiteralPath $backup -Destination $policy}
}
@{result='PASS signed private launcher ordinary opening/reopening and real managed startup I/O recovery';packageVersion=$package.Version.ToString();runs=$results;scope='Windows AppContainer installation and ordinary activation of the actual game-containing deliverable';xboxTested=$false;fortniteExecuted=$false}|ConvertTo-Json -Depth 6|Set-Content (Join-Path $OutputDirectory 'launcher-startup-results.json')
Copy-Item $startupLog (Join-Path $OutputDirectory 'launcher-startup.log')
