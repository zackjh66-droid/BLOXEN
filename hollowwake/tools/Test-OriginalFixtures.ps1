# Compiles ORIGINAL fixtures plus a bounded, reviewed upstream TLS routine.
# Does not run the full candidate loader, its host agent, any game or any supplied launcher.
param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$repo=Split-Path $PSScriptRoot -Parent
if(!$OutputDirectory){$OutputDirectory=Join-Path $repo 'artifacts\fixtures'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$vs=& $vswhere -latest -products '*' -property installationPath
$version=(Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt')).Trim();$vc=Join-Path $vs ('VC\Tools\MSVC\'+$version)
$sdkRoot=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10';$sdkVersion='10.0.19041.0'
$env:PATH=(Join-Path $vc 'bin\Hostx64\x64')+';'+(Join-Path $sdkRoot ('bin\'+$sdkVersion+'\x64'))+';'+$env:PATH
$env:INCLUDE=(Join-Path $vc 'include')+';'+(Join-Path $sdkRoot ('Include\'+$sdkVersion+'\ucrt'))+';'+(Join-Path $sdkRoot ('Include\'+$sdkVersion+'\shared'))+';'+(Join-Path $sdkRoot ('Include\'+$sdkVersion+'\um'))
$env:LIB=(Join-Path $vc 'lib\x64')+';'+(Join-Path $sdkRoot ('Lib\'+$sdkVersion+'\ucrt\x64'))+';'+(Join-Path $sdkRoot ('Lib\'+$sdkVersion+'\um\x64'))
$source=Join-Path $repo 'runtime-diagnostic\fixtures';$private=Join-Path $env:RUNNER_TEMP 'hollowwake-reviewed-tls';New-Item -ItemType Directory -Force $private|Out-Null
# Exact reviewed bytes, pinned to immutable upstream commit. Source is treated as data.
$upstream=Join-Path $private 'PeLoader.cpp'
Invoke-WebRequest -UseBasicParsing -Uri 'https://raw.githubusercontent.com/http-hadi/xbox-win32-runner/84bb0f71a698ea5d2733d1e02c1caa96a33b1c01/pe-loader/PeLoader.cpp' -OutFile $upstream
if((Get-FileHash $upstream -Algorithm SHA256).Hash.ToLower() -ne '6dc3a2e0e80333e6dbf32c4a9d19ffb37348b264be5142b2bea6643533498aee'){throw 'Upstream source differs from reviewed bytes'}
$text=[IO.File]::ReadAllText($upstream);$match=[regex]::Match($text,'(?s)bool PeLoader::ProcessTls\(.*?\r?\n\}')
if(!$match.Success -or $match.Value.Length -gt 2500){throw 'Reviewed routine extraction failed'}
$prefix='namespace xwr { class PeLoader { public: uint32_t m_tlsIndexCounter=0; bool ProcessTls(uint8_t*,uint32_t,uint32_t,uint16_t); };'
[IO.File]::WriteAllText((Join-Path $private 'TLSProbe.generated.h'),$prefix+"`n"+$match.Value+"`n}")
Push-Location $OutputDirectory
try {
 & cl.exe /nologo /LD /MT /GS- /Zl (Join-Path $source 'tls_fixture.cpp') /link /NOENTRY /NODEFAULTLIB /OUT:tls_fixture.dll 2>&1|Tee-Object 'tls-build.log';if($LASTEXITCODE -ne 0){throw 'Original static TLS fixture compilation failed'}
 & cl.exe /nologo /LD /MD /EHsc (Join-Path $source 'runtime_fixture.cpp') version.lib delayimp.lib /link /DELAYLOAD:version.dll /OUT:runtime_fixture.dll 2>&1|Tee-Object 'runtime-build.log';if($LASTEXITCODE -ne 0){throw 'Original import/exception fixture compilation failed'}
 & cl.exe /nologo /MD /EHsc /std:c++17 "/I$private" (Join-Path $source 'probe.cpp') /Fe:original-fixture-probe.exe 2>&1|Tee-Object 'probe-build.log';if($LASTEXITCODE -ne 0){throw 'Original fixture harness compilation failed'}
 & .\original-fixture-probe.exe 2>&1|Tee-Object 'fixture-test.log';if($LASTEXITCODE -ne 0){throw 'Fixture baseline or scoped candidate negative test failed'}
 # Headers/source containing upstream code remain private and are excluded from delivery.
 Get-ChildItem -File -Include '*.obj','*.lib','*.exp'|Remove-Item -Force
}finally{Pop-Location;Remove-Item -Recurse -Force $private}
