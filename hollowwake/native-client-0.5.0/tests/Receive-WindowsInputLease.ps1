param([Parameter(Mandatory=$true)][string]$OutputPath)
$ErrorActionPreference='Stop'
$rsa=[Security.Cryptography.RSA]::Create();$rsa.FromXmlString([IO.File]::ReadAllText((Join-Path $env:RUNNER_TEMP 'hw-input-private.xml')))
$deadline=[DateTime]::UtcNow.AddMinutes(20)
try{
 do{
  try{$envelope=Invoke-RestMethod -Uri ('https://raw.githubusercontent.com/zackjh66-droid/BLOXEN/hollowwake/native-system-route/hollowwake/integrated-windows-test/lease-'+$env:GITHUB_RUN_ID+'.json?poll='+[DateTimeOffset]::UtcNow.ToUnixTimeSeconds()) -TimeoutSec 15}catch{$envelope=$null}
  if($envelope){
   if($envelope.runId-ne $env:GITHUB_RUN_ID){throw 'Encrypted input envelope run mismatch'}
   $key=$rsa.Decrypt([Convert]::FromBase64String($envelope.key),[Security.Cryptography.RSAEncryptionPadding]::OaepSHA256)
   $cipher=[Convert]::FromBase64String($envelope.ciphertext);$plain=[byte[]]::new($cipher.Length);$aes=[Security.Cryptography.AesGcm]::new($key,16)
   try{$aes.Decrypt([Convert]::FromBase64String($envelope.nonce),$cipher,[Convert]::FromBase64String($envelope.tag),$plain,[Text.Encoding]::UTF8.GetBytes($env:GITHUB_RUN_ID))}finally{$aes.Dispose();[Array]::Clear($key,0,$key.Length)}
   $lease=[Text.Encoding]::UTF8.GetString($plain)|ConvertFrom-Json
   if($lease.runId-ne $env:GITHUB_RUN_ID-or $lease.expiresAt-lt [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()+300000){throw 'Private input lease expired or belongs to a different run'}
   [IO.File]::WriteAllBytes($OutputPath,$plain);[Array]::Clear($plain,0,$plain.Length)
   Write-Host 'Received one-run encrypted private input lease; signed URL not printed';exit 0
  }
  Start-Sleep -Seconds 10
 }while([DateTime]::UtcNow-lt $deadline)
 throw 'No valid recipient-encrypted private input lease was supplied for this run'
}finally{$rsa.Dispose();Remove-Item (Join-Path $env:RUNNER_TEMP 'hw-input-private.xml') -ErrorAction SilentlyContinue}
