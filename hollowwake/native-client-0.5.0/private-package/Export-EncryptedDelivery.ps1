# Run with PowerShell 7/.NET in the hosted runner, never archive-supplied code.
# Only recipient-encrypted files leave the private build directory.
param([Parameter(Mandatory=$true)][string]$InputDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$RecipientPublicKey)
$ErrorActionPreference='Stop'
$auditPath=Join-Path $InputDirectory 'private-package-audit.json'
$audit=Get-Content $auditPath -Raw|ConvertFrom-Json
if($audit.result-ne 'PASS private signed APPX contents match prepared hashes'-or $audit.images-lt 2-or $audit.publiclyUploaded){throw 'Private package audit does not permit encrypted delivery'}
$packages=@(Get-ChildItem $InputDirectory -Recurse -File|Where-Object {$_.Name-like 'Hollowwake_*'-and $_.Extension-in @('.appx','.msix')})
if($packages.Count-ne 1){throw 'Expected one private game-containing APPX'}
$stage=Join-Path $env:RUNNER_TEMP ('HollowwakeRecipientStage-'+[Guid]::NewGuid().ToString('N'))
$archive=$stage+'.zip'
New-Item -ItemType Directory -Path $stage|Out-Null
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
try{
 Copy-Item $packages[0].FullName (Join-Path $stage $packages[0].Name)
 Copy-Item (Join-Path $InputDirectory 'HollowwakeExperiment.cer') (Join-Path $stage 'HollowwakeExperiment.cer')
 Copy-Item $auditPath (Join-Path $stage 'private-package-audit.json')
 $dependencyRoot=Join-Path $stage 'Dependencies/x64';New-Item -ItemType Directory -Force $dependencyRoot|Out-Null
 foreach($file in Get-ChildItem $InputDirectory -Recurse -File -Filter '*.appx'|Where-Object {$_.FullName-match '[\\/]Dependencies[\\/]x64[\\/]'} ){
  $target=Join-Path $dependencyRoot $file.Name
  if(Test-Path $target){if((Get-FileHash $target).Hash-ne (Get-FileHash $file.FullName).Hash){throw 'Conflicting dependency names'}}else{Copy-Item $file.FullName $target}
 }
 $instructions=@'
HOLLOWWAKE EXPERIMENTAL NATIVE CLIENT PACKAGE

This includes the converted original Season 6 client and native dependencies.
It has NOT been executed on Xbox or confirmed to reach an original loading
screen/lobby. Website linking is separate from Fortnite authentication; the
version-specific in-client login/endpoint integration remains unverified.
Match servers are absent. Do not use Epic credentials.

Keep the working companion/rollback installed. This package has the separate
Hollowwake.NativeClientExperiment identity. Use Xbox Device Portal Apps > Add,
select the MSIX (or APPX), then supply the included Dependencies/x64 packages if requested.
The CER is the public development certificate; no private signing key is supplied.
Use the existing Dev Mode installation/trust procedure that worked previously.

If Dev Home offers App details > App type, select Game for this experiment to
request the game resource allocation. Controller/menu labels can vary by OS;
these UI steps have not been checked on hardware here. This classification does
not make a desktop EXE compatible. Microsoft documents foreground app/game
memory allocations at https://learn.microsoft.com/en-us/windows/uwp/xbox-apps/system-resource-allocation .

Boot code is included; do not import boot DLLs into LocalState for this route.
After linking or restoring your account, the launcher automatically downloads
only 139 pinned data files using small HTTPS byte ranges. Each file is streamed
and SHA-256 checked before becoming ready. No full ZIP is stored. About 12.36 GB
is transferred and 26.45 GB of data is installed, plus client code/dependencies.
Allow at least 27.6 GB of free app storage for the complete first download.
Use PAUSE (B) to stop, or DOWNLOAD / VERIFY CONTENT to retry. Verified files
are retained across pauses/restarts; the unfinished file restarts on reopening.
Original ZIPs previously imported by you are not deleted by this workflow.
The archive host receives no Hollowwake password, bearer token or cookies.

Structured failure reports upload privately after sign-in. If the app closes,
reopen and remain signed in so the incomplete attempt can upload. No page-by-
page photos are needed. Raw logs stay in LocalState.
Link only your Hollowwake account. Use the client-launch action once. Record
whether an original Fortnite loading screen appears, the exact first error, or
whether the host closes. After reopening, use launch logs. Device Portal's app
file explorer can retrieve LocalState/native-launch.log,
LocalState/native-imports-unresolved.txt and LocalState/managed-launch.log.
Game-written logs, when created, are under LocalState/GameClean/6.21/
FortniteGame/Saved/Logs. Do not infer lobby success from a passing preflight.

Original Fortnite authentication, lobby entry, characters, locker persistence,
admin shop integration and Xbox presentation remain unverified/incomplete.
The network adapters permit only the configured Hollowwake endpoint. A rejected
original production-service request is expected until client endpoint/login
integration is correctly configured; production Epic access is not enabled.
'@
 [IO.File]::WriteAllText((Join-Path $stage 'INSTALL.txt'),$instructions,[Text.UTF8Encoding]::new($false))
 $selected=@(Get-ChildItem $stage -Recurse -File)
 if($selected|Where-Object {$_.Extension-in @('.pfx','.key','.pem')}){throw 'Private/signing key or source material selected'}
 [IO.Compression.ZipFile]::CreateFromDirectory($stage,$archive,[IO.Compression.CompressionLevel]::Optimal,$false)
 $plain=[IO.File]::ReadAllBytes($archive);$key=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32);$nonce=[Security.Cryptography.RandomNumberGenerator]::GetBytes(12);$tag=[byte[]]::new(16);$cipher=[byte[]]::new($plain.Length)
 $aad=[Text.Encoding]::UTF8.GetBytes('Hollowwake private client delivery v1')
 $aes=[Security.Cryptography.AesGcm]::new($key,16);$rsa=[Security.Cryptography.RSA]::Create()
 try{
  $rsa.ImportFromPem([IO.File]::ReadAllText($RecipientPublicKey))
  $aes.Encrypt($nonce,$plain,$cipher,$tag,$aad)
  $wrapped=$rsa.Encrypt($key,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
  $publicDer=$rsa.ExportSubjectPublicKeyInfo()
  $output=Join-Path $OutputDirectory 'Hollowwake-Private-Client.hwenc';[IO.File]::WriteAllBytes($output,$cipher)
  function Hex([byte[]]$value){return [Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($value)).ToLowerInvariant()}
  $metadata=@{format='HollowwakePrivateArtifact1';algorithm='AES-256-GCM';keyWrap='RSA-OAEP-SHA256';recipientSha256=(Hex $publicDer);nonce=[Convert]::ToBase64String($nonce);tag=[Convert]::ToBase64String($tag);wrappedKey=[Convert]::ToBase64String($wrapped);cipherSha256=(Hex $cipher);plainSha256=(Hex $plain);plainBytes=$plain.Length;codeCommit=$env:GITHUB_SHA;gameExecuted=$false;xboxTested=$false;plaintextGameFilesPublic=$false}
  [IO.File]::WriteAllText((Join-Path $OutputDirectory 'delivery.json'),($metadata|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
  Write-Output ('PASS recipient-encrypted private delivery; bytes='+$cipher.Length+'; original client not executed; no plaintext game files/private keys in artifact')
 }finally{$aes.Dispose();$rsa.Dispose();[Array]::Clear($key,0,$key.Length);[Array]::Clear($plain,0,$plain.Length)}
}finally{Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue;Remove-Item -LiteralPath $archive -Force -ErrorAction SilentlyContinue}
