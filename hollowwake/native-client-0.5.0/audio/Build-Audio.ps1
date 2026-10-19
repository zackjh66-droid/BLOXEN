param([string]$OutputDirectory)
$ErrorActionPreference='Continue'
# Native tools may write warnings to stderr. Check their actual exit code;
# do not turn a warning into an aborted compile under Windows PowerShell 5.
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$source=Join-Path $env:RUNNER_TEMP 'HollowwakeReviewedAudioSources'
python (Join-Path $PSScriptRoot 'prepare_audio.py') $source
if($LASTEXITCODE){throw 'Pinned audio input preparation failed'}
Copy-Item (Join-Path $source 'source-review.json') $OutputDirectory
$dsbuild=Join-Path $source 'dsoal-build'
$runtime=Join-Path (Split-Path $PSScriptRoot -Parent) 'Native/bin/x64/Release/HollowwakeClientRuntime.lib'
# DSOAL consumes the public desktop COM declarations. Its output is a candidate
# native AppContainer DLL; explicit import/packaged behavior checks still apply.
& cmake -S (Join-Path $source 'dsoal') -B $dsbuild -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0.19041.0' '-DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=10.0.19041.0' '-DDSOAL_UPDATE_BUILD_VERSION=OFF' "-DHW_RUNTIME_LIBRARY=$runtime" 2>&1|Tee-Object (Join-Path $OutputDirectory 'dsoal-configure.log')
if($LASTEXITCODE){throw 'DSOAL native candidate configuration failed'}
& cmake --build $dsbuild --config Release --target dsound --parallel 4 2>&1|Tee-Object (Join-Path $OutputDirectory 'dsoal-build.log')
if($LASTEXITCODE){throw 'DSOAL AppContainer candidate compilation failed'}
$openal=Join-Path $source 'openal-soft';$albuild=Join-Path $source 'openal-build'
# Upstream has an explicit WindowsStore/UWP implementation and C++/WinRT
# WASAPI activation. Disable unrelated desktop backends/tools, not game checks.
& cmake --no-warn-unused-cli -Wno-dev -S $openal -B $albuild -G 'Visual Studio 17 2022' -A x64 '-DCMAKE_SYSTEM_NAME=WindowsStore' '-DCMAKE_SYSTEM_VERSION=10.0.19041.0' '-DCMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION=10.0.19041.0' '-DALSOFT_UTILS=OFF' '-DALSOFT_EXAMPLES=OFF' '-DALSOFT_TESTS=OFF' '-DALSOFT_CONFIG=OFF' '-DALSOFT_INSTALL=OFF' '-DALSOFT_EAX=OFF' '-DALSOFT_BACKEND_WASAPI=ON' '-DALSOFT_REQUIRE_WASAPI=ON' '-DALSOFT_BACKEND_DSOUND=OFF' '-DALSOFT_BACKEND_WINMM=OFF' '-DALSOFT_BACKEND_WAVE=OFF' '-DALSOFT_BACKEND_NULL=ON' 2>&1|Tee-Object (Join-Path $OutputDirectory 'openal-configure.log')
if($LASTEXITCODE){throw 'OpenAL upstream UWP configuration failed'}
& cmake --build $albuild --config Release --target OpenAL --parallel 4 2>&1|Tee-Object (Join-Path $OutputDirectory 'openal-build.log')
if($LASTEXITCODE){throw 'OpenAL genuine UWP compilation failed'}
$ds=Join-Path $dsbuild 'Release/dsound.dll';$al=Join-Path $albuild 'Release/OpenAL32.dll'
if(!(Test-Path $ds)-or !(Test-Path $al)){throw 'Expected audio output missing'}
Copy-Item $ds (Join-Path $OutputDirectory 'HWAUD.dll');Copy-Item $al (Join-Path $OutputDirectory 'dsoal-aldrv.dll')
& dumpbin.exe /nologo /imports $ds 2>&1|Tee-Object (Join-Path $OutputDirectory 'dsoal-imports.log')
& dumpbin.exe /nologo /imports $al 2>&1|Tee-Object (Join-Path $OutputDirectory 'openal-imports.log')
python -m pip install --disable-pip-version-check pefile==2024.8.26
if($LASTEXITCODE){throw 'Pinned provider PE reader unavailable'}
python (Join-Path (Split-Path $PSScriptRoot -Parent) 'tests/ProviderInventory.py') $OutputDirectory (Join-Path $OutputDirectory 'provider-imports.json') HWAUD.dll dsoal-aldrv.dll
if($LASTEXITCODE){throw 'Actual additional provider inventory failed'}
Copy-Item (Join-Path $source 'dsoal.tar.gz') $OutputDirectory;Copy-Item (Join-Path $source 'openal-soft.tar.gz') $OutputDirectory
Copy-Item (Join-Path $PSScriptRoot 'prepare_audio.py') $OutputDirectory;Copy-Item (Join-Path $PSScriptRoot 'Build-Audio.ps1') $OutputDirectory
@{scope='Audio candidate compilation only; not integrated, API behavior or Xbox validation';dsoalSha256=(Get-FileHash $ds -Algorithm SHA256).Hash.ToLowerInvariant();openalSha256=(Get-FileHash $al -Algorithm SHA256).Hash.ToLowerInvariant();xboxTested=$false;gameExecuted=$false}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'audio-build.json')
