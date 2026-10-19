param([string]$OutputDirectory)
$ErrorActionPreference='Continue'
$env:PYTHONUTF8='1'
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null
$source=Join-Path $env:RUNNER_TEMP 'HollowwakeReviewedMesaSources'
python (Join-Path $PSScriptRoot 'prepare_mesa.py') $source
if($LASTEXITCODE){throw 'Pinned Mesa preparation failed'}
Copy-Item (Join-Path $source 'mesa-source-review.json') $OutputDirectory
python -m pip install --disable-pip-version-check meson==1.5.2 mako==1.3.8 packaging==24.2 ninja==1.11.1.3
if($LASTEXITCODE){throw 'Legitimate pinned Mesa build tools unavailable'}
$store=Join-Path $env:VCToolsInstallDir 'lib/x64/store'
$build=Join-Path $source 'build'
# Use only the public UWP/CoreWindow path. Gaming.Xbox/GDK targets, the optional
# swap allocator, software-GDI output and unrelated drivers are not selected.
$mesaArgs=@('setup',$build,(Join-Path $source 'mesa'),'--backend=ninja','--buildtype=release','--wrap-mode=nodownload','-Duwp=true','-Dxbox-swap=false','-Dplatforms=windows','-Dgallium-drivers=d3d12','-Dvulkan-drivers=[]','-Dgallium-d3d12-video=disabled','-Dglx=disabled','-Degl=disabled','-Dgbm=disabled','-Dgles1=disabled','-Dgles2=disabled','-Dopengl=true','-Dshared-glapi=true','-Dllvm=disabled','-Ddraw-use-llvm=false','-Dglvnd=false','-Dshader-cache=disabled','-Dxmlconfig=disabled','-Dzlib=disabled','-Dzstd=disabled','-Dlmsensors=disabled','-Dvideo-codecs=[]','-Dbuild-tests=false','-Db_pch=false','-Dc_winlibs=[]','-Dcpp_winlibs=[]',"-Dc_args=['/D_XBOX_UWP','/DWINAPI_FAMILY=WINAPI_FAMILY_APP']","-Dcpp_args=['/D_XBOX_UWP','/DWINAPI_FAMILY=WINAPI_FAMILY_APP']","-Dc_link_args=['/APPCONTAINER','/LIBPATH:$store','windowsapp.lib']","-Dcpp_link_args=['/APPCONTAINER','/LIBPATH:$store','windowsapp.lib']")
& meson @mesaArgs 2>&1|Tee-Object (Join-Path $OutputDirectory 'mesa-configure.log')
if($LASTEXITCODE){throw 'Mesa UWP configuration failed'}
& meson compile -C $build -j 4 opengl32 libgallium_wgl 2>&1|Tee-Object (Join-Path $OutputDirectory 'mesa-build.log')
if($LASTEXITCODE){throw 'Mesa UWP compilation failed'}
$files=@(Get-ChildItem $build -Recurse -File -Filter '*.dll')
if(!($files|Where-Object Name -eq 'opengl32.dll')-or !($files|Where-Object Name -eq 'libgallium_wgl.dll')){throw 'Required actual WGL libraries were not produced'}
foreach($f in $files){Copy-Item $f.FullName (Join-Path $OutputDirectory $f.Name);& dumpbin.exe /nologo /imports $f.FullName 2>&1|Tee-Object (Join-Path $OutputDirectory ($f.Name+'-imports.log'))}
Copy-Item (Join-Path $source 'mesa-source.tar.gz') $OutputDirectory;Copy-Item (Join-Path $source 'directx-headers-source.tar.gz') $OutputDirectory
Copy-Item (Join-Path $PSScriptRoot 'prepare_mesa.py') $OutputDirectory;Copy-Item (Join-Path $PSScriptRoot 'CoreWindowGdi.cpp') $OutputDirectory;Copy-Item (Join-Path $PSScriptRoot 'Build-Mesa.ps1') $OutputDirectory
@{scope='Mesa UWP candidate compilation only, not game/Xbox or API behavior';files=@($files|ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()}});xboxTested=$false;gameExecuted=$false}|ConvertTo-Json -Depth 5|Set-Content (Join-Path $OutputDirectory 'mesa-build.json')
