param([string]$Directory)
$ErrorActionPreference='Stop';New-Item -ItemType Directory -Force $Directory|Out-Null
$rsa=[Security.Cryptography.RSA]::Create(3072)
try{
 [IO.File]::WriteAllText((Join-Path $env:RUNNER_TEMP 'hw-input-private.xml'),$rsa.ToXmlString($true))
 @{runId=$env:GITHUB_RUN_ID;publicKey=[Convert]::ToBase64String($rsa.ExportSubjectPublicKeyInfo());algorithm='RSA-OAEP-SHA256+A256GCM';purpose='One-run private input lease; no account credential'}|ConvertTo-Json|Set-Content (Join-Path $Directory 'input-public-key.json')
}finally{$rsa.Dispose()}
