param([Parameter(Mandatory=$true)][string]$GameData,[Parameter(Mandatory=$true)][string]$OutputDirectory)
# Conditional comparison only: exact unchanged original EXE, real verified data,
# no project/Epic credentials and a per-program outbound firewall block.
$ErrorActionPreference='Stop';$process=$null;$junctions=@();$rule='HW-OriginalSource-'+[Guid]::NewGuid().ToString('N')
$original=Join-Path $env:RUNNER_TEMP 'HollowwakePrivateInputs/6.21';$exe=Join-Path $original 'FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
$pin='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
$result=[ordered]@{scope='Conditional unchanged original Windows source comparison with same complete data; no account/network/Xbox proof';originalSha256=$pin;outboundBlocked=$false;accountCredentialsSupplied=$false;windowsTest=$true;xboxTested=$false;observations=@();failure=$null}
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
 $info=[Diagnostics.ProcessStartInfo]::new();$info.FileName=$exe;$info.Arguments='-windowed -d3d11 -ResX=1280 -ResY=720';$info.WorkingDirectory=Split-Path $exe;$info.UseShellExecute=$false;$info.EnvironmentVariables.Clear()
 foreach($key in @('SystemRoot','WINDIR','PATH','TEMP','TMP')){$info.EnvironmentVariables[$key]=[Environment]::GetEnvironmentVariable($key)}
 $privateProfile=Join-Path $env:RUNNER_TEMP 'HollowwakeOriginalSourceProfile';New-Item -ItemType Directory -Force $privateProfile|Out-Null
 foreach($key in @('USERPROFILE','LOCALAPPDATA','APPDATA')){$info.EnvironmentVariables[$key]=$privateProfile}
 $process=[Diagnostics.Process]::Start($info)
 for($tick=1;$tick-le 12;$tick++){
  Start-Sleep -Seconds 5;$process.Refresh();$rows=@()
  foreach($window in [Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Window))){
   if($window.Current.ProcessId-eq $process.Id){$texts=@();foreach($e in $window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Text))){if($e.Current.Name.Length-lt 1000){$texts+=$e.Current.Name}};$rows+=@{title=$window.Current.Name;text=$texts}}
  }
  $result.observations+=@{seconds=$tick*5;exited=$process.HasExited;windows=$rows}
  if($tick-in @(3,6,12)-or $process.HasExited){$b=[Windows.Automation.AutomationElement]::RootElement.Current.BoundingRectangle;$image=[Drawing.Bitmap]::new([int]$b.Width,[int]$b.Height);$g=[Drawing.Graphics]::FromImage($image);try{$g.CopyFromScreen([int]$b.Left,[int]$b.Top,0,0,$image.Size);$image.Save((Join-Path $OutputDirectory ('Windows-original-source-comparison-'+$tick+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$image.Dispose()}}
  if($process.HasExited){$result.exitCode=$process.ExitCode;break}
 }
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original executable changed during comparison'}
}catch{$result.failure=$_.Exception.Message}finally{
 if($process-and !$process.HasExited){Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue}
 Remove-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue
 foreach($link in $junctions){if((Get-Item -LiteralPath $link).Attributes-band [IO.FileAttributes]::ReparsePoint){[IO.Directory]::Delete($link,$false)}}
 $result|ConvertTo-Json -Depth 9|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'original-source-with-data.json')
}
