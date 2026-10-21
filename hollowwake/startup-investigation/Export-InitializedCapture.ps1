param([Parameter(Mandatory=$true)][string]$InputDirectory,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$RecipientPublicKey)
$ErrorActionPreference='Stop'
$receipt=Get-Content (Join-Path $InputDirectory 'capture.json') -Raw|ConvertFrom-Json
if(!$receipt.captureComplete-or !$receipt.comparisonEqual-or $receipt.clientChecksPatched-or $receipt.originalSha256-ne '9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'-or $receipt.phase-ne 'original_entry_first_call_return'){throw 'Complete exact-source original initialization capture required'}
$allowed=@('capture.json','runtime-headers.bin','source-headers.bin')+@(0..8|ForEach-Object {'section-{0:D2}.bin'-f $_})
$files=@(Get-ChildItem $InputDirectory -File)
if($files.Count-ne $allowed.Count-or ($files|Where-Object {$_.Name-notin $allowed-or $_.Attributes-band [IO.FileAttributes]::ReparsePoint})){throw 'Unexpected private capture file'}
$archive=Join-Path $env:RUNNER_TEMP ('HollowwakeInitialized-'+[Guid]::NewGuid().ToString('N')+'.zip')
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
Add-Type -AssemblyName System.IO.Compression
try {
 [IO.Compression.ZipFile]::CreateFromDirectory($InputDirectory,$archive,[IO.Compression.CompressionLevel]::Optimal,$false)
 $plain=[IO.File]::ReadAllBytes($archive);$key=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32);$nonce=[Security.Cryptography.RandomNumberGenerator]::GetBytes(12);$tag=[byte[]]::new(16);$cipher=[byte[]]::new($plain.Length)
 $aes=[Security.Cryptography.AesGcm]::new($key,16);$rsa=[Security.Cryptography.RSA]::Create()
 try {
  $rsa.ImportFromPem([IO.File]::ReadAllText($RecipientPublicKey))
  $aad=[Text.Encoding]::UTF8.GetBytes('Hollowwake private initialized-image capture v1')
  $aes.Encrypt($nonce,$plain,$cipher,$tag,$aad)
  $wrapped=$rsa.Encrypt($key,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
  $recipientDer=$rsa.ExportSubjectPublicKeyInfo()
  function Sha([byte[]]$data){[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($data)).ToLowerInvariant()}
  [IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'initialized-capture.hwenc'),$cipher)
  $metadata=@{format='HollowwakePrivateStartupCapture1';algorithm='AES-256-GCM';keyWrap='RSA-OAEP-SHA256';recipientSha256=(Sha $recipientDer);nonce=[Convert]::ToBase64String($nonce);tag=[Convert]::ToBase64String($tag);wrappedKey=[Convert]::ToBase64String($wrapped);cipherSha256=(Sha $cipher);plainSha256=(Sha $plain);plainBytes=$plain.Length;codeCommit=$env:GITHUB_SHA;originalInitializationExecuted=$true;xboxTested=$false;lobbyDemonstrated=$false;plaintextGameFilesPublic=$false}
  [IO.File]::WriteAllText((Join-Path $OutputDirectory 'delivery.json'),($metadata|ConvertTo-Json),[Text.UTF8Encoding]::new($false))
  Write-Output 'Recipient-encrypted initialized-image capture prepared; not an Xbox package or lobby proof.'
 } finally {$aes.Dispose();$rsa.Dispose();[Array]::Clear($key,0,$key.Length);[Array]::Clear($plain,0,$plain.Length)}
} finally {Remove-Item $archive -Force -ErrorAction SilentlyContinue;Remove-Item $InputDirectory -Recurse -Force -ErrorAction SilentlyContinue}
