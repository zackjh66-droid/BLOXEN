param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
Push-Location $OutputDirectory
try {
 & cl.exe /nologo /O2 /GS- /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'OriginalExeFixture.cpp') /Fe:OriginalExeFixture.exe /link /NODEFAULTLIB /ENTRY:OriginalEntry /SUBSYSTEM:WINDOWS /DYNAMICBASE /FIXED:NO /INCREMENTAL:NO /DEF:(Join-Path $PSScriptRoot 'OriginalExeFixture.def') kernel32.lib ntdll.lib 2>&1|Tee-Object native-image-fixture-build.log
 if($LASTEXITCODE){throw 'Original native-loader EXE fixture compilation failed'}
 $digest=(Get-FileHash -Algorithm SHA256 OriginalExeFixture.exe).Hash.ToLowerInvariant()
 & python (Join-Path $PSScriptRoot 'convert_image.py') OriginalExeFixture.exe OriginalConvertedFixture.dll --expected-sha256 $digest --report native-image-conversion.json 2>&1|Tee-Object native-image-conversion.log
 if($LASTEXITCODE){throw 'Original EXE conversion failed'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $PSScriptRoot 'NativeImageTest.cpp') kernel32.lib /Fe:NativeImageTest.exe 2>&1|Tee-Object native-image-host-build.log
 if($LASTEXITCODE){throw 'Native-loader host fixture compilation failed'}
 & .\NativeImageTest.exe (Join-Path (Get-Location) 'OriginalConvertedFixture.dll') 2>&1|Tee-Object native-image-test.log
 if($LASTEXITCODE){throw 'Native-loader EXE integration failed'}
} finally {Pop-Location}
