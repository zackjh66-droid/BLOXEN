param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$CaptureDirectory)
$ErrorActionPreference='Stop'
$source=$PSScriptRoot
$repo=Split-Path (Split-Path $source -Parent) -Parent
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
if(Test-Path $CaptureDirectory){throw 'Fresh private capture directory required'}
& cl.exe /nologo /std:c++17 /EHsc /MD /W4 (Join-Path $source 'NativeStartupProbe.cpp') bcrypt.lib kernel32.lib dbghelp.lib /Fe:"$OutputDirectory\NativeStartupProbe.exe" 2>&1|Tee-Object "$OutputDirectory\capture-probe-build.log"
if($LASTEXITCODE){throw 'Native capture probe compilation failed'}
$inputs=Join-Path $env:RUNNER_TEMP 'HollowwakeCaptureInputs'
$rule='Hollowwake-Capture-'+[Guid]::NewGuid().ToString('N')
try {
 python -u (Join-Path $repo 'hollowwake/native-client-0.5.0/private-package/fetch_inputs.py') (Join-Path $repo 'hollowwake/native-client-0.5.0/App/boot-inputs.json') $inputs 2>&1|Tee-Object "$OutputDirectory\capture-inputs.log"
 if($LASTEXITCODE){throw 'Exact private source acquisition failed; no runtime conclusion'}
 $exe=Join-Path $inputs '6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
 $pin='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original source hash mismatch'}
 New-NetFirewallRule -DisplayName $rule -Direction Outbound -Program $exe -Action Block -Profile Any|Out-Null
 & "$OutputDirectory\NativeStartupProbe.exe" $exe "$OutputDirectory\original-initialization-capture.jsonl" 30 --capture $CaptureDirectory
 if($LASTEXITCODE){throw 'Native initialization capture could not establish bounded execution'}
 $rows=@(Get-Content "$OutputDirectory\original-initialization-capture.jsonl"|ForEach-Object {$_|ConvertFrom-Json})
 if(!($rows|Where-Object {$_.event-eq 'initialized_image_captured'-and $_.error-eq 0})){throw 'Original first-call-return capture boundary not reached with verified initialized comparison state'}
 $receipt=Get-Content (Join-Path $CaptureDirectory 'capture.json') -Raw|ConvertFrom-Json
 if(!$receipt.captureComplete-or $receipt.originalSha256-ne $pin-or $receipt.phase-ne 'original_entry_first_call_return'-or !$receipt.comparisonEqual-or $receipt.clientChecksPatched-or $receipt.sections.Count-ne 9){throw 'Private initialization receipt incomplete or different scope'}
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original source bytes changed during capture'}
 foreach($section in $receipt.sections){$name=('section-{0:D2}.bin'-f $section.index);if((Get-Item (Join-Path $CaptureDirectory $name)).Length-ne $section.rawBytes){throw 'Private capture section length mismatch'}}
 @{originalInputSha256=$pin;nativeWindowsExecution=$true;originalInitializationBoundaryReached=$true;captureComplete=$true;gameStartupChecksPatched=$false;fullGameAssetsStaged=$false;accountCredentialsSupplied=$false;outboundNetworkBlocked=$true;xboxTested=$false;lobbyDemonstrated=$false;plaintextCapturePublic=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\capture-status.json"
} finally {
 Remove-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue
 Remove-Item $inputs -Recurse -Force -ErrorAction SilentlyContinue
}
