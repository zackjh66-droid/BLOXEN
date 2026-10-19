"""Prepare a pinned public Mesa UWP source candidate. No game inputs/execution.
The upstream UWP GDI placeholders are replaced, not counted as implementation.
Only the D3D12 renderer is selected; optional GPL swap allocator is disabled.
"""
from pathlib import Path
import argparse,hashlib,json,tarfile,urllib.request
PIN='2c7c63ec2802571f8d1b7c038e8277d5db27aecf'
SHA='bfc09d77bed4c81a681fb68fe4f111d7945aabe549ea171a74ecc76dc62754f8'
def prepare(output):
 root=Path(output);root.mkdir(parents=True,exist_ok=False);archive=root/'mesa-source.tar.gz';url='https://codeload.github.com/aerisarn/mesa-uwp/tar.gz/'+PIN;urllib.request.urlretrieve(url,archive)
 if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA:raise ValueError('Pinned Mesa source differs')
 with tarfile.open(archive) as package:
  for member in package:
   p=Path(member.name)
   if p.is_absolute() or '..' in p.parts or not p.parts or p.parts[0]!='mesa-uwp-'+PIN or not (member.isdir() or member.isfile()):raise ValueError('Unsafe/unexpected Mesa source entry')
  package.extractall(root,filter='data')
 source=root/('mesa-uwp-'+PIN);source.rename(root/'mesa');source=root/'mesa'
 # Official public DirectX header source, pinned to the fork's selected release.
 dxpin='c0f2af8af649833910786f38eb79cc333c77b1a6';dxsha='946e863c684c9264d21f890e3961d55f9a0318981865cf2a82e9afd4d72e3898'
 dxarchive=root/'directx-headers-source.tar.gz';urllib.request.urlretrieve('https://codeload.github.com/microsoft/DirectX-Headers/tar.gz/'+dxpin,dxarchive)
 if hashlib.sha256(dxarchive.read_bytes()).hexdigest()!=dxsha:raise ValueError('Public DirectX header source differs')
 with tarfile.open(dxarchive) as package:
  for member in package:
   p=Path(member.name)
   if p.is_absolute() or '..' in p.parts or not p.parts or p.parts[0]!='DirectX-Headers-'+dxpin or not (member.isfile() or member.isdir()):raise ValueError('Unexpected DirectX header source entry')
  package.extractall(root,filter='data')
 (root/('DirectX-Headers-'+dxpin)).rename(source/'subprojects/DirectX-Headers-1.0')
 options=source/'meson_options.txt';text=options.read_text();assert "'uwp'" not in text;text+="\noption('uwp', type : 'boolean', value : true, description : 'Use the public CoreWindow UWP backend')\n";options.write_text(text)
 gdi=source/'src/gallium/winsys/uwp/gdi_uwp.cpp';gdi.write_text((Path(__file__).parent/'CoreWindowGdi.cpp').read_text())
 p=source/'src/gallium/winsys/uwp/meson.build';text=p.read_text().replace("  cpp_args : '/ZW',","  cpp_args : [],");p.write_text(text)
 # A Mesa-selected system/provider module must come from the package graph.
 # Arbitrary writable-directory DLL loading is not permitted in this port.
 p=source/'src/util/u_dl.c';text=p.read_text();old='return (struct util_dl_library *)LoadLibraryA(filename);';assert text.count(old)==1
 text=text.replace(old,'''wchar_t wide[260];
   if (!filename || !filename[0]) { SetLastError(ERROR_INVALID_NAME); return NULL; }
   for (const char *p = filename; *p; ++p)
      if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '.' || *p == '_' || *p == '-')) { SetLastError(ERROR_INVALID_NAME); return NULL; }
   if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename, -1, wide, 260)) return NULL;
   return (struct util_dl_library *)LoadPackagedLibrary(wide, 0);''');p.write_text(text)
 (root/'mesa-source-review.json').write_text(json.dumps({'repository':'https://github.com/aerisarn/mesa-uwp','commit':PIN,'archiveSha256':SHA,'license':'Mesa MIT/BSD/per-file notices; optional xbox-swap allocator not selected','scope':'Candidate source preparation, not API behavior or Xbox execution','changes':['explicit missing uwp option','replace GDI placeholders with validated CoreWindow/viewport state','no busy-wait UI dispatcher or fake Create/DestroyWindow success','restrict dynamic DLL loading to package module basenames'],'gameExecuted':False,'xboxTested':False},indent=2))
 print('Prepared pinned Mesa UWP candidate and original CoreWindow contract; no game code executed')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('output');prepare(p.parse_args().output)
