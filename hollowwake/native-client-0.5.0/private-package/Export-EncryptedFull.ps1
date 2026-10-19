param([Parameter(Mandatory=$true)][string]$ResultFile,[Parameter(Mandatory=$true)][string]$OutputDirectory,[Parameter(Mandatory=$true)][string]$RecipientPublicKey)
$ErrorActionPreference='Stop'
$result=Get-Content $ResultFile -Raw|ConvertFrom-Json
$inputDirectory=$result.outputDirectory
$audit=Get-Content (Join-Path $inputDirectory 'full-package-audit.json') -Raw|ConvertFrom-Json
if($audit.result-ne 'PASS private full-content MSIX inventory and pinned hashes'-or $audit.contentFiles-ne 139-or $audit.fortniteExecuted){throw 'Verified full-content MSIX required'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$archive=Join-Path (Split-Path $inputDirectory) 'PrivateFullInstall.zip'
try{
 Add-Type -AssemblyName System.IO.Compression
 $zip=[IO.Compression.ZipFile]::Open($archive,[IO.Compression.ZipArchiveMode]::Create)
 try{
  foreach($file in Get-ChildItem $inputDirectory -File -Recurse){
   if($file.Extension-notin @('.msix','.cer','.appx','.json','.txt')){throw 'Unexpected/private-key file type in delivery selection'}
   $relative=[IO.Path]::GetRelativePath($inputDirectory,$file.FullName).Replace('\','/')
   [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip,$file.FullName,$relative,[IO.Compression.CompressionLevel]::NoCompression)|Out-Null
  }
 }finally{$zip.Dispose()}
 $rsa=[Security.Cryptography.RSA]::Create();$rsa.ImportFromPem([IO.File]::ReadAllText($RecipientPublicKey))
 $key=[Security.Cryptography.RandomNumberGenerator]::GetBytes(32);$aes=[Security.Cryptography.AesGcm]::new($key,16)
 $wrapped=$rsa.Encrypt($key,[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
 $plainSha=(Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant();$stream=[IO.File]::OpenRead($archive)
 $parts=@();$index=0;$total=0L
 try{
  $buffer=[byte[]]::new(16MB)
  while(($count=$stream.Read($buffer,0,$buffer.Length))-gt 0){
   $plain=[byte[]]::new($count);[Array]::Copy($buffer,$plain,$count)
   $cipher=[byte[]]::new($count);$nonce=[Security.Cryptography.RandomNumberGenerator]::GetBytes(12);$tag=[byte[]]::new(16)
   $aad=[Text.Encoding]::UTF8.GetBytes('Hollowwake private full delivery v2:'+ $index)
   $aes.Encrypt($nonce,$plain,$cipher,$tag,$aad)
   $name=('part-{0:D5}.hwenc'-f $index);[IO.File]::WriteAllBytes((Join-Path $OutputDirectory $name),$cipher)
   $parts+=@{index=$index;file=$name;bytes=$count;nonce=[Convert]::ToBase64String($nonce);tag=[Convert]::ToBase64String($tag);cipherSha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($cipher)).ToLowerInvariant()}
   $index++;$total+=$count;[Array]::Clear($plain,0,$plain.Length)
  }
 }finally{$stream.Dispose();$aes.Dispose();[Array]::Clear($key,0,$key.Length)}
 $metadata=@{format='HollowwakePrivateFullArtifact2';algorithm='AES-256-GCM';keyWrap='RSA-OAEP-SHA256';recipientSha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($rsa.ExportSubjectPublicKeyInfo())).ToLowerInvariant();wrappedKey=[Convert]::ToBase64String($wrapped);parts=$parts;plainBytes=$total;plainSha256=$plainSha;codeCommit=$env:GITHUB_SHA;gameExecuted=$false;xboxTested=$false;plaintextGameFilesPublic=$false}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory 'delivery.json'),($metadata|ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false));$rsa.Dispose()
 Write-Output ('PASS recipient-encrypted full private delivery; parts='+$index+'; no plaintext game/private keys selected')
}finally{if(Test-Path $archive){Remove-Item -LiteralPath $archive -Force}}
