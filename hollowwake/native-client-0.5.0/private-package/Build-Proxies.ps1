param([string]$PreparedDirectory,[string]$RuntimeLibrary,[string]$ProxyTools)
$ErrorActionPreference='Stop';$manifest=Get-Content (Join-Path $PreparedDirectory 'plan.json') -Raw|ConvertFrom-Json
foreach($proxy in $manifest.proxyDlls){
 $directory=Join-Path (Join-Path $PreparedDirectory 'ProxySources') $proxy.proxyDll;$obj=Join-Path $directory 'proxy.obj';$cpp=Join-Path $directory 'proxy.cpp';$asm=Join-Path $directory 'proxy.asm';$definition=Join-Path $directory 'proxy.def';$dll=Join-Path (Join-Path $PreparedDirectory 'GamePayload') $proxy.proxyDll
 & ml64.exe /nologo /c "/Fo$obj" $asm
 if($LASTEXITCODE){throw 'Private proxy assembly failed'}
 & cl.exe /nologo /LD /MD /DWIN32_LEAN_AND_MEAN /DWINAPI_FAMILY=WINAPI_FAMILY_APP $cpp $obj $RuntimeLibrary windowsapp.lib "/Fe:$dll" /link /APPCONTAINER "/LIBPATH:$env:VCToolsInstallDir\lib\x64\store" "/DEF:$definition"
 if($LASTEXITCODE){throw 'Private proxy link failed'}
}
