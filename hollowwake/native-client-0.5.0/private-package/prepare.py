"""Private static preparation; never executes game binaries or archive scripts.
Requires pefile from its maintained PyPI package, not from game inputs.
"""
from pathlib import Path
import argparse,hashlib,json,sys,shutil,collections
import pefile
ROOT=Path(__file__).resolve().parents[2]/'runtime-native-route'
sys.path.insert(0,str(ROOT/'native-image'));sys.path.insert(0,str(ROOT/'native-proxy'))
from convert_image import convert
from redirect_imports import redirect,SHORT
SHORT.update({"winmm.dll":"HWMM.dll","d3d9.dll":"HWD9.dll","xinput1_3.dll":"HWXI13.dll","xinput1_4.dll":"HWXI14.dll","xinput9_1_0.dll":"HWXI91.dll"})
from generate_native_proxy import emit
PIN='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def metadata(path):
 p=pefile.PE(str(path),fast_load=True)
 if p.FILE_HEADER.Machine!=0x8664:raise ValueError('Dependency is not x64: '+path.name)
 p.parse_data_directories(directories=[1,9,13]);deps=[]
 for attr in ['DIRECTORY_ENTRY_IMPORT','DIRECTORY_ENTRY_DELAY_IMPORT']:
  for d in getattr(p,attr,[]):deps.append(d.dll.decode('ascii').lower())
 p.close();return sorted(set(deps))
def prepare(source,out):
 source=Path(source).resolve();out=Path(out).resolve()
 if out.exists() or out==source or source in out.parents:raise ValueError('Use a new staging directory outside original inputs')
 main=source/'FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
 if sha(main)!=PIN:raise ValueError('Exact 6.21 main image hash mismatch')
 index=collections.defaultdict(list)
 for p in source.rglob('*.dll'):
  if p.is_symlink():raise ValueError('Symlink input rejected')
  if p.stat().st_size>512*1024**2:raise ValueError('Dependency exceeds analysis limit')
  try:
   pe=pefile.PE(str(p),fast_load=True)
   if pe.FILE_HEADER.Machine==0x8664:index[p.name.lower()].append(p)
   pe.close()
  except pefile.PEFormatError:raise ValueError('Invalid DLL: '+str(p.relative_to(source)))
 queue=[main];seen=set();images=[];external=set();duplicates={}
 while queue:
  p=queue.pop(0)
  if p in seen:continue
  seen.add(p);images.append(p)
  for name in metadata(p):
   # API-set forwarders are provided by Windows, not the archive's nonrelocatable shims.
   if name.startswith(('api-ms-','ext-ms-','xinput')):external.add(name);continue
   candidates=index.get(name,[])
   if not candidates:external.add(name);continue
   hashes={sha(q) for q in candidates}
   if len(hashes)!=1:
    adjacent=[q for q in candidates if q.parent==main.parent]
    if len(adjacent)!=1:raise ValueError('Ambiguous differing dependency copies: '+name)
    selected=adjacent[0]
    duplicates[name]={'policy':'Explicit private package choice: exact main-image adjacent copy; not proof of original DLL search behavior','selected':str(selected.relative_to(source)),'selectedSha256':sha(selected),'alternatives':[{'path':str(q.relative_to(source)),'sha256':sha(q)} for q in candidates]}
   else:
    selected=sorted(candidates)[0]
    if len(candidates)>1:duplicates[name]={'identicalCopies':[str(q.relative_to(source)) for q in candidates]}
   queue.append(selected)
 out.mkdir(parents=True);stage=out/'GamePayload';stage.mkdir();reports=out/'reports';reports.mkdir();proxies={};records=[]
 for i,p in enumerate(images):
  rel=p.relative_to(source).as_posix();original=sha(p)
  if p==main:
   intermediate=out/'ConvertedMain.private.dll';convert(p,intermediate,PIN)
   dest=stage/'6.21/FortniteGame/Binaries/Win64/Fortnite621.imports.private.dll';dest.parent.mkdir(parents=True)
  else:intermediate=p;dest=stage/p.name
  result=redirect(intermediate,dest,sha(intermediate))
  (reports/(str(i)+'.json')).write_text(json.dumps(result,indent=2))
  for item in result['proxies']:
   key=item['proxyDll'].lower()
   if key in proxies and proxies[key]['sourceDll'].lower()!=item['sourceDll'].lower():raise ValueError('Proxy name collision')
   merged=proxies.setdefault(key,{'sourceDll':item['sourceDll'],'proxyDll':item['proxyDll'],'symbols':[]});merged['symbols']=sorted(set(merged['symbols'])|set(item['symbols']))
  records.append({'originalRelativePath':'6.21/'+rel,'originalSha256':original,'packageRelativePath':dest.relative_to(stage).as_posix(),'sha256':sha(dest),'bytes':dest.stat().st_size,'mainImage':p==main})
  if sha(p)!=original:raise RuntimeError('Original changed')
 proxyRoot=out/'ProxySources';proxyRoot.mkdir()
 for item in proxies.values():
  # _iob is a legacy MSVCRT DATA export, not a callable function. A native
  # PE forwarder preserves the provider's actual object address and CRT ownership.
  if item['sourceDll'].lower()=='msvcrt.dll' and '_iob' in item['symbols']:item['dataSymbols']=['_iob']
  emit(item,proxyRoot/item['proxyDll'])
 manifest={'build':'6.21-CL-4526925','originalClientSha256':PIN,'files':records,'externalPlatformDependencies':sorted(external),'duplicateIdenticalInputs':duplicates,'proxyDlls':list(proxies.values()),'sourceUnchanged':True,'executed':False,'contentPaksIncluded':False,'compatibilityEstablished':False}
 (stage/'native-package-manifest.json').write_text(json.dumps(manifest,indent=2));(out/'plan.json').write_text(json.dumps(manifest,indent=2));return manifest
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('output');a=p.parse_args();m=prepare(a.source,a.output);print(json.dumps({'images':len(m['files']),'proxies':len(m['proxyDlls']),'externalPlatformDependencies':len(m['externalPlatformDependencies']),'executed':False}))
