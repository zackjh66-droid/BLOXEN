"""Fetch pinned, reviewed LGPL audio sources. Never reads/runs game inputs.
DSOAL uses the OpenAL Soft UWP WASAPI backend; this is a candidate until its
actual packaged module loading, COM and playback operations have been tested.
"""
import argparse,hashlib,json,tarfile,urllib.request
from pathlib import Path
PINS=[('dsoal','e11128250439d586483ee6598d1670ab52ab5e37','b55fe3845f0dc07082f56d4703ffe81f73b19f37f91159fa501ebc6d278bdab0'),('openal-soft','4a52288b772d1722713675f71bb30f321e78fd80','e06d35a855df060cf757a9162a4880b531f304cacbc29a6ea9a0be5809f25b56')]
def prepare(root):
 root=Path(root);root.mkdir(parents=True,exist_ok=False);records=[]
 for repo,pin,expected in PINS:
  url='https://codeload.github.com/kcat/'+repo+'/tar.gz/'+pin
  archive=root/(repo+'.tar.gz');urllib.request.urlretrieve(url,archive)
  if hashlib.sha256(archive.read_bytes()).hexdigest()!=expected:raise ValueError('Pinned source archive differs: '+repo)
  with tarfile.open(archive) as package:
   for member in package:
    p=Path(member.name)
    if p.is_absolute() or '..' in p.parts or not p.parts or p.parts[0]!=repo+'-'+pin or not (member.isfile() or member.isdir()):raise ValueError('Unexpected source archive entry')
   package.extractall(root,filter='data')
  source=root/(repo+'-'+pin);source.rename(root/repo)
  records.append({'repository':'https://github.com/kcat/'+repo,'commit':pin,'archiveSha256':expected,'license':'LGPL-2.1; upstream license and per-file notices govern','gameCodeIncluded':False})
 # DSOAL's only driver load must use the signed package graph, never CWD,
 # a Windows installation, downloaded writable files or arbitrary paths.
 p=root/'dsoal/src/dsoal.cpp';s=p.read_text();old='LoadLibraryW(std::data(aldriver_name))';assert s.count(old)==1;s=s.replace(old,'LoadPackagedLibrary(std::data(aldriver_name), 0)');p.write_text(s)
 # The public SDK hides DirectSound COM type declarations from UWP. Expose
 # those declarations in a narrow include wrapper; this enables no OS API and
 # supplies no implementation. Actual calls must still load/execute correctly.
 for header in ['dsound.h','dsconf.h','mmdeviceapi.h']:
  wrapper='hw_'+header
  (root/'dsoal/src'/wrapper).write_text('#pragma once\n#include <windows.h>\n#pragma push_macro("WINAPI_FAMILY")\n#undef WINAPI_FAMILY\n#define WINAPI_FAMILY WINAPI_FAMILY_DESKTOP_APP\n#include <'+header+'>\n#pragma pop_macro("WINAPI_FAMILY")\n')
  for file in (root/'dsoal/src').glob('*'):
   if file.suffix not in ['.h','.cpp'] or file.name.startswith('hw_'):continue
   text=file.read_text();text=text.replace('#include <'+header+'>','#include "'+wrapper+'"');file.write_text(text)
 p=root/'dsoal/src/dsoal.cpp';s=p.read_text();begin=s.index('        /* Increase refcount on dsound by 1 */');end=s.index('        break;',begin)
 s=s[:begin]+'        if(!HWRetainAudioModule(hInstDLL)) return FALSE;\n'+s[end:]
 s=s.replace('#define INITGUID','#define INITGUID\n#include <windows.h>\nextern "C" __declspec(dllimport) BOOL __cdecl HWRetainAudioModule(HMODULE);')
 begin=s.index('        if(const WCHAR *wstr{_wgetenv');end=s.index('        TRACE("Initializing library',begin)
 s=s[:begin]+'#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)\n'+s[begin:end]+'#endif\n'+s[end:];p.write_text(s)
 p=root/'dsoal/CMakeLists.txt';s=p.read_text();s+='\n# Hollowwake: candidate native DLL, no claim that flags alone port APIs.\nif(MSVC)\n target_link_options(dsound PRIVATE /APPCONTAINER)\n target_link_libraries(dsound PRIVATE "${HW_RUNTIME_LIBRARY}")\nendif()\n';p.write_text(s)
 # Pinned NuGet version from upstream, official feed only, no floating package.
 p=root/'openal-soft/CMakeLists.txt';s=p.read_text();old='-Version ${ALSOFT_CPPWINRT_VERSION} -ExcludeVersion';assert old in s;s=s.replace(old,old+' -Source https://api.nuget.org/v3/index.json');p.write_text(s)
 (root/'source-review.json').write_text(json.dumps({'sources':records,'changes':['DSOAL driver LoadPackagedLibrary namespace','UWP CRT and narrow public SDK COM declarations', 'real runtime module retain instead of unavailable static loader import', 'desktop environment log configuration excluded from UWP','official pinned CppWinRT feed'],'scriptsReviewed':['root CMakeLists','version.cmake','OpenAL embed-data cmake','vendored fmt CMakeLists'],'upstreamSourceArchivesRetained':True,'gameExecuted':False,'xboxTested':False},indent=2))
 print('Prepared pinned LGPL audio candidate sources; no game code read or executed')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output');prepare(p.parse_args().output)
