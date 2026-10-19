"""Prepare a pinned public Mesa UWP source candidate. No game inputs/execution.
The upstream UWP GDI placeholders are replaced, not counted as implementation.
Only the D3D12 renderer is selected; optional GPL swap allocator is disabled.
"""
from pathlib import Path
import argparse,hashlib,json,tarfile,urllib.request,posixpath,shutil
PIN='2c7c63ec2802571f8d1b7c038e8277d5db27aecf'
SHA='bfc09d77bed4c81a681fb68fe4f111d7945aabe549ea171a74ecc76dc62754f8'
def prepare(output):
 root=Path(output);root.mkdir(parents=True,exist_ok=False);archive=root/'mesa-source.tar.gz';url='https://codeload.github.com/aerisarn/mesa-uwp/tar.gz/'+PIN;urllib.request.urlretrieve(url,archive)
 if hashlib.sha256(archive.read_bytes()).hexdigest()!=SHA:raise ValueError('Pinned Mesa source differs')
 with tarfile.open(archive) as package:
  regular=[];links=[];prefix='mesa-uwp-'+PIN
  for member in package:
   p=Path(member.name)
   if p.is_absolute() or '..' in p.parts or not p.parts or p.parts[0]!=prefix:raise ValueError('Unsafe Mesa source entry')
   if member.issym():
    target=posixpath.normpath(posixpath.join(posixpath.dirname(member.name),member.linkname));parts=Path(target).parts
    if not parts or parts[0]!=prefix or '..' in parts or Path(member.linkname).is_absolute():raise ValueError('Mesa source link leaves pinned tree')
    links.append((member.name,target))
   elif member.isfile() or member.isdir():regular.append(member)
   else:raise ValueError('Unexpected Mesa source entry type')
  package.extractall(root,members=regular,filter='data')
  # Windows needs no symlink privilege: materialize only verified internal source
  # links after all regular targets exist. The original tar remains unchanged.
  for name,target in links:
   original=root/target;destination=root/name
   if not original.exists() or destination.exists() or original.is_symlink():raise ValueError('Invalid source link target')
   if original.is_dir():shutil.copytree(original,destination)
   else:shutil.copy2(original,destination)
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
