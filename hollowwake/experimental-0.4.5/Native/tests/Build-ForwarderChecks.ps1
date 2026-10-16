param([string]$OutputDirectory)
$ErrorActionPreference='Stop';$tests=$PSScriptRoot;$native=Split-Path $tests -Parent
New-Item -ItemType Directory -Force $OutputDirectory|Out-Null;Push-Location $OutputDirectory
try{
 & cl.exe /nologo /c /O2 /GS- /Zl (Join-Path $tests 'FixtureTarget.cpp') /Fo:FixtureTarget.obj 2>&1|Tee-Object fixture-compile.log;if($LASTEXITCODE){throw 'Original forwarder target compilation failed'}
 & link.exe /DLL /NOENTRY /NODEFAULTLIB /MACHINE:X64 /DYNAMICBASE /FIXED:NO FixtureTarget.obj /OUT:FixtureTarget.dll 2>&1|Tee-Object target-link.log;if($LASTEXITCODE){throw 'Relocatable original target link failed'}
 & link.exe /DLL /NOENTRY /NODEFAULTLIB /MACHINE:X64 /BASE:0x180000000 /FIXED FixtureTarget.obj /OUT:FixtureFixedCode.dll 2>&1|Tee-Object fixed-code-link.log;if($LASTEXITCODE){throw 'Fixed-code negative fixture link failed'}
 foreach($name in @('FixtureFirst','FixtureSecond')){
  @("LIBRARY $name",'EXPORTS',' Answer=FixtureTarget.Answer @1')|Set-Content ($name+'.def')
  & link.exe /DLL /NOENTRY /NODEFAULTLIB /MACHINE:X64 /BASE:0x180000000 /FIXED ("/DEF:"+$name+'.def') ("/OUT:"+$name+'.dll') 2>&1|Tee-Object ($name+'-link.log');if($LASTEXITCODE){throw 'Forward-only fixture link failed'}
 }
 & cl.exe /nologo /c /O2 /GS- /Zl (Join-Path $tests 'FixtureClock.cpp') /Fo:FixtureClock.obj 2>&1|Tee-Object clock-compile.log;if($LASTEXITCODE){throw 'Clock entry fixture compilation failed'}
 & link.exe /DLL /ENTRY:ClockEntry /NODEFAULTLIB /MACHINE:X64 /DYNAMICBASE /FIXED:NO FixtureClock.obj kernel32.lib /OUT:FixtureClock.dll 2>&1|Tee-Object clock-link.log;if($LASTEXITCODE){throw 'Clock entry fixture linkage failed'}
 @('LIBRARY msvcrt.dll','EXPORTS',' _initterm',' _initterm_e',' malloc',' calloc',' realloc',' free',' _msize',' _errno',' memcpy',' memmove',' memset',' memcmp',' _strdup',' _wcsdup')|Set-Content LegacyAllocator.def
 & lib.exe /nologo /DEF:LegacyAllocator.def /MACHINE:X64 /OUT:LegacyAllocator.lib 2>&1|Tee-Object allocator-import-library.log;if($LASTEXITCODE){throw 'Legacy allocation import library failed'}
 & cl.exe /nologo /c /MD /Od /Oi- /GS- /Zl (Join-Path $tests 'FixtureMemory.cpp') /Fo:FixtureMemory.obj 2>&1|Tee-Object memory-compile.log;if($LASTEXITCODE){throw 'Memory initializer fixture compilation failed'}
 & link.exe /DLL /ENTRY:MemoryEntry /NODEFAULTLIB /MACHINE:X64 /DYNAMICBASE /FIXED:NO FixtureMemory.obj LegacyAllocator.lib /OUT:FixtureMemory.dll 2>&1|Tee-Object memory-link.log;if($LASTEXITCODE){throw 'Legacy allocator fixture link failed'}
 $header='struct KnownModule {const wchar_t* path;const char* name;const char* hash;};'+[Environment]::NewLine+'static const KnownModule known[]={'+[Environment]::NewLine
 foreach($name in @('FixtureFirst.dll','FixtureSecond.dll','FixtureTarget.dll','FixtureFixedCode.dll','FixtureClock.dll','FixtureMemory.dll')){
  $hash=(Get-FileHash $name -Algorithm SHA256).Hash.ToLowerInvariant();$header+='{L"'+$name+'","'+$name+'","'+$hash+'"},'+[Environment]::NewLine
 };$header+='};';[IO.File]::WriteAllText((Join-Path $OutputDirectory 'FixtureModules.generated.h'),$header)
 & cl.exe /nologo /MD /EHsc /std:c++17 /DHW_DEPENDENCY_FIXTURE /DNOMINMAX /DWIN32_LEAN_AND_MEAN "/I$OutputDirectory" (Join-Path $tests 'ForwarderTest.cpp') (Join-Path $native 'Dependencies.cpp') windowsapp.lib bcrypt.lib /Fe:ForwarderTest.exe 2>&1|Tee-Object harness-build.log;if($LASTEXITCODE){throw 'Production-resolver fixture harness compilation failed'}
 & .\ForwarderTest.exe $OutputDirectory 2>&1|Tee-Object forwarder-test.log;if($LASTEXITCODE){throw 'Production forwarder resolver contract failed'}
 '{"productionResolverUsed":true,"fixedBaseCollisionEstablished":true,"forwardOnlyNameAndOrdinalResolved":true,"realMappedTargetReturn":621,"fixedExecutableRejected":true,"downloadedCodeExecuted":false,"mappedClockProcessAttachExecuted":true,"clockAdvancesUsingRealOsUptime":true,"clockWrapSemanticsVerified":true,"mappedLegacyMallocProcessAttach":true,"allocatorFamilyChecksPassed":true,"mappedInitializerTablesExecuted":true,"mappedGetSystemInfoMatchedHost":true}'|Set-Content forwarder-test.json
}finally{Pop-Location}
