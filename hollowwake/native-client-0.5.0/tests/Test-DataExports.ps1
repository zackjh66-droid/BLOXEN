param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$source=Split-Path $PSScriptRoot -Parent
$test=Join-Path $OutputDirectory 'DataExportFixture';New-Item -ItemType Directory -Force $test|Out-Null
$spec=Join-Path $test 'spec.json';@{sourceDll='HWDataProvider.dll';proxyDll='HWDataProxy.dll';symbols=@('HWFixtureData','HWReadData');dataSymbols=@('HWFixtureData')}|ConvertTo-Json|Set-Content $spec
python (Join-Path $source 'private-package/generate_native_proxy.py') $spec (Join-Path $test 'generated')
if($LASTEXITCODE){throw 'Data proxy generation failed'}
Push-Location $test
try{
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'DataExportProvider.cpp') /Fe:HWDataProvider.dll
 if($LASTEXITCODE){throw 'Data provider fixture compilation failed'}
 & cl.exe /nologo /LD /MD (Join-Path $PSScriptRoot 'DataExportHost.cpp') /Fe:HWProxyTestHost.dll
 if($LASTEXITCODE){throw 'Data resolver fixture compilation failed'}
 & ml64.exe /nologo /c /Foproxy-asm.obj generated/proxy.asm
 if($LASTEXITCODE){throw 'Data proxy fixture assembly failed'}
 # LIB emits the forwarder export directory without LINK's spurious undefined-symbol check.
 & lib.exe /nologo /DEF:generated/proxy-import.def /MACHINE:X64 /OUT:HWDataProxy.lib
 if($LASTEXITCODE){throw 'DATA-aware consumer import library generation failed'}
 & cl.exe /nologo /LD /MD generated/proxy.cpp /Foproxy-cpp.obj proxy-asm.obj HWDataProxy.exp HWProxyTestHost.lib windowsapp.lib /Fe:HWDataProxy.dll
 if($LASTEXITCODE){throw 'Data proxy fixture link failed'}
 & cl.exe /nologo /MD (Join-Path $PSScriptRoot 'DataExportConsumer.cpp') HWDataProxy.lib /Fe:DataExportConsumer.exe
 if($LASTEXITCODE){throw 'Data consumer fixture compilation failed'}
 & ./DataExportConsumer.exe
 if($LASTEXITCODE){throw 'Data import ABI test failed'}
}finally{Pop-Location}
