param([Parameter(Mandatory=$true)][string]$EvidenceDirectory)
$ErrorActionPreference='Stop'
$source=Split-Path $PSScriptRoot -Parent
New-Item -ItemType Directory -Force $EvidenceDirectory|Out-Null
$drives=@(Get-PSDrive -PSProvider FileSystem|Where-Object {$_.Name-match '^[A-Z]$'}|Sort-Object Free -Descending)
$drives|Select-Object Name,Free,Used|ConvertTo-Json|Set-Content (Join-Path $EvidenceDirectory 'full-content-capacity.json')
$drive=$drives|Where-Object {$_.Free-ge 44GB}|Select-Object -First 1
if(!$drive){throw 'FULL_CONTENT_BLOCKED: no hosted filesystem has the required 44 GiB staging/packaging headroom; see full-content-capacity.json'}
$work=Join-Path $drive.Root ('HollowwakePrivateFull-'+[Guid]::NewGuid().ToString('N'))
@{scope='Disposable hosted private inputs; never execute game images';work=$work;preservedOriginalArchiveModified=$false}|ConvertTo-Json|Set-Content (Join-Path $EvidenceDirectory 'full-content-attempt.json')
python -u (Join-Path $PSScriptRoot 'fetch_full_content.py') $work (Join-Path $source 'App\ContentPlan621.json') (Join-Path $source 'App\boot-inputs.json')
if($LASTEXITCODE){throw 'FULL_CONTENT_BLOCKED: complete source/content preparation failed; no full-content package exists'}
Copy-Item (Join-Path $work 'full-input-audit.json') $EvidenceDirectory
python -m pip install --disable-pip-version-check pefile==2024.8.26
if($LASTEXITCODE){throw 'Maintained PE metadata dependency unavailable'}
python (Join-Path $PSScriptRoot 'prepare.py') (Join-Path $work 'OriginalCode\6.21') (Join-Path $work 'PreparedClient')
if($LASTEXITCODE){throw 'Pinned real-client dependency preparation failed'}
& (Join-Path $source 'Build.ps1') -OutputDirectory (Join-Path $work 'CodePackage') -PrivatePreparedDirectory (Join-Path $work 'PreparedClient')
if($LASTEXITCODE){throw 'Real-client native/code package compilation or signing failed'}
$code=@(Get-ChildItem (Join-Path $work 'CodePackage') -Recurse -File|Where-Object {$_.Name-like 'Hollowwake_*'-and $_.Extension-in @('.appx','.msix')})
if($code.Count-ne 1){throw 'Expected exactly one compiled native game code package'}
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
$layout=Join-Path $work 'CompiledLayout';$content=Join-Path $work 'FullContent';$output=Join-Path $work 'FullPackage'
New-Item -ItemType Directory $output|Out-Null
& (Join-Path $sdk 'makeappx.exe') unpack /p $code[0].FullName /d $layout /o
if($LASTEXITCODE){throw 'Compiled package unpack failed'}
$map=Join-Path $work 'full-content-map.txt'
python (Join-Path $PSScriptRoot 'full_mapping.py') mapping $layout $content (Join-Path $source 'App\ContentPlan621.json') $map
if($LASTEXITCODE){throw 'Full content mapping validation failed'}
$msix=Join-Path $output 'Hollowwake_0.5.2.0_x64.msix'
# Mapping avoids copying 26 GB of game data into MSBuild staging/layout again.
& (Join-Path $sdk 'makeappx.exe') pack /f $map /p $msix /h SHA256 /o
if($LASTEXITCODE){throw 'Full private MSIX packing failed'}
$certificate=Join-Path $work 'CodePackage\HollowwakeExperiment.cer'
$public=Get-PfxCertificate -FilePath $certificate
& (Join-Path $sdk 'signtool.exe') sign /s My /sha1 $public.Thumbprint /fd SHA256 $msix
if($LASTEXITCODE){throw 'Full private MSIX development signing failed'}
& (Join-Path $sdk 'signtool.exe') verify /pa /v $msix 2>&1|Tee-Object (Join-Path $EvidenceDirectory 'full-msix-signature.log')
if($LASTEXITCODE){throw 'Full private MSIX signature trust check failed'}
python (Join-Path $PSScriptRoot 'full_mapping.py') audit $msix $content (Join-Path $source 'App\ContentPlan621.json') (Join-Path $work 'PreparedClient\plan.json') (Join-Path $output 'full-package-audit.json')
if($LASTEXITCODE){throw 'Full private MSIX final archive/file hashes failed'}
Copy-Item (Join-Path $output 'full-package-audit.json') $EvidenceDirectory
Copy-Item $certificate $output
$dependencyDirectory=Join-Path $output 'Dependencies\x64';New-Item -ItemType Directory -Force $dependencyDirectory|Out-Null
foreach($dependency in Get-ChildItem (Join-Path $work 'CodePackage') -Recurse -Filter '*.appx'|Where-Object {$_.FullName-match '[\\/]Dependencies[\\/]x64[\\/]'}){
 $destination=Join-Path $dependencyDirectory $dependency.Name
 if(Test-Path $destination){if((Get-FileHash $destination).Hash-ne (Get-FileHash $dependency.FullName).Hash){throw 'Dependency name/hash collision'}}else{Copy-Item $dependency.FullName $destination}
}
@'
HOLLOWWAKE 0.5.2 — FULL-CONTENT EXPERIMENT, NOT A CONFIRMED FORTNITE PORT

This signed x64 MSIX includes the actual converted 6.21-CL-4526925 client,
native dependencies/compatibility DLLs and 139 pinned game-content files.
No boot import or in-app game-content download is required for this build.
Keep the original ZIP and working companion/rollback unchanged.

Xbox Device Portal > Apps > Add: select Hollowwake_0.5.2.0_x64.msix and
the included Dependencies/x64 packages if requested. The CER is a public
development certificate. No private signing key is included.

If the console exposes Dev Home > highlight app > View > View details >
App type > Game, that changes resource allocation; it does not implement
unsupported desktop APIs. Console labels/behavior remain hardware-unverified.

IMPORTANT: the existing Xbox report has 42 unredirected platform bindings
missing. This full-content packaging does not establish their availability.
Do not expect a lobby based on successful installation or account linking.
The runtime retains preflight checks and stops on missing requirements.
No original client entry/loading screen/lobby has been demonstrated here.
Native Fortnite login and presence remain incomplete/unverified.
Matchmaking is unavailable. Never enter Epic credentials.

Use Show launch log > Missing imports or Full log for existing diagnostics.
Device Portal file access can retrieve LocalState/native-launch.log,
native-imports-unresolved.txt and native-platform-imports.json, plus
LocalState/managed-launch.log. Game logs, if created, are under
LocalState/GameClean/6.21/FortniteGame/Saved/Logs.
'@|Set-Content -Encoding UTF8 (Join-Path $output 'INSTALL.txt')
@{result='Full private MSIX built and verified; not Xbox/lobby validation';outputDirectory=$output;fortniteExecuted=$false;entryReached=$false;lobbyReached=$false;xboxTested=$false;knownXboxPlatformMissing=42;presenceAvailable=$false;matchmakingAvailable=$false}|ConvertTo-Json|Set-Content (Join-Path $EvidenceDirectory 'full-content-result.json')
Remove-Item -LiteralPath $content -Recurse -Force
# Do not upload plaintext game files or private keys. Large encrypted export is
# a separate, explicit delivery operation; public CI artifacts contain evidence only.
