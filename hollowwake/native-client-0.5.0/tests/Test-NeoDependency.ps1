param([Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$file=Join-Path $env:RUNNER_TEMP ('HwNeoVclibs-'+[Guid]::NewGuid().ToString('N')+'.appx')
try{
 Invoke-WebRequest -UseBasicParsing -Uri 'https://content-cdn.neofn.dev/Microsoft.VCLibs.x64.14.00.appx' -OutFile $file -MaximumRedirection 0
 if((Get-Item $file).Length-ne 896581-or (Get-FileHash $file -Algorithm SHA256).Hash.ToLowerInvariant()-ne '9c17b521f9d690a1f504da5108ed6eec5669eb3a8fd1331eef43e40d84e74283'){throw 'Supplied dependency size/hash differs from static inspection'}
 $sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin\10.0.19041.0\x64'
 & (Join-Path $sdk 'signtool.exe') verify /pa /all /v $file 2>&1|Tee-Object (Join-Path $OutputDirectory 'neo-vclibs-signature.log')
 if($LASTEXITCODE){throw 'Supplied VCLibs Authenticode/package trust verification failed'}
 @{result='PASS Windows signtool package signature trust';name='Microsoft.VCLibs.140.00';architecture='x64';version='14.0.33519.0';sha256='9c17b521f9d690a1f504da5108ed6eec5669eb3a8fd1331eef43e40d84e74283';identicalToExistingHollowwake051Dependency=$true;executed=$false}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'neo-vclibs-signature.json')
}finally{if(Test-Path $file){Remove-Item $file -Force}}
