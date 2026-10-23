param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$PrivateInputLeasePath,[switch]$AttachNativeTrace)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
$result=[ordered]@{stage='find-installed-package';windowsTest=$true;xboxTested=$false;launchButtonInvoked=$false;nativeThreadCreated=$false;originalLobbyObserved=$false;serverMenusVerified=$false;testAccount=$null;failure=$null}
$applicationPid=0;$processObserver=$null;$executionTrace=$null;$browser=$null;$data=$null;$gameData=$null;$verifiedData=$false;$attemptStarted=[DateTime]::UtcNow
function SaveResult { $result|ConvertTo-Json -Depth 7|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'integrated-windows-result.json') }
function Invoke-IntegratedActivation([string]$AppId){
 for($activationAttempt=1;$activationAttempt-le 2;$activationAttempt++){
  try{return [HwIntegratedActivation]::Launch($AppId)}catch{
   $problem=$_.Exception;$retryable=$false;$codes=@();while($problem){$code=$problem.HResult.ToString('X8');$codes+=('0x'+$code);if($code-eq '80040905'){$retryable=$true};$problem=$problem.InnerException}
   if(!$result.activationFailures){$result.activationFailures=@()};$result.activationFailures+=@{attempt=$activationAttempt;hresults=$codes;time=[DateTime]::UtcNow.ToString('o')};SaveResult
   if(!$retryable-or $activationAttempt-ge 2){throw};Start-Sleep -Milliseconds 1500
  }
 }
}
function Post($path,$body){Invoke-RestMethod -Method Post -Uri ($origin+$path) -WebSession $browser -Headers @{Origin=$origin} -ContentType 'application/json' -Body ($body|ConvertTo-Json -Compress) -TimeoutSec 30}
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
function Control($id){$e=[Windows.Automation.AutomationElement]::RootElement.FindFirst([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::AutomationIdProperty,$id));if($e-and !$e.Current.IsOffscreen){return $e};return $null}
function InvokeControl($id){$until=[DateTime]::UtcNow.AddSeconds(30);do{$e=Control $id;if($e-and $e.Current.IsEnabled){$e.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke();return};Start-Sleep -Milliseconds 250}while([DateTime]::UtcNow-lt $until);throw "Actual UI control unavailable: $id"}
function Screenshot($name){$bounds=[Windows.Automation.AutomationElement]::RootElement.Current.BoundingRectangle;$bitmap=[Drawing.Bitmap]::new([int]$bounds.Width,[int]$bounds.Height);$g=[Drawing.Graphics]::FromImage($bitmap);try{$g.CopyFromScreen([int]$bounds.Left,[int]$bounds.Top,0,0,$bitmap.Size);$bitmap.Save((Join-Path $OutputDirectory ($name+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$bitmap.Dispose()}}
try{
 $package=Get-AppxPackage -Name 'Hollowwake.NativeClientExperiment'
 if($package){$data=Join-Path $env:LOCALAPPDATA ('Packages\'+$package.PackageFamilyName+'\LocalState');New-Item -ItemType Directory -Force $data|Out-Null;$gameData=Join-Path $data 'GameClean';$result.package=$package.PackageFullName}
 else{$gameData=Join-Path $env:RUNNER_TEMP 'HollowwakeIndependentOriginalData'}
 $result.stage='download-complete-pinned-data';SaveResult
 $vs=& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
 $csc=Join-Path $vs 'MSBuild\Current\Bin\Roslyn\csc.exe';$services=Join-Path $PSScriptRoot '../App/Services';$stager=Join-Path $env:RUNNER_TEMP 'HollowwakeStageGameData.exe'
 & $csc /nologo /target:exe /langversion:latest /r:System.Net.Http.dll /r:System.IO.Compression.dll /r:System.IO.Compression.FileSystem.dll /r:System.Web.Extensions.dll "/out:$stager" (Join-Path $PSScriptRoot 'Stage-WindowsGameData.cs') (Join-Path $services 'ContentTransfer.cs') (Join-Path $services 'IndexedContentTransfer.cs') (Join-Path $services 'SequentialContentTransfer.cs')
 if($LASTEXITCODE-ne 0){throw 'Production transfer input stager did not compile'}
 & $stager (Join-Path $PSScriptRoot '../App/ContentPlan621.json') (Join-Path $PSScriptRoot '../App/ContentIndex621.json') $gameData (Join-Path $OutputDirectory 'integrated-windows-data.json') $PrivateInputLeasePath
 if($LASTEXITCODE-ne 0){throw 'Pinned full game-data acquisition failed; no runtime result inferred'}
 $verifiedData=$true
 $result.stage='verify-installed-private-client';SaveResult
 if(!$package){throw 'Signed private app is not installed; verified original-source comparison can still run independently'}
 $installedPlan=Join-Path $package.InstallLocation 'native-package-manifest.json';$preparedPlan=Join-Path $env:RUNNER_TEMP 'HollowwakePrivatePrepared/GamePayload/native-package-manifest.json'
 if(!(Test-Path $installedPlan)-or !(Test-Path $preparedPlan)-or (Get-FileHash $installedPlan -Algorithm SHA256).Hash-ne (Get-FileHash $preparedPlan -Algorithm SHA256).Hash){throw 'Installed app differs from the current verified private candidate; no account or app launch attempted'}
 $result.stage='create-temporary-normal-account';SaveResult
 $origin='https://hollowwake.jihnjohn10.chatgpt.site';$browser=[Microsoft.PowerShell.Commands.WebRequestSession]::new()
 $suffix=[Guid]::NewGuid().ToString('N').Substring(0,8);$username='hw_win_probe_'+$suffix;$email=$username+'@hollowwake.test';$password=[Guid]::NewGuid().ToString('N')+'!A9'+[Guid]::NewGuid().ToString('N')
 $signup=Post '/api/auth/sign-up/email' @{email=$email;password=$password;name=$username;username=$username};$password=$null
 if(!$signup.user.id){throw 'Temporary normal account registration did not return a user'}
 $result.testAccount=@{id=$signup.user.id;username=$username};$signup=$null
 Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface IHwIntegratedActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr x,[MarshalAs(UnmanagedType.LPWStr)]string y,out uint z);[PreserveSig]int ActivateForProtocol(IntPtr x,out uint y);}
public class HwIntegratedActivation{public static uint Launch(string id){var m=(IHwIntegratedActivation)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")));uint p;Marshal.ThrowExceptionForHR(m.ActivateApplication(id,"",0,out p));return p;}}
'@
 $result.stage='normal-app-activation-and-real-device-link';SaveResult
 $applicationPid=Invoke-IntegratedActivation ($package.PackageFamilyName+'!App');$processObserver=[Diagnostics.Process]::GetProcessById($applicationPid);$null=$processObserver.Handle;$deadline=[DateTime]::UtcNow.AddSeconds(90);$code=$null
 do{$e=Control 'CodeText';if($e){$candidate=$e.Current.Name.Replace(' ','').Replace('-','');if($candidate-match '^[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{8}$'){$code=$candidate;break}};if(!(Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)){throw 'App exited before device approval'};Start-Sleep -Milliseconds 500}while([DateTime]::UtcNow-lt $deadline)
 if(!$code){throw 'Actual app did not display a current device-link code'}
 $null=Post '/api/link/verify' @{user_code=$code};$null=Post '/api/link/approve' @{user_code=$code};$code=$null
 $deadline=[DateTime]::UtcNow.AddSeconds(90);do{$e=Control 'LaunchButton';if($e){break};Start-Sleep -Seconds 1}while([DateTime]::UtcNow-lt $deadline);if(!$e){throw 'Approved device did not reach linked menu'}
 $result.stage='production-app-independent-content-verification';SaveResult
 $managed=Join-Path $data 'managed-launch.log';$deadline=[DateTime]::UtcNow.AddMinutes(10);$retried=$false
 do{
  $tail='';if(Test-Path $managed){$tail=Get-Content $managed -Raw}
  if($tail-match 'CONTENT_READY bytes=26449827840 total=139 missing=0'){break}
  $import=Control 'ImportButton';if(!$retried-and $import-and $import.Current.IsEnabled){InvokeControl 'ImportButton';$retried=$true}
  if(!(Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)){throw 'App exited during independent real content verification'}
  Start-Sleep -Seconds 2
 }while([DateTime]::UtcNow-lt $deadline)
 if($tail-notmatch 'CONTENT_READY bytes=26449827840 total=139 missing=0'){throw 'Production app did not verify the complete selected data; game launch not attempted'}
 Screenshot 'Windows-verified-data-before-real-launch'
 # Debug attachment changes OS-visible process state. Normal integration runs
 # use no debugger; the narrow trace is explicit opt-in investigation only.
 $result.debuggerRequested=[bool]$AttachNativeTrace;$result.debuggerAttached=$false
 if($AttachNativeTrace){
 $traceExe=Join-Path $env:RUNNER_TEMP 'HWNativeExecutionTrace.exe';$traceLog=Join-Path $OutputDirectory 'native-execution-trace.log'
 if(!(Test-Path $traceExe)){throw 'Internal execution trace was not compiled'}
 $mainPath=Join-Path $package.InstallLocation '6.21/FortniteGame/Binaries/Win64/Fortnite621.imports.private.dll';$mainHash=(Get-FileHash $mainPath -Algorithm SHA256).Hash.ToLowerInvariant()
 $executionTrace=Start-Process -FilePath $traceExe -ArgumentList @([string]$applicationPid,('"'+$package.InstallLocation+'"'),$mainHash,('"'+$traceLog+'"'),'230') -PassThru
 $deadline=[DateTime]::UtcNow.AddSeconds(15)
 do{if(Test-Path $traceLog){$traceText=Get-Content $traceLog -Raw;if($traceText-match '"event":"attached"'){break}};$executionTrace.Refresh();if($executionTrace.HasExited){break};Start-Sleep -Milliseconds 100}while([DateTime]::UtcNow-lt $deadline)
 $result.debuggerAttached=[bool]($traceText-match '"event":"attached"')
 if(!$result.debuggerAttached){$result.traceFailure='Windows debugger could not attach; inspect native-execution-trace.log. No guest behavior was inferred.'}
 }
 $result.stage='invoke-actual-launch-button';SaveResult
 InvokeControl 'LaunchButton';$result.launchButtonInvoked=$true;$result.launchedAt=[DateTime]::UtcNow.ToString('o');SaveResult
 $native=Join-Path $data 'native-launch.log';$observations=@();$deadline=[DateTime]::UtcNow.AddMinutes(3);$tick=0
 do{
  Start-Sleep -Seconds 5;$tick++
  $alive=[bool](Get-Process -Id $applicationPid -ErrorAction SilentlyContinue)
  $nativeText='';if(Test-Path $native){$nativeText=Get-Content $native -Raw}
  if($nativeText-match 'NATIVE_THREAD original entry is OS start address'){$result.nativeThreadCreated=$true}
  if($tick-in @(2,4,6,8,10,12,24,36)-or !$alive){Screenshot ('Windows-actual-client-after-launch-'+$tick)}
  $dialogs=[Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Window))
  $titles=@();foreach($dialog in $dialogs){$title=$dialog.Current.Name;if($title-match 'Fortnite|Error|Message|Hollowwake'){$texts=$dialog.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Text));$body=@();foreach($textControl in $texts){if($textControl.Current.Name.Length-lt 1000){$body+=$textControl.Current.Name}};$body=@($body|ForEach-Object{$_-replace '(?i)(-AUTH_PASSWORD=)[^\s]+','$1[REDACTED]'-replace '(?<![A-Za-z0-9_-])[A-Za-z0-9_-]{43}(?![A-Za-z0-9_-])','[REDACTED]'});$titles+=@{title=$title;text=$body};if(($body-join ' ')-match 'Failed to open descriptor file'){ $result.firstEngineError=($body-join ' ');$result.originalEngineStartupObserved=$true }}}
  $observations+=@{seconds=$tick*5;alive=$alive;windowTitles=$titles}
  if($result.firstEngineError){Screenshot 'Windows-original-client-engine-error';$result.stage='actual-client-engine-error';break}
  if(!$alive){
   $result.stage='actual-client-process-exited'
   try{$result.processExitCode=$processObserver.ExitCode;$result.processExitCodeHex=('0x'+$processObserver.ExitCode.ToString('X8'))}catch{$result.processExitCodeUnavailable=$_.Exception.GetType().Name}
   $result.failure='Actual client process exited during observation; this does not establish a loading screen or lobby.'
   break
  }
 }while([DateTime]::UtcNow-lt $deadline)
 $result.observations=$observations
 if($result.stage-eq 'invoke-actual-launch-button'){$result.stage='actual-client-observation-complete'}
 # No screenshot classifier or successful helper build asserts a lobby/menu connection.
 $result.scope='Real signed app, complete verified data, approved normal account, actual launch action. Native thread creation alone is not proof of a loading screen or lobby.'
}catch{
 $result.failure=$_.Exception.Message;Write-Host ('INTEGRATED FAILURE stage='+$result.stage+' reason='+$result.failure)
 if($applicationPid){try{Screenshot 'Windows-actual-test-failure'}catch{}}
}finally{
 Remove-Item $PrivateInputLeasePath -ErrorAction SilentlyContinue
 if($data){foreach($name in @('native-launch.log','managed-launch.log','launcher-startup.log','content-data-audit.txt','client-missing-imports.txt')){$path=Join-Path $data $name;if(Test-Path $path){$text=Get-Content $path -Raw;$text=$text-replace '(?i)(-AUTH_PASSWORD=)[^\s]+','$1[REDACTED]';$text|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory ('integrated-'+$name))}}}
 if($data){try{& (Join-Path $PSScriptRoot 'Collect-GameLogs.ps1') -OutputDirectory $OutputDirectory -Since $attemptStarted -Roots @{mapped=(Join-Path $data 'GameClean/6.21/FortniteGame/Saved/Logs');packageLocal=(Join-Path (Split-Path $data) 'LocalCache/Local/FortniteGame/Saved/Logs');packageRoaming=(Join-Path (Split-Path $data) 'LocalCache/Roaming/FortniteGame/Saved/Logs');windowsLocal=(Join-Path $env:LOCALAPPDATA 'FortniteGame/Saved/Logs')}}catch{Write-Host ('Actual game-log collection failed: '+$_.Exception.GetType().Name)}}
 if($data){try{& (Join-Path $PSScriptRoot 'Collect-GameCrashContext.ps1') -CrashRoot (Join-Path $data 'GameClean/6.21/FortniteGame/Saved/Crashes') -OutputDirectory $OutputDirectory -Since $attemptStarted}catch{Write-Host ('Crash-code context export failed: '+$_.Exception.GetType().Name)}}
 # Revoke only this temporary account's linked sessions, never another user's.
 if($browser-and $result.testAccount){try{$devices=Invoke-RestMethod -Uri ($origin+'/api/devices') -WebSession $browser -TimeoutSec 30;foreach($device in $devices.devices){$null=Post '/api/devices/revoke' @{id=$device.id}};$null=Post '/api/auth/sign-out' @{}}catch{Write-Host 'Temporary test-session cleanup could not complete; no credential was exported'}}
 if($applicationPid){Stop-Process -Id $applicationPid -Force -ErrorAction SilentlyContinue}
 if($executionTrace){if(!$executionTrace.WaitForExit(10000)){$result.traceFailure='Internal debugger did not exit after app cleanup';Stop-Process -Id $executionTrace.Id -Force -ErrorAction SilentlyContinue};$executionTrace.Dispose()}
 if($processObserver){$processObserver.Dispose()}
 SaveResult
}
if($verifiedData-and ($result.failure-or $result.firstEngineError-or $result.stage-eq 'actual-client-process-exited')){& (Join-Path $PSScriptRoot 'Test-OriginalSourceWithData.ps1') -GameData $gameData -OutputDirectory $OutputDirectory -AttachNativeTrace:$AttachNativeTrace;& (Join-Path $PSScriptRoot 'Test-OriginalSourceWithData.ps1') -GameData $gameData -OutputDirectory $OutputDirectory -Variant normalized}
if($result.failure){exit 1}
