param([Parameter(Mandatory)][string]$OutputDirectory,[Parameter(Mandatory)][string]$PublicKey)
$ErrorActionPreference='Stop'
$dir=Join-Path $env:RUNNER_TEMP 'NeoReferencePrivate/screens'
$files=@(Get-ChildItem $dir -Filter '*.png' -ErrorAction SilentlyContinue)
if(!$files.Count){return}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$zip=Join-Path $env:RUNNER_TEMP 'neo-reference-screens.zip'
Compress-Archive -LiteralPath $files.FullName -DestinationPath $zip -Force
if((Get-Item $zip).Length-gt 52428800){throw 'Screen evidence exceeds bounded size'}
$plain=[IO.File]::ReadAllBytes($zip);$cipher=[byte[]]::new($plain.Length)
$key=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32)
$nonce=[Security.Cryptography.RandomNumberGenerator]::GetBytes(12);$tag=[byte[]]::new(16)
$aes=[Security.Cryptography.AesGcm]::new($key,16);$rsa=[Security.Cryptography.RSA]::Create()
try{
 $rsa.ImportFromPem([IO.File]::ReadAllText($PublicKey))
 $aes.Encrypt($nonce,$plain,$cipher,$tag,[Text.Encoding]::UTF8.GetBytes('Hollowwake private client delivery v1'))
 $wrapped=$rsa.Encrypt($key,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
 $sha={param($b)([Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($b))).ToLowerInvariant()}
 [IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'Neo-Reference-Screens.hwenc'),$cipher)
 @{format='HollowwakePrivateArtifact1';algorithm='AES-256-GCM';keyWrap='RSA-OAEP-SHA256';recipientSha256=(&$sha $rsa.ExportSubjectPublicKeyInfo());nonce=[Convert]::ToBase64String($nonce);tag=[Convert]::ToBase64String($tag);wrappedKey=[Convert]::ToBase64String($wrapped);cipherSha256=(&$sha $cipher);plainSha256=(&$sha $plain);plainBytes=$plain.Length;codeCommit=$env:GITHUB_SHA;scope='Neo reference screenshots only';referenceAppExecuted=$true;hollowwakeGameExecuted=$false;xboxTested=$false;screenCount=$files.Count}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'neo-screens-delivery.json')
}finally{$aes.Dispose();$rsa.Dispose();[Array]::Clear($key);[Array]::Clear($plain);Remove-Item $zip -Force -ErrorAction SilentlyContinue}
