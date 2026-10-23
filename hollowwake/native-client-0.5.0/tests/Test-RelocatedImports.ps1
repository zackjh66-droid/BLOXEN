param([string]$OutputDirectory)
$ErrorActionPreference='Stop'
$repo=Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$fixture=Join-Path $repo 'runtime-native-route\native-image'
$redirect=Join-Path $repo 'runtime-native-route\native-proxy\redirect_imports.py'
$work=Join-Path $OutputDirectory 'OwnedRelocatedImports'
New-Item -ItemType Directory -Force $work|Out-Null
Push-Location $work
try{
 & (Join-Path $fixture 'Build-NativeImage.ps1') -OutputDirectory $work
 if($LASTEXITCODE){throw 'Original native-loader lifecycle fixture failed'}
 $digest=(Get-FileHash OriginalConvertedFixture.dll -Algorithm SHA256).Hash.ToLowerInvariant()
 & python $redirect OriginalConvertedFixture.dll RelocatedFixture.dll --expected-sha256 $digest --report relocated-imports.json --preserve-original-sections
 if($LASTEXITCODE){throw 'Section-preserving normal import preparation failed'}
 $plan=Get-Content relocated-imports.json -Raw|ConvertFrom-Json
 if(!$plan.ownedImportMetadata.originalSectionBytesPreserved-or !$plan.ownedImportMetadata.originalDelayDescriptorsPreserved-or !$plan.ownedImportMetadata.lookupAndIatRvasRetained){throw 'Original fixture sections changed'}
 foreach($proxy in $plan.proxies){
  if($proxy.sourceDll-notmatch '^(kernel32|ntdll)\.dll$'){throw 'Unexpected owned fixture dependency'}
  $provider=$proxy.sourceDll.Substring(0,$proxy.sourceDll.Length-4)
  $lines=@("LIBRARY $($proxy.proxyDll)",'EXPORTS')
  foreach($symbol in $proxy.symbols){
   if($symbol-notmatch '^[A-Za-z_][A-Za-z0-9_]*$'){throw 'Unexpected owned fixture symbol'}
   $lines+=(' '+$symbol+'='+$provider+'.'+$symbol)
  }
  $definition=$proxy.proxyDll+'.def';$lines|Set-Content -Encoding ASCII $definition
  # Stateless OS forwarder only: no simulated API returns or client DllMain changes.
  & link.exe /nologo /DLL /NOENTRY /MACHINE:X64 "/DEF:$definition" "/OUT:$($proxy.proxyDll)"
  if($LASTEXITCODE){throw 'Real OS forwarder compilation failed'}
 }
 & ./NativeImageTest.exe (Join-Path $work 'RelocatedFixture.dll')
 if($LASTEXITCODE){throw 'Relocated import fixture failed real OS entry/TLS/thread/unwind/export checks'}
 & cl.exe /nologo /std:c++17 /EHsc /MD /W4 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /DHW_RESTORE_HEADER_BASE (Join-Path $fixture 'NativeImageTest.cpp') kernel32.lib /Fe:HeaderInitializedHost.exe
 if($LASTEXITCODE){throw 'Measured header-initialization host compilation failed'}
 & ./HeaderInitializedHost.exe (Join-Path $work 'RelocatedFixture.dll')
 if($LASTEXITCODE){throw 'Real OS entry/TLS/thread/unwind/unload changed after measured header initialization'}
 'PASS relocated import directory: real OS forwarded imports, original entry, thread/TLS/unwind and forwarded export; Windows fixture only.'
}finally{Pop-Location}
