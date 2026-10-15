param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$source=$PSScriptRoot
if(!$OutputDirectory){$OutputDirectory=Join-Path $source 'evidence\hosted'}
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe';$vs=& $vswhere -latest -products '*' -property installationPath
$version=(Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt')).Trim();$vc=Join-Path $vs ('VC\Tools\MSVC\'+$version)
$sdk=Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10';$v='10.0.19041.0'
$env:PATH=(Join-Path $vc 'bin\Hostx64\x64')+';'+$env:PATH
$env:INCLUDE=(Join-Path $vc 'include')+';'+(Join-Path $sdk "Include\$v\ucrt")+';'+(Join-Path $sdk "Include\$v\shared")+';'+(Join-Path $sdk "Include\$v\um")+';'+(Join-Path $sdk "Include\$v\winrt")
$env:LIB=(Join-Path $vc 'lib\x64')+';'+(Join-Path $sdk "Lib\$v\ucrt\x64")+';'+(Join-Path $sdk "Lib\$v\um\x64")
Push-Location $OutputDirectory
try {
 & cl.exe /nologo /c /MD /EHsc /std:c++17 /DWINAPI_FAMILY=WINAPI_FAMILY_APP /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'graphics\CoreWindowBridge.cpp') /Fo:CoreWindowBridge.obj 2>&1|Tee-Object graphics-build.log
 if($LASTEXITCODE){throw 'CoreWindow bridge UWP API-family compilation failed'}
 & cl.exe /nologo /c /MD /EHsc /std:c++17 /DWINAPI_FAMILY=WINAPI_FAMILY_APP /DWIN32_LEAN_AND_MEAN /DNOMINMAX (Join-Path $source 'graphics\GuestImports.cpp') /Fo:GuestImports.obj 2>&1|Tee-Object graphics-imports-build.log
 if($LASTEXITCODE){throw 'Graphics exact-ABI hooks compilation failed'}
 # Link against Windows Store import library; not the desktop d3d11/dxgi libs.
 & lib.exe /nologo /OUT:HollowwakeCoreWindowBridge.lib CoreWindowBridge.obj GuestImports.obj
 & link.exe /DLL /APPCONTAINER CoreWindowBridge.obj GuestImports.obj windowsapp.lib vcruntime.lib ucrt.lib /OPT:NOREF "/LIBPATH:$vc\lib\x64\store" /OUT:CoreWindowBridge.dll 2>&1|Tee-Object graphics-link.log
 if($LASTEXITCODE){throw 'CoreWindow bridge UWP linkage failed'}
 & cl.exe /nologo /LD /GS- /Oi /Zl (Join-Path $source 'tls\Donor.cpp') kernel32.lib /link /NODEFAULTLIB /ENTRY:DonorEntry /INCLUDE:_tls_used /OUT:HollowwakeTlsDonor.dll 2>&1|Tee-Object donor-build.log
 if($LASTEXITCODE){throw 'Donor build failed'}
 & cl.exe /nologo /LD /GS- /Oi /Zl (Join-Path $source 'tests\GuestTls.cpp') /link /NODEFAULTLIB /NOENTRY /INCLUDE:_tls_used /OUT:GuestTls.dll 2>&1|Tee-Object guest-build.log
 if($LASTEXITCODE){throw 'Guest build failed'}
 $guestHash=(Get-FileHash .\GuestTls.dll -Algorithm SHA256).Hash
 $hashBytes=for($i=0;$i-lt 64;$i+=2){'0x'+$guestHash.Substring($i,2)}
 [IO.File]::WriteAllText((Join-Path $OutputDirectory 'GuestHash.generated.h'),('static const unsigned char ExpectedGuestSha256[32]={'+($hashBytes -join ',')+'};'))
 & cl.exe /nologo /MD /EHsc /std:c++17 "/I$OutputDirectory" (Join-Path $source 'tests\MappedTls.cpp') bcrypt.lib /Fe:MappedTls.exe 2>&1|Tee-Object mapped-build.log
 if($LASTEXITCODE){throw 'Mapped fixture harness build failed'}
 & .\MappedTls.exe 2>&1|Tee-Object mapped-test.log
 if($LASTEXITCODE){throw "Mapped fixture runtime failed: $LASTEXITCODE"}
 # A modified fixture must be rejected by the compiled hash gate BEFORE mapping.
 $original=[IO.File]::ReadAllBytes((Join-Path $OutputDirectory 'GuestTls.dll'))
 try {
  $modified=$original.Clone();$modified[$modified.Length-1]=$modified[$modified.Length-1] -bxor 1
  [IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'GuestTls.dll'),$modified)
  $negative=Start-Process -FilePath (Join-Path $OutputDirectory 'MappedTls.exe') -NoNewWindow -Wait -PassThru -RedirectStandardOutput 'hash-rejection-stdout.log' -RedirectStandardError 'hash-rejection-stderr.log'
  if($negative.ExitCode-ne 3){throw 'Modified fixture was not safely rejected before mapping'}
  '{"modifiedFixtureRejectedBeforeMapping":true,"exitCode":3}'|Set-Content hash-rejection.json
 }finally{[IO.File]::WriteAllBytes((Join-Path $OutputDirectory 'GuestTls.dll'),$original)}
 Get-ChildItem -File | Where-Object {$_.Extension -in @('.obj','.exp')} | Remove-Item -Force
 Get-ChildItem -File | Where-Object {$_.Extension -in @('.dll','.exe')} | Get-FileHash -Algorithm SHA256 | Select-Object Path,Hash | ConvertTo-Json | Set-Content fixture-hashes.json
}finally{Pop-Location}
