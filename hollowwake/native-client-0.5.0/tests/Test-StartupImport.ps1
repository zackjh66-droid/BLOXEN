param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$source=Split-Path $PSScriptRoot -Parent
$test=Join-Path $OutputDirectory 'OwnStartupFixture';New-Item -ItemType Directory -Force $test|Out-Null
Push-Location $test
try{
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'StartupProvider.cpp') /Fe:StartupProvider.dll
 if($LASTEXITCODE){throw 'Original startup provider fixture compilation failed'}
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'StartupConsumer.cpp') StartupProvider.lib /Fe:StartupConsumer.dll
 if($LASTEXITCODE){throw 'Original startup consumer fixture compilation failed'}
 & cl.exe /nologo /MD /DWIN32_LEAN_AND_MEAN (Join-Path $source 'Native/StartupImport.cpp') (Join-Path $PSScriptRoot 'StartupImportTest.cpp') kernel32.lib windowsapp.lib /Fe:StartupImportTest.exe
 if($LASTEXITCODE){throw 'Production startup import binder compilation failed'}
 & ./StartupImportTest.exe
 if($LASTEXITCODE){throw 'Actual image-local startup import behavior failed'}
}finally{Pop-Location}
