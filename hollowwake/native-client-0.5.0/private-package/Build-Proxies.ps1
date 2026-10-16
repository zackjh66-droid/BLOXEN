param([string]$PreparedDirectory,[string]$RuntimeLibrary,[string]$ProxyTools)
$ErrorActionPreference='Stop';$manifest=Get-Content (Join-Path $PreparedDirectory 'plan.json') -Raw|ConvertFrom-Json
foreach($proxy in $manifest.proxyDlls){
 $directory=Join-Path (Join-Path $PreparedDirectory 'ProxySources') $proxy.proxyDll;$obj=Join-Path $directory 'proxy-asm.obj';$cpp=Join-Path $directory 'proxy.cpp';$asm=Join-Path $directory 'proxy.asm';$definition=Join-Path $directory 'proxy-import.def';$dll=Join-Path (Join-Path $PreparedDirectory 'GamePayload') $proxy.proxyDll
 & ml64.exe /nologo /c "/Fo$obj" $asm
 if($LASTEXITCODE){throw 'Private proxy assembly failed'}
 $importLibrary=Join-Path $directory 'proxy-exports.lib';$exportObject=Join-Path $directory 'proxy-exports.exp'
 & lib.exe /nologo "/DEF:$definition" /MACHINE:X64 "/OUT:$importLibrary"
 if($LASTEXITCODE){throw 'Private proxy export directory generation failed'}
 & cl.exe /nologo /LD /MD /DWIN32_LEAN_AND_MEAN /DWINAPI_FAMILY=WINAPI_FAMILY_APP $cpp ("/Fo"+(Join-Path $directory 'proxy-cpp.obj')) $obj $exportObject $RuntimeLibrary windowsapp.lib "/Fe:$dll" /link /APPCONTAINER "/LIBPATH:$env:VCToolsInstallDir\lib\x64\store"
 if($LASTEXITCODE){throw 'Private proxy link failed'}
}
