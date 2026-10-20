param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$work=Join-Path $OutputDirectory 'OwnedDelayPolicy';New-Item -ItemType Directory -Force $work|Out-Null
Push-Location $work
try{
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'DelayOptionalProvider.cpp') /link /OUT:OwnedOptional.dll /IMPLIB:OwnedOptional.lib
 if($LASTEXITCODE){throw 'Owned optional provider compilation failed'}
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'DelayConsumer.cpp') OwnedOptional.lib delayimp.lib /link /OUT:OwnedDelayConsumer.dll /DELAYLOAD:OwnedOptional.dll
 if($LASTEXITCODE){throw 'Real delayed consumer compilation failed'}
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'DelayConsumer.cpp') OwnedOptional.lib /link /OUT:OwnedNormalConsumer.dll
 if($LASTEXITCODE){throw 'Owned normal dependency consumer compilation failed'}
 Move-Item OwnedOptional.dll OwnedOptional.not-installed
 & cl.exe /nologo /EHsc /MD (Join-Path $PSScriptRoot 'DelayLoadPolicyTest.cpp') /Fe:DelayLoadPolicyTest.exe
 if($LASTEXITCODE){throw 'Delay-loader behavior fixture compilation failed'}
 & ./DelayLoadPolicyTest.exe (Join-Path $work 'OwnedDelayConsumer.dll') (Join-Path $work 'OwnedNormalConsumer.dll')
 if($LASTEXITCODE){throw 'Real OS delayed-loading behavior differs from policy'}
 & dumpbin.exe /nologo /imports OwnedDelayConsumer.dll|Set-Content delay-import-inventory.log
}finally{Pop-Location}
