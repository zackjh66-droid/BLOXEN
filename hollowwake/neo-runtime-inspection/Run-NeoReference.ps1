# Run only on a fresh disposable Windows runner with no repository checkout,
# repository token, Hollowwake account, hosting key, signing key or game input.
# The exact user-supplied reference stays an ordinary signed AppContainer app.
param([Parameter(Mandatory=$true)][string]$OutputDirectory,[int]$MaximumSeconds=1800)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Security
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
$work=Join-Path $env:RUNNER_TEMP 'NeoReferencePrivate'
New-Item -ItemType Directory -Force $work|Out-Null
$report=[ordered]@{scope='Exact supplied Neo reference installed and observed on disposable Windows; not an Xbox or Hollowwake game test';stage='download-pinned-package';xboxTested=$false;hollowwakeCredentialsSupplied=$false;neoCredentialsSupplied=$false;packageInstalled=$false;activated=$false;downloadObserved=$false;downloadComplete=$false;loginGateObserved=$false;actions=@();progress=@();storage=@();failure=$null}
$appProcess=$null;$pkg=$null;$trustThumbprint=$null;$started=[DateTime]::UtcNow
function Save { $report|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'neo-runtime-result.json') }
function CaptureReference {
 try {
  $dir=Join-Path $work 'screens';New-Item -ItemType Directory -Force $dir|Out-Null
  if(@(Get-ChildItem $dir -Filter '*.png').Count-ge 10){return}
  $windows=[Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.Condition]::TrueCondition)
  foreach($w in $windows){
   if($w.Current.ProcessId-ne $appPid-and $w.Current.Name-notmatch '\bNeo(?:XS|FN)?\b'){continue}
   $r=$w.Current.BoundingRectangle
   if($r.Width-lt 64-or $r.Height-lt 64-or $r.Width-gt 4096-or $r.Height-gt 4096){continue}
   $b=[Drawing.Bitmap]::new([int]$r.Width,[int]$r.Height);$g=[Drawing.Graphics]::FromImage($b)
   try{$g.CopyFromScreen([int]$r.X,[int]$r.Y,0,0,$b.Size);$b.Save((Join-Path $dir (([DateTime]::UtcNow.ToString('HHmmssfff'))+'.png')),[Drawing.Imaging.ImageFormat]::Png)}finally{$g.Dispose();$b.Dispose()}
   break
  }
 }catch{}
}
function PinnedDownload($url,$target,$hash){
 & curl.exe --fail --location --silent --show-error --retry 2 --connect-timeout 30 --max-time 600 --output $target $url
 if($LASTEXITCODE-ne 0){throw 'Pinned reference HTTP acquisition failed'}
 if((Get-FileHash $target -Algorithm SHA256).Hash.ToLowerInvariant()-ne $hash){throw 'Pinned reference SHA256 mismatch; execution refused'}
}
function ReadMember($archive,$name){
 $entry=$archive.GetEntry($name);if(!$entry-or $entry.Length-gt 1048576){throw 'Bounded package metadata missing'}
 $stream=$entry.Open();$copy=[IO.MemoryStream]::new();try{$stream.CopyTo($copy);return ,$copy.ToArray()}finally{$stream.Dispose();$copy.Dispose()}
}
function Snapshot($roots){
 $files=@();foreach($root in $roots){if(!(Test-Path -LiteralPath $root.path)){continue}
  foreach($f in Get-ChildItem -LiteralPath $root.path -Recurse -File -ErrorAction SilentlyContinue){
   $relative=$f.FullName.Substring($root.path.Length).TrimStart('\').Replace('\','/')
   if($relative-match '(^|/)(Saved|Config|Logs|Accounts?|Tokens?|EBWebView|WebView|Cache)(/|$)'){continue}
   if($f.Extension-notin @('.pak','.sig','.dll','.exe','.part','.tmp','.zip','.rar','.bin')){continue}
   if($relative.Length-gt 240-or $relative-notmatch '^[A-Za-z0-9 _./+()\-]+$'){continue}
   $files+=@{root=$root.name;relativePath=$relative;bytes=$f.Length;lastWriteUtc=$f.LastWriteTimeUtc.ToString('o');fullPath=$f.FullName}
  }
 };return ,$files
}
function PublicInventory($items,[bool]$hashCode){
 foreach($f in $items){$row=[ordered]@{root=$f.root;relativePath=$f.relativePath;bytes=$f.bytes;lastWriteUtc=$f.lastWriteUtc}
  if($hashCode-and $f.relativePath-match '\.(dll|exe)$'-and $f.bytes-le 536870912){try{$row.sha256=(Get-FileHash -LiteralPath $f.fullPath -Algorithm SHA256).Hash.ToLowerInvariant()}catch{$row.hashUnavailable=$true}}
  $row
 }
}
try{
 Save
 $packageFile=Join-Path $work 'NeoXS_1.0.6.0_x64.msix'
 PinnedDownload 'https://content-cdn.neofn.dev/NeoXS_1.0.6.0_x64.msix' $packageFile '9d7ead048380ffd73d15afe1c53bd03c54a4ba186a93856bff652937c2c03ef8'
 $report.packageSha256=(Get-FileHash $packageFile -Algorithm SHA256).Hash.ToLowerInvariant();$report.packageBytes=(Get-Item $packageFile).Length
 $zip=[IO.Compression.ZipFile]::OpenRead($packageFile)
 try{
  [xml]$manifest=[Text.Encoding]::UTF8.GetString((ReadMember $zip 'AppxManifest.xml'))
  $signature=ReadMember $zip 'AppxSignature.p7x'
  if([Text.Encoding]::ASCII.GetString($signature,0,4)-ne 'PKCX'){throw 'Unexpected Appx signature prefix'}
  $cms=[Security.Cryptography.Pkcs.SignedCms]::new();$cms.Decode([byte[]]$signature[4..($signature.Length-1)]);$cms.CheckSignature($true)
  if($cms.SignerInfos.Count-ne 1){throw 'Unexpected signer count'}
  $signer=$cms.SignerInfos[0].Certificate
  if(!$signer-or $manifest.Package.Identity.Name-ne 'Microsoft.MicrosoftEdge.Neo'-or $manifest.Package.Identity.ProcessorArchitecture-ne 'x64'-or $signer.Subject-ne $manifest.Package.Identity.Publisher){throw 'Reference manifest/signer identity differs'}
  $report.referenceIdentity=@{name=[string]$manifest.Package.Identity.Name;publisher=[string]$manifest.Package.Identity.Publisher;version=[string]$manifest.Package.Identity.Version;certificateThumbprint=$signer.Thumbprint;signatureMathVerified=$true}
  $app=@($manifest.Package.Applications.Application)[0]
  $report.executable=[string]$app.Executable
 }finally{$zip.Dispose()}
 $report.stage='install-microsoft-dependency';Save
 $dependencyFile=Join-Path $work 'Microsoft.VCLibs.x64.14.00.appx'
 PinnedDownload 'https://content-cdn.neofn.dev/Microsoft.VCLibs.x64.14.00.appx' $dependencyFile '9c17b521f9d690a1f504da5108ed6eec5669eb3a8fd1331eef43e40d84e74283'
 $required=[version]'14.0.33519.0';$installed=@(Get-AppxPackage -Name 'Microsoft.VCLibs.140.00'|Where-Object{$_.Architecture-eq 'X64'-and $_.Version-ge $required})
 if(!$installed.Count){Add-AppxPackage -Path $dependencyFile}
 $report.stage='install-exact-reference';Save
 # Trust only the exact supplied package signer, in this disposable user's store.
 $store=[Security.Cryptography.X509Certificates.X509Store]::new('TrustedPeople','CurrentUser');$store.Open('ReadWrite');try{$store.Add($signer);$trustThumbprint=$signer.Thumbprint}finally{$store.Close()}
 Add-AppxPackage -Path $packageFile -DependencyPath $dependencyFile
 $pkg=Get-AppxPackage -Name 'Microsoft.MicrosoftEdge.Neo'
 if(!$pkg-or $pkg.Version-ne [version]'1.0.6.0'){throw 'Exact reference did not register'}
 $report.packageInstalled=$true;$report.packageFullName=$pkg.PackageFullName
 $local=Join-Path $env:LOCALAPPDATA ('Packages\'+$pkg.PackageFamilyName+'\LocalState')
 $external='D:\DevelopmentFiles\Neo'
 Add-Type @'
using System;using System.Runtime.InteropServices;
[ComImport,Guid("2e941141-7f97-4756-ba1d-9decde894a3d"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]interface INeoReferenceActivation{[PreserveSig]int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)]string id,[MarshalAs(UnmanagedType.LPWStr)]string args,uint options,out uint pid);[PreserveSig]int ActivateForFile(IntPtr x,[MarshalAs(UnmanagedType.LPWStr)]string y,out uint z);[PreserveSig]int ActivateForProtocol(IntPtr x,out uint y);}
public static class NeoReferenceActivation{
 [DllImport("userenv.dll",CharSet=CharSet.Unicode)]static extern int DeriveAppContainerSidFromAppContainerName(string name,out IntPtr sid);
 [DllImport("advapi32.dll")]static extern IntPtr FreeSid(IntPtr sid);
 public static string Sid(string family){IntPtr sid;Marshal.ThrowExceptionForHR(DeriveAppContainerSidFromAppContainerName(family,out sid));try{return new System.Security.Principal.SecurityIdentifier(sid).Value;}finally{FreeSid(sid);}}
 public static uint Launch(string id){var m=(INeoReferenceActivation)Activator.CreateInstance(Type.GetTypeFromCLSID(new Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")));uint pid;Marshal.ThrowExceptionForHR(m.ActivateApplication(id,"",0,out pid));return pid;}
}
'@
 # Give this AppContainer access only to its expected game-content directory.
 if(Test-Path 'D:\'){
  New-Item -ItemType Directory -Force $external|Out-Null
  $sid=[Security.Principal.SecurityIdentifier]::new([NeoReferenceActivation]::Sid($pkg.PackageFamilyName))
  $acl=Get-Acl $external;$rule=[Security.AccessControl.FileSystemAccessRule]::new($sid,'Modify','ContainerInherit,ObjectInherit','None','Allow');$acl.AddAccessRule($rule);Set-Acl $external $acl
 }
 $roots=@(@{name='installed-package';path=$pkg.InstallLocation},@{name='local-state';path=$local},@{name='development-files';path=$external})
 $before=Snapshot $roots
 @(PublicInventory $before $true)|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'neo-installed-before.json')
 $report.stage='activate-reference';Save
 $appPid=[NeoReferenceActivation]::Launch($pkg.PackageFamilyName+'!'+[string]$app.Id)
 $appProcess=[Diagnostics.Process]::GetProcessById($appPid);$null=$appProcess.Handle;$report.activated=$true;$report.stage='observe-installer';Save
 $clicked=@{};$initialData=0L;$lastData=-1L;$stableSince=[DateTime]::UtcNow;$deadline=[DateTime]::UtcNow.AddSeconds($MaximumSeconds);$observedAt=[DateTime]::UtcNow
 do{
  Start-Sleep -Seconds 10
  $appProcess.Refresh();if($appProcess.HasExited){$report.appExited=$true;$report.appExitCode=$appProcess.ExitCode;break}
  $windows=[Windows.Automation.AutomationElement]::RootElement.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.PropertyCondition]::new([Windows.Automation.AutomationElement]::ControlTypeProperty,[Windows.Automation.ControlType]::Window))
  CaptureReference
  $buttonLabels=@();$ready=$false
  foreach($window in $windows){
   if($window.Current.ProcessId-ne $appPid-and $window.Current.Name-notmatch '\bNeo(?:XS)?\b'){continue}
   foreach($control in $window.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)){
    try{
     if($control.Current.IsOffscreen){continue};$label=$control.Current.Name;$type=$control.Current.ControlType
     if($type-eq [Windows.Automation.ControlType]::Button){
      if($label-match '^(?:log\s?in|sign\s?in|register|create (?:an? )?account|connect discord)$'){$report.loginGateObserved=$true}
      if($label-match '^(?:download(?: (?:game|content|files))?|install(?: (?:game|content|files))?|update(?: game)?|play|launch)$'-and $control.Current.IsEnabled){
       $action=$Matches[0].ToLowerInvariant();$buttonLabels+=$action
       if(!$clicked.ContainsKey($action)-and !$report.loginGateObserved){$pattern=$null;if($control.TryGetCurrentPattern([Windows.Automation.InvokePattern]::Pattern,[ref]$pattern)){$pattern.Invoke();$clicked[$action]=$true;$report.actions+=@{action=$action;time=[DateTime]::UtcNow.ToString('o')}}}
      }
     }
     if($type-eq [Windows.Automation.ControlType]::Text-and $label.Length-le 256){
      if($label-match '(?i)(downloading|installing|verifying|extracting|preparing|updating)'){
       $numbers=@([regex]::Matches($label,'\d+(?:\.\d+)?\s*(?:%|[KMGT]i?B(?:/s)?)')|ForEach-Object{$_.Value})
       if($numbers.Count-and $report.progress.Count-lt 120){$report.progress+=@{time=[DateTime]::UtcNow.ToString('o');quantities=$numbers}}
      }
      if($label-match '^(?:installation complete|download complete|ready to play|game installed)$'){$ready=$true}
      if($label-match '^(?:sign in|log in|link (?:your )?account|required login)$'){$report.loginGateObserved=$true}
     }
    }catch{} # UI controls may disappear while their real parent changes screens.
   }
  }
  $current=Snapshot $roots;$data=@($current|Where-Object{$_.root-ne 'installed-package'});$bytes=0L;foreach($f in $data){$bytes+=$f.bytes}
  if($bytes-gt $initialData){$report.downloadObserved=$true}
  if($bytes-ne $lastData){$stableSince=[DateTime]::UtcNow;$lastData=$bytes}
  $report.storage+=@{time=[DateTime]::UtcNow.ToString('o');observedDataFiles=$data.Count;observedDataBytes=$bytes}
  if($ready-and $report.downloadObserved-and ([DateTime]::UtcNow-$stableSince).TotalSeconds-ge 30){$report.downloadComplete=$true;break}
  Save
  if($report.loginGateObserved-and !$report.downloadObserved-and ([DateTime]::UtcNow-$observedAt).TotalSeconds-ge 60){$report.stage='reference-account-required';break}
  if(!$report.downloadObserved-and ([DateTime]::UtcNow-$observedAt).TotalSeconds-ge 240){$report.stage='no-download-observed';break}
 }while([DateTime]::UtcNow-lt $deadline)
 if($report.downloadComplete){$report.stage='reference-reported-download-complete'}elseif($report.downloadObserved){$report.stage='download-observed-completeness-unverified'}
}catch{
 $codes=@();$e=$_.Exception;while($e){$codes+=('0x'+$e.HResult.ToString('X8'));$e=$e.InnerException}
 $report.failure=@{stage=$report.stage;exceptionType=$_.Exception.GetType().FullName;hresults=$codes;errorId=($_.FullyQualifiedErrorId-replace '[^A-Za-z0-9_.-]','_').Substring(0,[Math]::Min(180,$_.FullyQualifiedErrorId.Length))}
}finally{
 if($appProcess){CaptureReference;try{if(!$appProcess.HasExited){Stop-Process -Id $appProcess.Id -Force -ErrorAction SilentlyContinue}}catch{}}
 if($pkg){
  $after=Snapshot $roots
  @(PublicInventory $after $true)|ConvertTo-Json -Depth 5|Set-Content -Encoding UTF8 (Join-Path $OutputDirectory 'neo-installed-after.json')
  $report.actualGameDataFiles=@($after|Where-Object{$_.root-ne 'installed-package'-and $_.relativePath-match '\.(pak|sig)$'}).Count
  $report.actualGameCodeFilesOutsidePackage=@($after|Where-Object{$_.root-ne 'installed-package'-and $_.relativePath-match '\.(dll|exe)$'}).Count
  $report.installedClient=@(PublicInventory @($after|Where-Object{$_.relativePath-match 'FortniteClient-Win64-Shipping\.(dll|exe)$'}) $true)
 }
 $report.elapsedSeconds=[int]([DateTime]::UtcNow-$started).TotalSeconds;Save
 # Screenshots stay private and are encrypted by the subsequent evidence step.
 # No plaintext screenshots, raw app logs or proprietary code are uploaded.
}
if($report.failure){throw ('Reference operation failed at '+$report.failure.stage+'; bounded error codes retained')}
