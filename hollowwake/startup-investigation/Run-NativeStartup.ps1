param([Parameter(Mandatory=$true)][string]$OutputDirectory,[switch]$ConvertedOnly)
$ErrorActionPreference='Stop'
$source=$PSScriptRoot
$repo=Split-Path (Split-Path $source -Parent) -Parent
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
& cl.exe /nologo /std:c++17 /EHsc /MD /W4 (Join-Path $source 'NativeStartupProbe.cpp') bcrypt.lib kernel32.lib /Fe:"$OutputDirectory\NativeStartupProbe.exe" 2>&1|Tee-Object "$OutputDirectory\probe-build.log"
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
 # Reuse the exact existing conversion implementation, with NO import rewrite,
 # service-address changes, pointer repair or substitute APIs. Real desktop OS
 # dependencies isolate whether the conversion itself changes initial startup.
 $converted=Join-Path (Split-Path $exe -Parent) 'Fortnite621.baseline.private.dll'
 python (Join-Path $repo 'hollowwake/runtime-native-route/native-image/convert_image.py') $exe $converted --expected-sha256 $pin --report "$OutputDirectory\baseline-conversion.json"
 if($LASTEXITCODE){throw 'Exact private baseline conversion failed'}
 $convertedSha=(Get-FileHash $converted -Algorithm SHA256).Hash.ToLowerInvariant()
 ('static const char kBaselinePin[]="'+$convertedSha+'";')|Set-Content -Encoding ASCII "$OutputDirectory\BaselinePin.generated.h"
 $hostExe=Join-Path (Split-Path $exe -Parent) 'ConvertedStartupHost.exe'
 & cl.exe /nologo /std:c++17 /EHsc /MD /I"$OutputDirectory" (Join-Path $source 'ConvertedStartupHost.cpp') bcrypt.lib kernel32.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\converted-host-build.log"
 if($LASTEXITCODE){throw 'Converted private baseline host compilation failed'}
 New-NetFirewallRule -DisplayName $hostRule -Direction Outbound -Program $hostExe -Action Block -Profile Any|Out-Null
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-native.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Converted native baseline could not establish bounded execution'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /DUSE_NATIVE_ENTRY_CONTEXT /I"$OutputDirectory" (Join-Path $source 'ConvertedStartupHost.cpp') bcrypt.lib kernel32.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\converted-context-build.log"
 if($LASTEXITCODE){throw 'Measured native-entry-context host compilation failed'}
 & "$OutputDirectory\NativeStartupProbe.exe" $hostExe "$OutputDirectory\converted-client-context.jsonl" 30 --converted
 if($LASTEXITCODE){throw 'Measured native-entry-context startup could not establish bounded execution'}
 $native=Join-Path $repo 'hollowwake/native-client-0.5.0/Native'
 $route=Join-Path $repo 'hollowwake/runtime-native-route'
 & cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DUSE_PRODUCTION_IDENTITY /I"$OutputDirectory" /I"$native" (Join-Path $source 'ConvertedStartupHost.cpp') (Join-Path $native 'PackagedNamespace.cpp') (Join-Path $native 'StartupImport.cpp') (Join-Path $route 'NativeSystem.cpp') (Join-Path $route 'NativeProxyDispatch.cpp') bcrypt.lib kernel32.lib windowsapp.lib runtimeobject.lib /Fe:"$hostExe" 2>&1|Tee-Object "$OutputDirectory\converted-identity-build.log"
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
 $checkFile=if($ConvertedOnly){'converted-client-native.jsonl'}else{'original-client-native.jsonl'}
 $rows=@(Get-Content (Join-Path $OutputDirectory $checkFile)|ForEach-Object {$_|ConvertFrom-Json})
 if(!($rows|Where-Object {$_.event-eq 'input_verified'})-or !($rows|Where-Object {$_.event-eq 'process_created'})){throw 'Real original process was not created'}
 @{
  inputSha256=$pin;originalExecutableUnmodified=$true;originalExecutedThisRun=(!$ConvertedOnly)
  debuggerAttached=$true;hardwareExecutionBreakpoints=$true;softwarePatches=$false
  nativeWindowsExecution=$true;childEnvironmentInherited=$false;outboundNetworkBlocked=$true
  separateConvertedBaselineAttempted=$true;integratedXboxLauncherExecuted=$false
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
