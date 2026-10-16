param([string]$OutputDirectory)
$ErrorActionPreference='Stop';New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
Push-Location $OutputDirectory
try {
 & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'NativeSystem.cpp') (Join-Path $PSScriptRoot 'tests\NativeSystemTest.cpp') kernel32.lib runtimeobject.lib /Fe:NativeSystemTest.exe 2>&1|Tee-Object build.log
 if($LASTEXITCODE){throw 'Native-system test compilation failed'}
 & .\NativeSystemTest.exe 2>&1|Tee-Object test.log
 if($LASTEXITCODE){throw 'Native-system functionality failed'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'NativeSystem.cpp') (Join-Path $PSScriptRoot '..\experimental-0.4.7\Native\ModuleLoader.cpp') (Join-Path $PSScriptRoot 'tests\ModuleNamespaceTest.cpp') kernel32.lib runtimeobject.lib /Fe:ModuleNamespaceTest.exe 2>&1|Tee-Object namespace-build.log
 if($LASTEXITCODE){throw 'Production module namespace test compilation failed'}
 & .\ModuleNamespaceTest.exe 2>&1|Tee-Object namespace-test.log
 if($LASTEXITCODE){throw 'Production module namespace functionality failed'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'NativeSystem.cpp') (Join-Path $PSScriptRoot 'GuestThreads.cpp') (Join-Path $PSScriptRoot 'ThreadImports.cpp') (Join-Path $PSScriptRoot 'tests\GuestThreadTest.cpp') kernel32.lib runtimeobject.lib /Fe:GuestThreadTest.exe 2>&1|Tee-Object thread-build.log
 if($LASTEXITCODE){throw 'Production guest thread adapter compilation failed'}
 & .\GuestThreadTest.exe 2>&1|Tee-Object thread-test.log
 if($LASTEXITCODE){throw 'Production guest thread lifecycle checks failed'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DWINAPI_FAMILY=WINAPI_FAMILY_APP /c (Join-Path $PSScriptRoot 'NativeSystem.cpp') /Fo:NativeSystemApp.obj 2>&1|Tee-Object uwp-family-compile.log
 if($LASTEXITCODE){throw 'UWP API-family compilation failed'}
 '{"result":"PASS","scope":"Original native-system export resolver; actual OS startup-info, thread and legacy allocation operations","fortniteExecuted":false,"xboxTested":false,"appxDelivered":false}'|Set-Content result.json
}finally{Pop-Location}

$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -property installationPath
$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
& $msbuild (Join-Path $PSScriptRoot '..\experimental-0.4.7\Native\Native.vcxproj') /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:GenerateAppxPackageOnBuild=false /nologo 2>&1|Tee-Object (Join-Path $OutputDirectory 'integrated-uwp-build.log')
if($LASTEXITCODE){throw 'Integrated native UWP runtime failed compilation/linkage'}
