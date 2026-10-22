param([Parameter(Mandatory=$true)][string]$GameData,[Parameter(Mandatory=$true)][string]$OutputDirectory,[ValidateSet("original","normalized")][string]$Variant="original")
# Conditional comparison only: exact unchanged original EXE, real verified data,
# no project/Epic credentials and a per-program outbound firewall block.
$attemptStarted=[DateTime]::UtcNow
$ErrorActionPreference='Stop';$process=$null;$junctions=@();$rule='HW-OriginalSource-'+[Guid]::NewGuid().ToString('N')
$original=Join-Path $env:RUNNER_TEMP 'HollowwakePrivateInputs/6.21';$exe=Join-Path $original 'FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
$sourcePin='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478';$pin=$sourcePin
$sourceExe=$exe
if($Variant-eq 'normalized'){
 $prepared=Join-Path $env:RUNNER_TEMP 'HollowwakePrivatePrepared';$normalized=Join-Path $prepared 'InitializedMain.private.exe';$receipt=Get-Content (Join-Path $prepared 'reports/initialized-preparation.json') -Raw|ConvertFrom-Json
 if($receipt.originalSha256-ne $sourcePin-or !$receipt.originalInitializationExecuted-or $receipt.checksPatched-or (Get-FileHash $sourceExe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $sourcePin-or (Get-FileHash $normalized -Algorithm SHA256).Hash.ToLowerInvariant()-ne $receipt.outputSha256){throw 'Exact original and normalized preparation receipts required'}
 $copyRoot=Join-Path $env:RUNNER_TEMP 'HollowwakeNormalizedSourceComparison';if(Test-Path $copyRoot){throw 'Fresh normalized comparison directory required'}
 $manifest=Get-Content (Join-Path $PSScriptRoot '../App/boot-inputs.json') -Raw|ConvertFrom-Json
 foreach($file in $manifest.files){if($file.path-eq '6.21/FortniteGame/Content/Splash/Splash.bmp'){$splash=Join-Path $GameData $file.path;if((Get-Item $splash).Length-ne $file.bytes-or (Get-FileHash $splash -Algorithm SHA256).Hash.ToLowerInvariant()-ne $file.sha256){throw 'Comparison splash differs from verified game data'};continue};if([IO.Path]::GetExtension($file.path)-notin @('.dll','.exe')){throw 'Unexpected comparison code input'};$comparisonInput=Join-Path (Split-Path $original) $file.path;if((Get-Item $comparisonInput).Length-ne $file.bytes-or (Get-FileHash $comparisonInput -Algorithm SHA256).Hash.ToLowerInvariant()-ne $file.sha256){throw 'Comparison source file differs from original build'};$target=Join-Path $copyRoot $file.path;New-Item -ItemType Directory -Force (Split-Path $target)|Out-Null;Copy-Item -LiteralPath $comparisonInput -Destination $target}
 $original=Join-Path $copyRoot '6.21';$exe=Join-Path $original 'FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe';Copy-Item -LiteralPath $normalized -Destination $exe -Force;$pin=$receipt.outputSha256
}
$prefix=if($Variant-eq 'normalized'){'normalized-source'}else{'original-source'}
$logDirectory=Join-Path $OutputDirectory $(if($Variant-eq 'normalized'){'NormalizedSourceLogs'}else{'OriginalSourceLogs'});New-Item -ItemType Directory -Force $logDirectory|Out-Null
$result=[ordered]@{scope='Conditional unchanged original Windows source comparison with same complete data; no account/network/Xbox proof';variant=$Variant;originalSha256=$sourcePin;executableSha256=$pin;outboundBlocked=$false;accountCredentialsSupplied=$false;windowsTest=$true;xboxTested=$false;observations=@();failure=$null}
if($Variant-eq 'normalized'){$result.scope='Normalized original EXE with no DLL conversion, import redirection, service-host patch or account credentials; same verified data and original entry/checks; private Windows comparison only'}
Add-Type -AssemblyName UIAutomationClient;Add-Type -AssemblyName UIAutomationTypes;Add-Type -AssemblyName System.Drawing
try{
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Unchanged original source hash differs'}
 foreach($relative in @('FortniteGame/Content','Engine/Binaries/ThirdParty/CEF3/Win64/Resources')){
  $target=Join-Path (Join-Path $GameData '6.21') $relative;$link=Join-Path $original $relative
  if(!(Test-Path $target -PathType Container)-or (Test-Path $link)){throw 'Original-source data junction is absent or would replace an existing directory'}
  New-Item -ItemType Directory -Force (Split-Path $link)|Out-Null;New-Item -ItemType Junction -Path $link -Value $target|Out-Null;$junctions+=$link
 }
 $cef=Join-Path (Join-Path $GameData '6.21') 'Engine/Binaries/ThirdParty/CEF3/Win64'
 Get-ChildItem $cef -File|Where-Object{$_.Extension-in @('.dat','.bin')}|ForEach-Object{Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $original ('Engine/Binaries/ThirdParty/CEF3/Win64/'+$_.Name))}
 New-NetFirewallRule -DisplayName $rule -Direction Outbound -Program $exe -Action Block -Profile Any|Out-Null;$result.outboundBlocked=$true
 $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=$exe;$info.Arguments='-windowed -d3d11 -AllowSoftwareRendering -ResX=1280 -ResY=720 -abslog="'+(Join-Path $logDirectory 'engine.log')+'"';$info.WorkingDirectory=Split-Path $exe;$info.UseShellExecute=$false;$info.EnvironmentVariables.Clear()
 foreach($key in @('SystemRoot','WINDIR','PATH','TEMP','TMP')){$info.EnvironmentVariables[$key]=[Environment]::GetEnvironmentVariable($key)}
 $privateProfile=Join-Path $env:RUNNER_TEMP ('HollowwakeSourceProfile-'+$Variant);New-Item -ItemType Directory -Force $privateProfile|Out-Null
 foreach($key in @('USERPROFILE','LOCALAPPDATA','APPDATA')){$info.EnvironmentVariables[$key]=$privateProfile}
 $process=[Diagnostics.Process]::Start($info)
 for($tick=1;$tick-le 36;$tick++){
  Start-Sleep -Seconds 5;$process.Refresh();$rows=@()
  foreach($window in [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Window))){
   if($window.Current.ProcessId-eq $process.Id){$texts=@();foreach($e in $window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Text))){if($e.Current.Name.Length-lt 1000){$texts+=$e.Current.Name}};$rows+=@{title=$window.Current.Name;text=$texts}}
  }
  $result.observations+=@{seconds=$tick*5;exited=$process.HasExited;windows=$rows}
  if($tick-in @(3,6,12,24,36)-or $process.HasExited){$b=[Windows.Automation.AutomationElement]::RootElement.Current.BoundingRectangle;$image=[Drawing.Bitmap]::new([int]$b.Width,[int]$b.Height);$g=[Drawing.Graphics]::FromImage($image);try{$g.CopyFromScreen([int]$b.Left,[int]$b.Top,0,0,$image.Size);$image.Save((Join-Path $OutputDirectory ('Windows-'+$prefix+'-comparison-'+$tick+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$image.Dispose()}}
  if($process.HasExited){$result.exitCode=$process.ExitCode;break}
 }
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original executable changed during comparison'}
}catch{$result.failure=$_.Exception.Message}finally{
 if($process-and !$process.HasExited){Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue}
 Remove-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue
 foreach($link in $junctions){if((Get-Item -LiteralPath $link).Attributes-band [IO.FileAttributes]::ReparsePoint){[IO.Directory]::Delete($link,$false)}}
 if($privateProfile){try{$logOutput=$logDirectory;New-Item -ItemType Directory -Force $logOutput|Out-Null;& (Join-Path $PSScriptRoot 'Collect-GameLogs.ps1') -OutputDirectory $logOutput -Since $attemptStarted -Roots @{sourceProfile=(Join-Path $privateProfile 'FortniteGame/Saved/Logs');sourceTree=(Join-Path $original 'FortniteGame/Saved/Logs');profileLocal=(Join-Path $privateProfile 'AppData/Local/FortniteGame/Saved/Logs');windowsLocal=(Join-Path $env:LOCALAPPDATA 'FortniteGame/Saved/Logs')}}catch{$result.logCollectionFailure=$_.Exception.GetType().Name}}
 $result|ConvertTo-Json -Depth 9|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory ($prefix+'-with-data.json'))
}
