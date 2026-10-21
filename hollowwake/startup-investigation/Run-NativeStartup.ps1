param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$ConvertedOnly,[switch]$EvaluateNoEntry,[switch]$OsEntryOnly,[switch]$MainMetadataOnly,[switch]$InitializedOnly)
$ErrorActionPreference='Stop'
$source=$PSScriptRoot
$repo=Split-Path (Split-Path $source -Parent) -Parent
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
& cl.exe /nologo /std:c++17 /EHsc /MD /W4 (Join-Path $source 'NativeStartupProbe.cpp') bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$OutputDirectory\NativeStartupProbe.exe" 2>&1|Tee-Object "$OutputDirectory\probe-build.log"
if($LASTEXITCODE){throw 'Native startup probe compilation failed'}
& cl.exe /nologo /std:c++17 /EHsc /MD (Join-Path $source 'OwnedStartupFixture.cpp') kernel32.lib /Fe:"$OutputDirectory\OwnedStartupFixture.exe" 2>&1|Tee-Object "$OutputDirectory\owned-build.log"
if($LASTEXITCODE){throw 'Owned child fixture compilation failed'}
& "$OutputDirectory\NativeStartupProbe.exe" "$OutputDirectory\OwnedStartupFixture.exe" "$OutputDirectory\owned-child.jsonl" 10 --owned
if($LASTEXITCODE){throw 'Owned child probe failed'}
$owned=@(Get-Content "$OutputDirectory\owned-child.jsonl"|ForEach-Object {$_|ConvertFrom-Json})
if(!($owned|Where-Object {$_.event-eq 'exception'-and $_.code-eq 3221225477-and !$_.firstChance})-or !($owned|Where-Object {$_.event-eq 'process_exit'-and $_.code-eq 3221225477})){throw 'Native owned-child exception/termination evidence absent'}
# Preserve the existing signed application and conversion. This run investigates
# the exact original executable as a main process; it produces no game package.
$inputs=Join-Path $env:RUNNER_TEMP 'HollowwakeStartupInputs'
python -u (Join-Path $repo 'hollowwake/native-client-0.5.0/private-package/fetch_inputs.py') (Join-Path $repo 'hollowwake/native-client-0.5.0/App/boot-inputs.json') $inputs 2>&1|Tee-Object "$OutputDirectory\input-acquisition.log"
if($LASTEXITCODE){throw 'Exact private source input acquisition failed; runtime findings are separate'}
$exe=Join-Path $inputs '6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
$pin='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original client SHA-256 mismatch'}
# Network denial is per executable. The child receives no CI environment/token,
# no account/password/ticket, and a kill-on-close, one-process, 2GiB job.
# Do not fall back to allowing networking if this operation is unavailable.
$rule='Hollowwake-Startup-'+[Guid]::NewGuid().ToString('N')
$hostRule=$rule+'-host'
$mainRule=$rule+'-main'
$mainRoot=Join-Path $env:RUNNER_TEMP 'HollowwakeMainImageInputs'
try {
 New-NetFirewallRule -DisplayName $rule -Direction Outbound -Program $exe -Action Block -Profile Any|Out-Null
 if(!$ConvertedOnly){
  & "$OutputDirectory\NativeStartupProbe.exe" $exe "$OutputDirectory\original-client-native.jsonl" 30
  if($LASTEXITCODE){throw 'Native startup probe could not establish bounded execution'}
  & "$OutputDirectory\NativeStartupProbe.exe" $exe "$OutputDirectory\original-client-plain.jsonl" 30 --plain
  if($LASTEXITCODE){throw 'Native startup without debugger could not establish bounded execution'}
 }
 if($InitializedOnly){
  python -m pip install --disable-pip-version-check pefile==2024.8.26
  if($LASTEXITCODE){throw 'Pinned PE dependency unavailable'}
  $capture=Join-Path $inputs 'initialized-private'
  & "$OutputDirectory\NativeStartupProbe.exe" $exe "$OutputDirectory\normal-original-capture.jsonl" 30 --capture $capture
  if($LASTEXITCODE){throw 'Normal original initialization capture failed'}
  Copy-Item $inputs $mainRoot -Recurse
  # Remove the copied capture immediately; no derived runtime image is public.
  Remove-Item (Join-Path $mainRoot 'initialized-private') -Recurse -Force
  $mainExe=Join-Path $mainRoot '6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
  Remove-Item $mainExe -Force
  python (Join-Path $source 'prepare_initialized_image.py') $exe $capture $mainExe "$OutputDirectory\initialized-preparation.json"
  if($LASTEXITCODE){throw 'Private initialized-image normalization rejected'}
  $permissionReceipt=Get-Content (Join-Path $capture 'capture.json') -Raw|ConvertFrom-Json
  if(!$permissionReceipt.protections-or !$permissionReceipt.protections.Count){throw 'Measured original page permissions absent'}
  $permissionLines=@('struct CapturedProtection {DWORD rva;SIZE_T bytes;DWORD protect;};','static constexpr CapturedProtection kCapturedProtections[]={')
  foreach($region in $permissionReceipt.protections){
   if(!$region.bytes-or $region.rva-ge 0x6314000-or $region.bytes-gt (0x6314000-$region.rva)-or $region.protect-notin @(1,2,4,8,16,32,64,128)){throw 'Unsupported measured image page permission'}
   $permissionLines+=('{'+$region.rva+','+$region.bytes+','+$region.protect+'},')
  }
  $permissionLines+='};';$permissionLines|Set-Content -Encoding ASCII "$OutputDirectory\CapturedProtections.generated.h"
  $preparedSha=(Get-FileHash $mainExe -Algorithm SHA256).Hash.ToLowerInvariant()
  ('constexpr char kMainImageSha[]="'+$preparedSha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\MainPin.generated.h"
  & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DMAIN_PIN_GENERATED /I"$OutputDirectory" (Join-Path $source 'NativeStartupProbe.cpp') bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$OutputDirectory\NativeStartupProbe.exe" 2>&1|Tee-Object "$OutputDirectory\initialized-probe-build.log"
  if($LASTEXITCODE){throw 'Private initialized-image probe build failed'}
  New-NetFirewallRule -DisplayName $mainRule -Direction Outbound -Program $mainExe -Action Block -Profile Any|Out-Null
  & "$OutputDirectory\NativeStartupProbe.exe" $mainExe "$OutputDirectory\initialized-main.jsonl" 30 --main-image
  if($LASTEXITCODE){throw 'Private initialized main image execution not established'}
  $converted=Join-Path (Split-Path $mainExe -Parent) 'Fortnite621.baseline.private.dll'
  python (Join-Path $repo 'hollowwake/runtime-native-route/native-image/convert_image.py') $mainExe $converted --expected-sha256 $preparedSha --report "$OutputDirectory\initialized-conversion.json"
  if($LASTEXITCODE){throw 'Initialized-image existing converter rejected input'}
  $convertedSha=(Get-FileHash $converted -Algorithm SHA256).Hash.ToLowerInvariant()
  ('static const char kBaselinePin[]="'+$convertedSha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\BaselinePin.generated.h"
  $hostExe=Join-Path (Split-Path $mainExe -Parent) 'ConvertedStartupHost.exe'
  New-NetFirewallRule -DisplayName $hostRule -Direction Outbound -Program $hostExe -Action Block -Profile Any|Out-Null
  $native=Join-Path $repo 'hollowwake/native-client-0.5.0/Native'
  $route=Join-Path $repo 'hollowwake/runtime-native-route'
  foreach($identity in @($false,$true)){
   $flags=@('/nologo','/std:c++17','/EHsc','/MD','/D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR','/DUSE_OS_ENTRY_THREAD',('/I'+$OutputDirectory))
   $flags+='/DUSE_CAPTURED_PROTECTIONS'
   $sources=@((Join-Path $source 'ConvertedStartupHost.cpp'));$label='initialized-dll'
   if($identity){
    $label='initialized-dll-identity';$flags+=@('/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/DUSE_PRODUCTION_IDENTITY',('/I'+$native))
    $sources+=@((Join-Path $native 'PackagedNamespace.cpp'),(Join-Path $native 'StartupImport.cpp'),(Join-Path $route 'NativeSystem.cpp'),(Join-Path $route 'NativeProxyDispatch.cpp'))
   }
   & cl.exe @flags @sources bcrypt.lib kernel32.lib windowsapp.lib runtimeobject.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\$label-build.log"
   if($LASTEXITCODE){throw ('Initialized host build failed: '+$label)}
   & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\$label.jsonl" 30 --converted
   if($LASTEXITCODE){throw ('Initialized DLL execution not established: '+$label)}
  }
  @{nativeWindowsExecution=$true;normalOriginalInitializationCaptured=$true;entryCallRetained=$true;checksPatched=$false;uncapturedBss=$true;fullGameAssetsStaged=$false;accountCredentialsSupplied=$false;xboxTested=$false;lobbyDemonstrated=$false;existingSignedPackagesChanged=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\scope.json"
  return
 }
 if($MainMetadataOnly){
  Copy-Item $inputs $mainRoot -Recurse
  $mainExe=Join-Path $mainRoot '6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
  New-NetFirewallRule -DisplayName $mainRule -Direction Outbound -Program $mainExe -Action Block -Profile Any|Out-Null
  foreach($variant in @('certificate','converted-original-entry')){
   Remove-Item $mainExe -Force
   python (Join-Path $source 'prepare_metadata_comparison.py') $exe $mainExe "$OutputDirectory\$variant-preparation.json" --variant $variant
   if($LASTEXITCODE){throw ('Metadata comparison rejected: '+$variant)}
   $sha=(Get-FileHash $mainExe -Algorithm SHA256).Hash.ToLowerInvariant()
   ('constexpr char kMainImageSha[]="'+$sha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\MainPin.generated.h"
   & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DMAIN_PIN_GENERATED /I"$OutputDirectory" (Join-Path $source 'NativeStartupProbe.cpp') bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$OutputDirectory\NativeStartupProbe.exe" 2>&1|Tee-Object "$OutputDirectory\$variant-probe-build.log"
   if($LASTEXITCODE){throw 'Pinned metadata probe compilation failed'}
   & "$OutputDirectory\NativeStartupProbe.exe" $mainExe "$OutputDirectory\main-$variant.jsonl" 30 --main-image
   if($LASTEXITCODE){throw ('Metadata comparison execution not established: '+$variant)}
  }
  @{nativeWindowsExecution=$true;originalOsEntryRetained=$true;originalSectionBytesUnchanged=$true;checksPatched=$false;xboxTested=$false;lobbyDemonstrated=$false;existingSignedPackagesChanged=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\scope.json"
  return
 }
 # Reuse the exact existing conversion implementation, with NO import rewrite,
 # service-address changes, pointer repair or substitute APIs. Real desktop OS
 # dependencies isolate whether the conversion itself changes initial startup.
 $converted=Join-Path (Split-Path $exe -Parent) 'Fortnite621.baseline.private.dll'
 python (Join-Path $repo 'hollowwake/runtime-native-route/native-image/convert_image.py') $exe $converted --expected-sha256 $pin --report "$OutputDirectory\baseline-conversion.json"
 if($LASTEXITCODE){throw 'Exact private baseline conversion failed'}
 $convertedSha=(Get-FileHash $converted -Algorithm SHA256).Hash.ToLowerInvariant()
 ('static const char kBaselinePin[]="'+$convertedSha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\BaselinePin.generated.h"
 $hostExe=Join-Path (Split-Path $exe -Parent) 'ConvertedStartupHost.exe'
 if($OsEntryOnly){
  New-NetFirewallRule -DisplayName $hostRule -Direction Outbound -Program $hostExe -Action Block -Profile Any|Out-Null
  $native=Join-Path $repo 'hollowwake/native-client-0.5.0/Native'
  $route=Join-Path $repo 'hollowwake/runtime-native-route'
  foreach($identity in @($false,$true)){
   $flags=@('/nologo','/std:c++17','/EHsc','/MD','/D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR','/DUSE_OS_ENTRY_THREAD',('/I'+$OutputDirectory))
   $sources=@((Join-Path $source 'ConvertedStartupHost.cpp'))
   $label='os-entry'
   if($identity){
    $label='os-entry-identity'
    $flags+=@('/DWIN32_LEAN_AND_MEAN','/DNOMINMAX','/DUSE_PRODUCTION_IDENTITY',('/I'+$native))
    $sources+=@((Join-Path $native 'PackagedNamespace.cpp'),(Join-Path $native 'StartupImport.cpp'),(Join-Path $route 'NativeSystem.cpp'),(Join-Path $route 'NativeProxyDispatch.cpp'))
   }
   & cl.exe @flags @sources bcrypt.lib kernel32.lib windowsapp.lib runtimeobject.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\$label-build.log"
   if($LASTEXITCODE){throw ('Direct OS entry host compilation failed: '+$label)}
   & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-$label.jsonl" 30 --converted
   if($LASTEXITCODE){throw ('Direct OS entry execution could not be established: '+$label)}
   $rows=@(Get-Content "$OutputDirectory\converted-client-$label.jsonl"|ForEach-Object {$_|ConvertFrom-Json})
   if(!($rows|Where-Object {$_.event-eq 'process_created'})){throw 'No real child process was created'}
  }
  @{inputSha256=$pin;nativeWindowsExecution=$true;directOsThreadEntry=$true;productionIdentityComparison=$true;softwareInstructionPatches=$false;outboundNetworkBlocked=$true;fullGameAssetsStaged=$false;xboxTested=$false;lobbyDemonstrated=$false;existingSignedPackagesChanged=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\scope.json"
  return
 }
 & cl.exe /nologo /std:c++17 /EHsc /MD /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR /I"$OutputDirectory" (Join-Path $source 'ConvertedStartupHost.cpp') bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\converted-host-build.log"
 if($LASTEXITCODE){throw 'Converted private baseline host compilation failed'}
 New-NetFirewallRule -DisplayName $hostRule -Direction Outbound -Program $hostExe -Action Block -Profile Any|Out-Null
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-native.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Converted native baseline could not establish bounded execution'}
 & ml64.exe /nologo /c /Fo"$OutputDirectory\NativeEntryContext.obj" (Join-Path $source 'NativeEntryContext.asm')
 if($LASTEXITCODE){throw 'Observed native entry handoff assembly failed'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR /DUSE_NATIVE_ENTRY_CONTEXT /I"$OutputDirectory" (Join-Path $source 'ConvertedStartupHost.cpp') "$OutputDirectory\NativeEntryContext.obj" bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\converted-context-build.log"
 if($LASTEXITCODE){throw 'Measured native-entry-context host compilation failed'}
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-context.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Measured native-entry-context startup could not establish bounded execution'}
 $native=Join-Path $repo 'hollowwake/native-client-0.5.0/Native'
 $route=Join-Path $repo 'hollowwake/runtime-native-route'
 & cl.exe /nologo /std:c++17 /EHsc /MD /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUSE_PRODUCTION_IDENTITY /DUSE_NATIVE_ENTRY_CONTEXT /I"$OutputDirectory" /I"$native" (Join-Path $source 'ConvertedStartupHost.cpp') "$OutputDirectory\NativeEntryContext.obj" (Join-Path $native 'PackagedNamespace.cpp') (Join-Path $native 'StartupImport.cpp') (Join-Path $route 'NativeSystem.cpp') (Join-Path $route 'NativeProxyDispatch.cpp') bcrypt.lib kernel32.lib windowsapp.lib runtimeobject.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\converted-identity-build.log"
 if($LASTEXITCODE){throw 'Production identity baseline host compilation failed'}
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-identity.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Production identity startup baseline could not establish bounded execution'}
 # A main-image comparison keeps the original executable name/path structure.
 # Only a private copy is changed; no original bytes/checks are rewritten.
 Copy-Item $inputs $mainRoot -Recurse
 $mainExe=Join-Path $mainRoot '6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
 Remove-Item $mainExe -Force
 python (Join-Path $source 'prepare_main_image.py') $exe $mainExe "$OutputDirectory\main-image-preparation.json"
 if($LASTEXITCODE){throw 'Bounded main-image comparison preparation failed'}
 New-NetFirewallRule -DisplayName $mainRule -Direction Outbound -Program $mainExe -Action Block -Profile Any|Out-Null
 & "$OutputDirectory\NativeStartupProbe.exe" $mainExe "$OutputDirectory\client-main-image.jsonl" 30 --main-image
 if($LASTEXITCODE){throw 'Main-image comparison failed to establish bounded execution'}
 python -m pip install --disable-pip-version-check pefile==2024.8.26
 if($LASTEXITCODE){throw 'Pinned PE analysis dependency unavailable'}
 if($EvaluateNoEntry){
 & cl.exe /nologo /std:c++17 /EHsc /MD /LD (Join-Path $source 'OwnedNoEntryTls.cpp') /link /NOENTRY /OUT:"$OutputDirectory\OwnedNoEntryTls.dll" 2>&1|Tee-Object "$OutputDirectory\noentry-tls-build.log"
 if($LASTEXITCODE){throw 'Owned no-entry static TLS build failed'}
 & cl.exe /nologo /std:c++17 /EHsc /MD (Join-Path $source 'OwnedNoEntryTlsTest.cpp') kernel32.lib /Fe:"$OutputDirectory\OwnedNoEntryTlsTest.exe" 2>&1|Tee-Object "$OutputDirectory\noentry-host-build.log"
 if($LASTEXITCODE){throw 'Owned no-entry TLS host build failed'}
 & "$OutputDirectory\OwnedNoEntryTlsTest.exe" "$OutputDirectory\OwnedNoEntryTls.dll" 2>&1|Tee-Object "$OutputDirectory\noentry-tls-test.log"
 if($LASTEXITCODE){throw ('OS no-entry DLL static TLS behavior failed, exit='+$LASTEXITCODE)}
 Remove-Item $converted -Force
 python (Join-Path $source 'prepare_noentry.py') $exe $converted "$OutputDirectory\noentry-preparation.json"
 if($LASTEXITCODE){throw 'Section-preserving no-entry preparation failed'}
 $convertedSha=(Get-FileHash $converted -Algorithm SHA256).Hash.ToLowerInvariant()
 ('static const char kBaselinePin[]="'+$convertedSha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\BaselinePin.generated.h"
 & cl.exe /nologo /std:c++17 /EHsc /MD /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR /DUSE_NATIVE_ENTRY_CONTEXT /DNO_ENTRY_CASE /I"$OutputDirectory" (Join-Path $source 'ConvertedStartupHost.cpp') "$OutputDirectory\NativeEntryContext.obj" bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\noentry-client-host-build.log"
 if($LASTEXITCODE){throw 'No-entry client host build failed'}
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-noentry.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'No-entry client startup did not establish bounded execution'}
 }
 # No-entry was rejected by the owned TLS test in run 39655659302. The selected
 # comparison retains a real DLL entry and all original section extents.
 Remove-Item $converted -Force
 python (Join-Path $source 'prepare_new_section.py') $exe $converted "$OutputDirectory\new-section-preparation.json"
 if($LASTEXITCODE){throw 'Section-preserving real-entry conversion failed'}
 $convertedSha=(Get-FileHash $converted -Algorithm SHA256).Hash.ToLowerInvariant()
 ('static const char kBaselinePin[]="'+$convertedSha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\BaselinePin.generated.h"
 & cl.exe /nologo /std:c++17 /EHsc /MD /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR /DUSE_NATIVE_ENTRY_CONTEXT /I"$OutputDirectory" (Join-Path $source 'ConvertedStartupHost.cpp') "$OutputDirectory\NativeEntryContext.obj" bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\new-section-client-host-build.log"
 if($LASTEXITCODE){throw 'Section-preserving client host build failed'}
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-new-section.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Section-preserving client did not establish bounded execution'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUSE_PRODUCTION_IDENTITY /DUSE_NATIVE_ENTRY_CONTEXT /I"$OutputDirectory" /I"$native" (Join-Path $source 'ConvertedStartupHost.cpp') "$OutputDirectory\NativeEntryContext.obj" (Join-Path $native 'PackagedNamespace.cpp') (Join-Path $native 'StartupImport.cpp') (Join-Path $route 'NativeSystem.cpp') (Join-Path $route 'NativeProxyDispatch.cpp') bcrypt.lib kernel32.lib windowsapp.lib runtimeobject.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\new-section-identity-build.log"
 if($LASTEXITCODE){throw 'Section-preserving identity host compilation failed'}
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-new-section-identity.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Section-preserving identity did not establish bounded execution'}
 $checkFile=if($ConvertedOnly){'converted-client-native.jsonl'}else{'original-client-native.jsonl'}
 $rows=@(Get-Content (Join-Path $OutputDirectory $checkFile)|ForEach-Object {$_|ConvertFrom-Json})
 if(!($rows|Where-Object {$_.event-eq 'input_verified'})-or !($rows|Where-Object {$_.event-eq 'process_created'})){throw 'Real original process was not created'}
 @{
  inputSha256=$pin;originalExecutableUnmodified=$true;originalExecutedThisRun=(!$ConvertedOnly)
  debuggerAttached=$true;hardwareExecutionBreakpoints=$true;softwarePatches=$false
  nativeWindowsExecution=$true;childEnvironmentInherited=$false;outboundNetworkBlocked=$true
  separateConvertedBaselineAttempted=$true;integratedXboxLauncherExecuted=$false
  noEntryRouteSelected=[bool]$EvaluateNoEntry;sectionPreservingRealEntryAttempted=$true
  fullGameAssetsStaged=$false;accountCredentialsSupplied=$false;originalLoadingScreenProven=$false
  xboxTested=$false;lobbyDemonstrated=$false;existingSignedPackagesChanged=$false
 }|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\scope.json"
} finally {
 Remove-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue
 Remove-NetFirewallRule -DisplayName $hostRule -ErrorAction SilentlyContinue
 Remove-NetFirewallRule -DisplayName $mainRule -ErrorAction SilentlyContinue
 Remove-Item $mainRoot -Recurse -Force -ErrorAction SilentlyContinue
 Remove-Item $inputs -Recurse -Force -ErrorAction SilentlyContinue
}
