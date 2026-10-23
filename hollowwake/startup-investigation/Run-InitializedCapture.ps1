param([Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$CaptureDirectory,[string]$ExistingInputDirectory,[string]$PrivateInputLeasePath)
$ErrorActionPreference='Stop'
$source=$PSScriptRoot
$repo=Split-Path (Split-Path $source -Parent) -Parent
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$OutputDirectory=(Resolve-Path $OutputDirectory).Path
if(Test-Path $CaptureDirectory){throw 'Fresh private capture directory required'}
& cl.exe /nologo /std:c++17 /EHsc /MD /W4 (Join-Path $source 'NativeStartupProbe.cpp') bcrypt.lib kernel32.lib dbghelp.lib user32.lib /Fe:"$OutputDirectory\NativeStartupProbe.exe" 2>&1|Tee-Object "$OutputDirectory\capture-probe-build.log"
if($LASTEXITCODE){throw 'Native capture probe compilation failed'}
$inputs=if($ExistingInputDirectory){(Resolve-Path $ExistingInputDirectory).Path}else{Join-Path $env:RUNNER_TEMP 'HollowwakeCaptureInputs'}
$rule='Hollowwake-Capture-'+[Guid]::NewGuid().ToString('N')
$previousCryptoPolicy=[Environment]::GetEnvironmentVariable('OPENSSL_ia32cap','Process')
try {
 if(!$previousCryptoPolicy){[Environment]::SetEnvironmentVariable('OPENSSL_ia32cap',':~0x20000000','Process')}
 if(!$ExistingInputDirectory){
 $inputArguments=@();if($PrivateInputLeasePath){$inputArguments=@('--private-lease',$PrivateInputLeasePath)}
 python -u (Join-Path $repo 'hollowwake/native-client-0.5.0/private-package/fetch_inputs.py') (Join-Path $repo 'hollowwake/native-client-0.5.0/App/boot-inputs.json') $inputs @inputArguments 2>&1|Tee-Object "$OutputDirectory\capture-inputs.log"
 if($LASTEXITCODE){throw 'Exact private source acquisition failed; no runtime conclusion'}
 }
 $exe=Join-Path $inputs '6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
 $pin='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original source hash mismatch'}
 New-NetFirewallRule -DisplayName $rule -Direction Outbound -Program $exe -Action Block -Profile Any|Out-Null
 & "$OutputDirectory\NativeStartupProbe.exe" $exe "$OutputDirectory\original-initialization-capture.jsonl" 30 --capture $CaptureDirectory
 if($LASTEXITCODE){throw 'Native initialization capture could not establish bounded execution'}
 $rows=@(Get-Content "$OutputDirectory\original-initialization-capture.jsonl"|ForEach-Object {$_|ConvertFrom-Json})
 if(!($rows|Where-Object {$_.event-eq 'initialized_image_captured'-and $_.error-eq 0})){throw 'Original first-call-return capture boundary not reached with verified initialized comparison state'}
 if(!($rows|Where-Object {$_.event-eq 'original_vector_boundary'-and $_.armed-and $_.pendingCalls-eq 0})-or ($rows|Where-Object {$_.event-in @('original_vector_observation_failed','original_vector_observation_limit')})){throw 'Original exception registration observation is incomplete'}

 $receipt=Get-Content (Join-Path $CaptureDirectory 'capture.json') -Raw|ConvertFrom-Json
 if(!$receipt.captureComplete-or $receipt.originalSha256-ne $pin-or $receipt.phase-ne 'original_entry_first_call_return'-or !$receipt.comparisonEqual-or $receipt.clientChecksPatched-or $receipt.sections.Count-ne 9){throw 'Private initialization receipt incomplete or different scope'}
 if((Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()-ne $pin){throw 'Original source bytes changed during capture'}
 $threadTls=$receipt.threadTlsObservation
 if(!$threadTls-or $threadTls.bytes-ne 28-or $threadTls.nonzeroBytes-lt 0-or $threadTls.nonzeroBytes-gt 28-or $threadTls.differencesFromSourceTemplate-lt 0-or $threadTls.differencesFromSourceTemplate-gt 28-or $threadTls.differencesFromRuntimeTemplate-lt 0-or $threadTls.differencesFromRuntimeTemplate-gt 28-or $threadTls.rawStateCopiedToNewProcess){throw 'Original static TLS observation is incomplete'}
 foreach($name in @('thread-tls.bin','thread-tls-template.bin')){if((Get-Item (Join-Path $CaptureDirectory $name)).Length-ne 28){throw 'Exact private TLS block length differs'}}
 @{scope='Read-only original entry-thread static TLS observation; no state replay';originalSha256=$pin;phase=$receipt.phase;threadTlsObservation=$threadTls;debuggerObserved=$true;xboxTested=$false}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$OutputDirectory\initialization-thread-tls-state.json"
 $bss=$receipt.bssObservation
 if($bss){
  if($bss.rva-ne 0x54d5c00-or $bss.bytes-ne 0x300e78-or $bss.nonzeroAlignedQwords-lt 0-or $bss.ownImageOrDeltaReferences.Count-gt 4096-or $bss.rawStateCopiedToNewProcess){throw 'Unexpected zero-fill observation extent'}
  @{scope='Read-only original first-initialization zero-fill observation; no state replay or runtime fix';originalSha256=$pin;phase=$receipt.phase;bssObservation=$bss;debuggerObserved=$true;xboxTested=$false}|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 "$OutputDirectory\initialization-bss-state.json"
 }

 foreach($section in $receipt.sections){$name=('section-{0:D2}.bin'-f $section.index);if((Get-Item (Join-Path $CaptureDirectory $name)).Length-ne $section.rawBytes){throw 'Private capture section length mismatch'}}
 @{originalInputSha256=$pin;cryptoDispatchDefaultApplied=(!$previousCryptoPolicy);cryptoDispatchDefault=':~0x20000000';nativeWindowsExecution=$true;originalInitializationBoundaryReached=$true;captureComplete=$true;gameStartupChecksPatched=$false;fullGameAssetsStaged=$false;accountCredentialsSupplied=$false;outboundNetworkBlocked=$true;xboxTested=$false;lobbyDemonstrated=$false;plaintextCapturePublic=$false}|ConvertTo-Json|Set-Content -Encoding UTF8 "$OutputDirectory\capture-status.json"
} finally {
 [Environment]::SetEnvironmentVariable('OPENSSL_ia32cap',$previousCryptoPolicy,'Process')
 Remove-NetFirewallRule -DisplayName $rule -ErrorAction SilentlyContinue
 if(!$ExistingInputDirectory){Remove-Item $inputs -Recurse -Force -ErrorAction SilentlyContinue}
}
