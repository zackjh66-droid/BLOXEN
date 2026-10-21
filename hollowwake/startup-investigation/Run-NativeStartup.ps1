param([Parameter(Mandatory=$true)][string]$OutputDirectory)
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
try {
 New-NetFirewallRule -DisplayName $rule -Direction Outbound -Program $exe -Action Block -Profile Any|Out-Null
 & "$OutputDirectory\NativeStartupProbe.exe" $exe "$OutputDirectory\original-client-native.jsonl" 30
 if($LASTEXITCODE){throw 'Native startup probe could not establish bounded execution'}
 $rows=@(Get-Content "$OutputDirectory\original-client-native.jsonl"|ForEach-Object {$_|ConvertFrom-Json})
 if(!($rows|Where-Object {$_.event-eq 'input_verified'})-or !($rows|Where-Object {$_.event-eq 'process_created'})){throw 'Real original process was not created'}
 @{
  inputSha256=$pin;originalExecutableUnmodified=$true;executableConverted=$false
  debuggerAttached=$true;hardwareExecutionBreakpoints=$true;softwarePatches=$false
  nativeWindowsExecution=$true;childEnvironmentInherited=$false;outboundNetworkBlocked=$true
  fullGameAssetsStaged=$false;accountCredentialsSupplied=$false;originalLoadingScreenProven=$false
  xboxTested=$false;lobbyDemonstrated=$false;existingSignedPackagesChanged=$false
 }|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\scope.json"
} finally {
 Remove-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue
 Remove-Item $inputs -Recurse -Force -ErrorAction SilentlyContinue
}
