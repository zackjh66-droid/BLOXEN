param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$proxyOutput=Join-Path $OutputDirectory 'proxy'
New-Item -ItemType Directory -Force $proxyOutput|Out-Null
Push-Location $proxyOutput
try {
 $base=Split-Path $PSScriptRoot
 & cl.exe /nologo /LD /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'FixtureHost.cpp') (Join-Path $base 'NativeSystem.cpp') (Join-Path $base 'NativeProxyDispatch.cpp') kernel32.lib runtimeobject.lib /Fe:HollowwakeClientRuntime.dll 2>&1|Tee-Object (Join-Path $OutputDirectory 'proxy-host-build.log')
 if($LASTEXITCODE){throw 'Own production-dispatch proxy host build failed'}
 & cl.exe /nologo /O2 /GS- /DHW_PROXY_FIXTURE /DWIN32_LEAN_AND_MEAN (Join-Path $base 'native-image\OriginalExeFixture.cpp') /Fe:ProxyInput.exe /link /NODEFAULTLIB /ENTRY:OriginalEntry /SUBSYSTEM:WINDOWS /DYNAMICBASE /FIXED:NO /INCREMENTAL:NO /INCLUDE:_tls_used kernel32.lib 2>&1|Tee-Object (Join-Path $OutputDirectory 'proxy-input-build.log')
 if($LASTEXITCODE){throw 'Original import-redirect EXE fixture build failed'}
 $hash=(Get-FileHash -Algorithm SHA256 ProxyInput.exe).Hash.ToLowerInvariant()
 & python (Join-Path $base 'native-image\convert_image.py') ProxyInput.exe ProxyConvertedUnredirected.dll --expected-sha256 $hash --report convert.json 2>&1|Tee-Object (Join-Path $OutputDirectory 'proxy-convert.log')
 if($LASTEXITCODE){throw 'Original EXE conversion failed'}
 $hash=(Get-FileHash -Algorithm SHA256 ProxyConvertedUnredirected.dll).Hash.ToLowerInvariant()
 & python (Join-Path $PSScriptRoot 'redirect_imports.py') ProxyConvertedUnredirected.dll ProxyConvertedFixture.dll --expected-sha256 $hash --report redirect.json 2>&1|Tee-Object (Join-Path $OutputDirectory 'proxy-redirect.log')
 if($LASTEXITCODE){throw 'Original EXE import redirection failed'}
 $specs=@((Get-Content redirect.json -Raw|ConvertFrom-Json).proxies)
 $specs+=@{sourceDll='FixtureAbiProvider.dll';proxyDll='HWMIX.dll';symbols=@('MixDouble','#17')}
 $specs+=@{sourceDll='kernel32.dll';proxyDll='HWBAD.dll';symbols=@('GetTickCount64','HollowwakeAbsentFunction')}
 foreach($spec in $specs){
  $name=$spec.proxyDll
  $specPath=Join-Path (Get-Location) ($name+'.json');$spec|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 $specPath
  $generated=Join-Path (Get-Location) ($name+'.source')
  & python (Join-Path $PSScriptRoot 'generate_proxy.py') $specPath $generated
  if($LASTEXITCODE){throw "Proxy generation failed: $name"}
  $obj=Join-Path $generated 'proxy.obj';$cpp=Join-Path $generated 'proxy.cpp';$asm=Join-Path $generated 'proxy.asm';$def=Join-Path $generated 'proxy.def'
  & ml64.exe /nologo /c "/Fo$obj" $asm 2>&1|Tee-Object -Append (Join-Path $OutputDirectory 'proxy-dll-build.log')
  if($LASTEXITCODE){throw "Proxy assembly failed: $name"}
  & cl.exe /nologo /LD /MD /DWIN32_LEAN_AND_MEAN /DWINAPI_FAMILY=WINAPI_FAMILY_APP $cpp $obj HollowwakeClientRuntime.lib windowsapp.lib "/Fe:$name" /link "/DEF:$def" 2>&1|Tee-Object -Append (Join-Path $OutputDirectory 'proxy-dll-build.log')
  if($LASTEXITCODE){throw "Proxy DLL link failed: $name"}
 }
 & cl.exe /nologo /std:c++17 /EHsc /MD /DWIN32_LEAN_AND_MEAN (Join-Path $PSScriptRoot 'ProxyTest.cpp') kernel32.lib /Fe:ProxyTest.exe 2>&1|Tee-Object (Join-Path $OutputDirectory 'proxy-test-build.log')
 if($LASTEXITCODE){throw 'Proxy host test compilation failed'}
 & .\ProxyTest.exe (Get-Location).Path 2>&1|Tee-Object (Join-Path $OutputDirectory 'proxy-test.log')
 if($LASTEXITCODE){throw 'Native redirected executable integration failed'}
}finally{Pop-Location}
