param([string]$OutputDirectory)
$ErrorActionPreference='Continue'
$env:PYTHONUTF8='1'
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$source=Join-Path $env:RUNNER_TEMP 'HollowwakeReviewedMesaSources'
python (Join-Path $PSScriptRoot 'prepare_mesa.py') $source
if($LASTEXITCODE){throw 'Pinned Mesa preparation failed'}
Copy-Item (Join-Path $source 'mesa-source-review.json') $OutputDirectory
$env:PATH=(Join-Path $source 'parser-tools')+';'+$env:PATH
python -m pip install --disable-pip-version-check meson==1.5.2 mako==1.3.8 packaging==24.2 ninja==1.11.1.3
if($LASTEXITCODE){throw 'Legitimate pinned Mesa build tools unavailable'}
$store=(Join-Path $env:VCToolsInstallDir 'lib/x64/store').Replace('\','/')
$build=Join-Path $source 'build'
$runtime=(Join-Path (Split-Path $PSScriptRoot -Parent) 'Native/bin/x64/Release/HollowwakeClientRuntime.lib').Replace('\','/')
# Use only the public UWP/CoreWindow path. Gaming.Xbox/GDK targets, the optional
# swap allocator, software-GDI output and unrelated drivers are not selected.
# UWP executables require package activation and framework dependency binding.
# Tell Meson this is an actual WindowsStore cross target, not a desktop process
# it may launch for configuration probes. This changes build-system detection,
# not game preflight or the behavior of any API implementation.
$cross=Join-Path $source 'windows-store-x64.ini'
@'
[binaries]
c = 'cl'
cpp = 'cl'
ar = 'lib'
strip = 'llvm-strip'
[host_machine]
system = 'windows'
cpu_family = 'x86_64'
cpu = 'x86_64'
endian = 'little'
[properties]
needs_exe_wrapper = true
'@|Set-Content -Encoding ASCII $cross
$mesaArgs=@('setup','--cross-file',$cross,$build,(Join-Path $source 'mesa'),'--backend=ninja','--buildtype=release','--wrap-mode=nodownload','-Duwp=true','-Dxbox-swap=false','-Dplatforms=windows','-Dgallium-drivers=d3d12','-Dvulkan-drivers=[]','-Dgallium-d3d12-video=disabled','-Dglx=disabled','-Degl=disabled','-Dgbm=disabled','-Dgles1=disabled','-Dgles2=disabled','-Dopengl=true','-Dshared-glapi=true','-Dllvm=disabled','-Ddraw-use-llvm=false','-Dglvnd=false','-Dshader-cache=disabled','-Dxmlconfig=disabled','-Dzlib=disabled','-Dzstd=disabled','-Dlmsensors=disabled','-Dvideo-codecs=[]','-Dbuild-tests=false','-Db_pch=false','-Dc_winlibs=[]','-Dcpp_winlibs=[]',"-Dc_args=['/D_XBOX_UWP','/DWINAPI_FAMILY=WINAPI_FAMILY_APP']","-Dcpp_args=['/D_XBOX_UWP','/DWINAPI_FAMILY=WINAPI_FAMILY_APP']","-Dc_link_args=['/APPCONTAINER','/LIBPATH:$store','$runtime','windowsapp.lib']","-Dcpp_link_args=['/APPCONTAINER','/LIBPATH:$store','$runtime','windowsapp.lib']")
& meson @mesaArgs 2>&1|Tee-Object (Join-Path $OutputDirectory 'mesa-configure.log')
if($LASTEXITCODE){throw 'Mesa UWP configuration failed'}
& meson compile -C $build -j 4 opengl32 libgallium_wgl 2>&1|Tee-Object (Join-Path $OutputDirectory 'mesa-build.log')
if($LASTEXITCODE){throw 'Mesa UWP compilation failed'}
$files=@(Get-ChildItem $build -Recurse -File -Filter '*.dll')
if(!($files|Where-Object Name -eq 'opengl32.dll')-or !($files|Where-Object Name -eq 'libgallium_wgl.dll')){throw 'Required actual WGL libraries were not produced'}
foreach($f in $files){$outputName=if($f.Name-eq 'opengl32.dll'){'HWMGL.dll'}else{$f.Name};Copy-Item $f.FullName (Join-Path $OutputDirectory $outputName);& dumpbin.exe /nologo /imports $f.FullName 2>&1|Tee-Object (Join-Path $OutputDirectory ($f.Name+'-imports.log'))}
python -m pip install --disable-pip-version-check pefile==2024.8.26
if($LASTEXITCODE){throw 'Pinned provider PE reader unavailable'}
# Official unmodified Microsoft redistributable. Its AppContainer flag is
# reported honestly; actual UWP loading/behavior and every import are checked.
$dxcUrl='https://api.nuget.org/v3-flatcontainer/microsoft.direct3d.dxc/1.8.2407.12/microsoft.direct3d.dxc.1.8.2407.12.nupkg'
$dxcPackage=Join-Path $source 'microsoft.direct3d.dxc.1.8.2407.12.nupkg'
Invoke-WebRequest -UseBasicParsing -Uri $dxcUrl -OutFile $dxcPackage
if((Get-FileHash $dxcPackage -Algorithm SHA256).Hash.ToLowerInvariant()-ne 'eb4f6a3bb6b08aaa62f435b3dbf26b180702ca52398d3650d0dd538f56742cdc'){throw 'Official DXC package hash differs'}
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive=[IO.Compression.ZipFile]::OpenRead($dxcPackage)
try{
 foreach($pair in @(@('build/native/bin/x64/dxil.dll','DXIL.dll'),@('LICENSE-MS.txt','DXIL-LICENSE-MS.txt'),@('LICENSE-LLVM.txt','DXIL-LICENSE-LLVM.txt'),@('distributable_files.txt','DXIL-distributable-files.txt'))){
  $entry=$archive.GetEntry($pair[0]);if(!$entry){throw 'Required Microsoft redistributable/notice absent'};[IO.Compression.ZipFileExtensions]::ExtractToFile($entry,(Join-Path $OutputDirectory $pair[1]),$true)
 }
}finally{$archive.Dispose()}
$dxil=Join-Path $OutputDirectory 'DXIL.dll'
if((Get-FileHash $dxil -Algorithm SHA256).Hash.ToLowerInvariant()-ne '783bc73ce44cd23d735751c39e9340eed50e267cc6738884a29ed284ab9400a6'){throw 'Unmodified official DXIL binary differs'}
$signature=Get-AuthenticodeSignature $dxil
if($signature.Status-ne 'Valid'-or $signature.SignerCertificate.Subject-notmatch 'Microsoft'){throw 'Official DXIL Microsoft Authenticode verification failed'}
@{url=$dxcUrl;version='1.8.2407.12';packageSha256='eb4f6a3bb6b08aaa62f435b3dbf26b180702ca52398d3650d0dd538f56742cdc';file='DXIL.dll';sha256='783bc73ce44cd23d735751c39e9340eed50e267cc6738884a29ed284ab9400a6';signatureStatus=[string]$signature.Status;unmodified=$true;appContainerFlag=$false;redistributionNotice='DXIL-distributable-files.txt';xboxTested=$false}|ConvertTo-Json|Set-Content (Join-Path $OutputDirectory 'dxil-redistributable.json')
$providerNames=@($files|ForEach-Object {if($_.Name-eq 'opengl32.dll'){'HWMGL.dll'}else{$_.Name}})
python (Join-Path (Split-Path $PSScriptRoot -Parent) 'tests/ProviderInventory.py') $OutputDirectory (Join-Path $OutputDirectory 'provider-imports.json') @providerNames 'DXIL.dll'
if($LASTEXITCODE){throw 'Actual additional Mesa provider inventory failed'}
Copy-Item (Join-Path $source 'mesa-source.tar.gz') $OutputDirectory;Copy-Item (Join-Path $source 'directx-headers-source.tar.gz') $OutputDirectory
Copy-Item (Join-Path $PSScriptRoot 'prepare_mesa.py') $OutputDirectory;Copy-Item (Join-Path $PSScriptRoot 'CoreWindowGdi.cpp') $OutputDirectory;Copy-Item (Join-Path $PSScriptRoot 'Build-Mesa.ps1') $OutputDirectory
@{scope='Mesa UWP candidate compilation only, not game/Xbox or API behavior';files=@($files|ForEach-Object {@{name=$(if($_.Name-eq 'opengl32.dll'){'HWMGL.dll'}else{$_.Name});sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}});xboxTested=$false;gameExecuted=$false}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory 'mesa-build.json')
