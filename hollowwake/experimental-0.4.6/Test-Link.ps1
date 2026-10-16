# UI Automation against our own companion on a disposable hosted Windows runner.
# Credentials, secret device codes and bearer tokens are never read from UI or logged.
param([string]$BackendOrigin,[string]$PackageFamilyName,[uint32]$AppProcessId,[string]$OutputDirectory)
$ErrorActionPreference='Stop'
$result=[ordered]@{platform='Windows Server 2022';xboxExecution=$false;checks=@();result='UNVERIFIED'}
try {
 Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
 function Visible-Control([string]$automationId) {
  $condition=New-Object System.Windows.Automation.AndCondition (New-Object System.Windows.Automation.PropertyCondition ([System.Windows.Automation.AutomationElement]::AutomationIdProperty),$automationId),(New-Object System.Windows.Automation.PropertyCondition ([System.Windows.Automation.AutomationElement]::IsOffscreenProperty),$false)
  return [System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition)
 }
 function Wait-Control([string]$automationId,[int]$seconds=40,[string]$expected='') {
  $deadline=[DateTime]::UtcNow.AddSeconds($seconds)
  do {try {$element=Visible-Control $automationId;if($element -and (!$expected -or $element.Current.Name -eq $expected)){return $element}}catch{};Start-Sleep -Milliseconds 500} while([DateTime]::UtcNow -lt $deadline)
  throw "Native control unavailable: $automationId"
 }
 function Find-Control([string]$automationId) {
  $condition=New-Object System.Windows.Automation.PropertyCondition ([System.Windows.Automation.AutomationElement]::AutomationIdProperty),$automationId
  return [System.Windows.Automation.AutomationElement]::RootElement.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$condition)
 }
 function Invoke-Button([string]$automationId) {
  $button=Find-Control $automationId
  if ($button -and $button.Current.IsOffscreen) {throw "Menu button requires scrolling: $automationId"}
  if (!$button) {throw "Native button unavailable: $automationId"}
  $button.SetFocus();$button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
 }
 function Invoke-ButtonFocus([string]$automationId) {
  $button=Wait-Control $automationId
  $button.SetFocus();Start-Sleep -Milliseconds 1000
 }
 function Check-MenuFits {
  $names=@('LaunchButton','ImportButton','FullImportButton','LogButton','CheckButton','SignOutButton')
  $rectangles=@()
  foreach($name in $names){
   $button=Wait-Control $name
   $walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker;$window=$button
   while($window -and $window.Current.ControlType -ne [System.Windows.Automation.ControlType]::Window){$window=$walker.GetParent($window)}
   if(!$window){throw 'Native window unavailable'}
   $outer=$window.Current.BoundingRectangle;$rect=$button.Current.BoundingRectangle
   if($button.Current.IsOffscreen -or $rect.Width -le 0 -or $rect.Height -le 0 -or $rect.Left -lt $outer.Left -or $rect.Top -lt $outer.Top -or $rect.Right -gt $outer.Right -or $rect.Bottom -gt $outer.Bottom){throw "Menu clipped: $name"}
   $button.SetFocus()
   $rectangles+=@{id=$name;x=$rect.X;y=$rect.Y;width=$rect.Width;height=$rect.Height}
  }
  @{result='PASS';platform='Hosted Windows';xboxTested=$false;allSixButtonsVisibleWithoutScrolling=$true;windowWidth=$outer.Width;windowHeight=$outer.Height;buttons=$rectangles}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory 'menu-fit.json')
 }
 function Save-NativeScreenshot([string]$name,[string]$controlId) {
  try {
   Add-Type -AssemblyName System.Drawing
   $element=Wait-Control $controlId
   $walker=[System.Windows.Automation.TreeWalker]::ControlViewWalker
   $window=$element
   while($window -and $window.Current.ControlType -ne [System.Windows.Automation.ControlType]::Window){$window=$walker.GetParent($window)}
   if(!$window){throw 'No native top-level window found'}
   try {$window.GetCurrentPattern([System.Windows.Automation.WindowPattern]::Pattern).SetWindowVisualState([System.Windows.Automation.WindowVisualState]::Maximized)}catch{}
   Start-Sleep -Milliseconds 1200
   $rect=$window.Current.BoundingRectangle
   $bitmap=New-Object System.Drawing.Bitmap ([int]$rect.Width),([int]$rect.Height)
   $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
   $graphics.CopyFromScreen([int]$rect.X,[int]$rect.Y,0,0,$bitmap.Size)
   $bitmap.Save((Join-Path $OutputDirectory $name),[System.Drawing.Imaging.ImageFormat]::Png)
   $graphics.Dispose();$bitmap.Dispose()
  }catch{('Screenshot unavailable: '+$_.Exception.Message)|Set-Content (Join-Path $OutputDirectory ($name+'.txt'))}
 }
 $status=Invoke-RestMethod -Uri ($BackendOrigin+'/api/status')
 if($status.project -ne 'Hollowwake'){throw 'Wrong backend'}
 $result.checks+='Hosted Windows HTTPS backend access'
 $session=New-Object Microsoft.PowerShell.Commands.WebRequestSession
 $suffix=[Guid]::NewGuid().ToString('N').Substring(0,8);$username='qa_win_'+$suffix
 $password=[Convert]::ToBase64String([Guid]::NewGuid().ToByteArray())+'!Hollowwake42'
 $headers=@{Origin=$BackendOrigin}
 $signup=@{username=$username;name=$username;email=($username+'@example.test');password=$password}|ConvertTo-Json
 $null=Invoke-RestMethod -Uri ($BackendOrigin+'/api/auth/sign-up/email') -Method POST -ContentType 'application/json' -Headers $headers -WebSession $session -Body $signup
 $password=$null;$signup=$null
 function Approve-DisplayedCode {
  $deadline=[DateTime]::UtcNow.AddSeconds(40);$code=''
  do {try {$codeElement=Visible-Control 'CodeText';if($codeElement){$code=$codeElement.Current.Name -replace '\s',''}}catch{};if($code -match '^[A-Z2-9]{8}$'){break};Start-Sleep -Milliseconds 500} while([DateTime]::UtcNow -lt $deadline)
  if($code -notmatch '^[A-Z2-9]{8}$'){throw 'Native device code is not ready'}
  $body=@{user_code=$code}|ConvertTo-Json
  $null=Invoke-RestMethod -Uri ($BackendOrigin+'/api/link/verify') -Method POST -ContentType 'application/json' -Headers $headers -WebSession $session -Body $body
  # Explicit approval is exercised only for this test's own displayed session.
  $null=Invoke-RestMethod -Uri ($BackendOrigin+'/api/link/approve') -Method POST -ContentType 'application/json' -Headers $headers -WebSession $session -Body $body
  $null=Wait-Control 'UsernameText' 40 $username
 }
 Save-NativeScreenshot 'native-linking-windows.png' 'CodeText'
 Approve-DisplayedCode
 Check-MenuFits
 Invoke-ButtonFocus 'ImportButton'
 Save-NativeScreenshot 'native-menu-windows.png' 'UsernameText'
 Invoke-Button 'LogButton'
 $failure=Wait-Control 'FailureSummary'
 if(!$failure.Current.Name.Contains('HOLLOWWAKE_ORIGINAL_FIXTURE!MissingFunction')){throw 'Real persisted fail-fast fixture is missing from short summary'}
 $dialog=Wait-Control 'ClientDiagnostics'
 $closeType=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::ControlTypeProperty,[System.Windows.Automation.ControlType]::Button)
 $closeName=[System.Windows.Automation.PropertyCondition]::new([System.Windows.Automation.AutomationElement]::NameProperty,'Close')
 $closeCondition=[System.Windows.Automation.AndCondition]::new($closeType,$closeName)
 $close=$dialog.FindFirst([System.Windows.Automation.TreeScope]::Descendants,$closeCondition)
 if(!$close -or $close.Current.IsOffscreen){throw 'Diagnostics close button is clipped'}
 Save-NativeScreenshot 'native-diagnostics-windows.png' 'FailureSummary'
 $close.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke()
 $result.checks+='Real persisted failure is visible in compact diagnostics; close button accessible'
 $result.checks+='Native code display, website-account approval, native polling and linked username'
 $devices=Invoke-RestMethod -Uri ($BackendOrigin+'/api/devices') -WebSession $session
 if($devices.devices.Count -ne 1){throw 'Expected exactly one native device session'}
 $seenBefore=$devices.devices[0].lastSeenAt
 Stop-Process -Id $AppProcessId -Force;Start-Sleep -Seconds 2
 $AppProcessId=[HwActivation]::Launch($PackageFamilyName+'!App')
 $null=Wait-Control 'UsernameText' 40 $username
 $devices=Invoke-RestMethod -Uri ($BackendOrigin+'/api/devices') -WebSession $session
 if($devices.devices[0].lastSeenAt -le $seenBefore){throw 'No server validation observed after app restart'}
 $result.checks+='Protected native credential restored after process restart and validated by server'
 Invoke-Button 'SignOutButton';$null=Wait-Control 'StartButton'
 $devices=Invoke-RestMethod -Uri ($BackendOrigin+'/api/devices') -WebSession $session
 if($devices.devices.Count -ne 0){throw 'Native logout did not revoke its session'}
 $result.checks+='Native sign-out button revokes the linked device session'
 Invoke-Button 'StartButton';Approve-DisplayedCode
 $devices=Invoke-RestMethod -Uri ($BackendOrigin+'/api/devices') -WebSession $session
 $revoke=@{id=$devices.devices[0].id}|ConvertTo-Json
 $null=Invoke-RestMethod -Uri ($BackendOrigin+'/api/devices/revoke') -Method POST -ContentType 'application/json' -Headers $headers -WebSession $session -Body $revoke
 $null=Wait-Control 'CodeText' 85
 $result.checks+='Website revocation is detected by the native heartbeat and returns to linking'
 $result.result='PASS'
} catch {$result.error=$_.Exception.Message;$result.hresult=('0x{0:X8}' -f $_.Exception.HResult)} finally {
 $result | ConvertTo-Json -Depth 5 | Set-Content (Join-Path $OutputDirectory 'native-link-ui-test.json')
 $result | ConvertTo-Json -Depth 5 | Write-Host
}
