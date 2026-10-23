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
from audit_original_code import audit_bytes as audit_original_code
SHORT.update({"imm32.dll":"HWIM.dll","opengl32.dll":"HWGL.dll","dwmapi.dll":"HWDWM.dll","dsound.dll":"HWDS.dll","sensapi.dll":"HWSENS.dll","winmm.dll":"HWMM.dll","d3d9.dll":"HWD9.dll","xinput1_3.dll":"HWXI13.dll","xinput1_4.dll":"HWXI14.dll","xinput9_1_0.dll":"HWXI91.dll"})
from generate_native_proxy import emit
from patch_service_hosts import patch as patch_service_hosts
from patch_stack_address import patch as patch_stack_address
from patch_windowed_viewport import audit as audit_windowed_viewport
PIN='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def metadata(path):
 p=pefile.PE(str(path),fast_load=True)
 if p.FILE_HEADER.Machine!=0x8664:raise ValueError('Dependency is not x64: '+path.name)
 p.parse_data_directories(directories=[1,9,13]);deps=[]
 for attr in ['DIRECTORY_ENTRY_IMPORT','DIRECTORY_ENTRY_DELAY_IMPORT']:
  for d in getattr(p,attr,[]):deps.append(d.dll.decode('ascii').lower())
 p.close();return sorted(set(deps))
def select_images(source,main):
 index=collections.defaultdict(list)
 for p in source.rglob('*.dll'):
  if p.is_symlink():raise ValueError('Symlink input rejected')
  if p.stat().st_size>512*1024**2:raise ValueError('Dependency exceeds analysis limit')
  try:
   pe=pefile.PE(str(p),fast_load=True)
   if pe.FILE_HEADER.Machine==0x8664:index[p.name.lower()].append(p)
   pe.close()
  except pefile.PEFormatError:raise ValueError('Invalid DLL: '+str(p.relative_to(source)))
 # These modules are loaded by name during real observed main initialization,
 # not represented by normal or delayed PE import descriptors.
 dynamic=['Engine/Binaries/ThirdParty/PhysX3/Win64/VS2015/APEX_Legacy_x64.dll','Engine/Binaries/ThirdParty/PhysX3/Win64/VS2015/APEX_Clothing_x64.dll']
 pins=json.loads((Path(__file__).resolve().parents[1]/'App/boot-inputs.json').read_text())
 expected={item['path'].casefold():item for item in pins['files']}
 queue=[main]+[source/name for name in dynamic];seen=set();images=[];external=set();duplicates={}
 while queue:
  p=queue.pop(0)
  if p in seen:continue
  pin=expected.get(('6.21/'+p.relative_to(source).as_posix()).casefold())
  if not pin or not p.is_file() or p.is_symlink() or p.stat().st_size!=pin['bytes'] or sha(p)!=pin['sha256']:raise ValueError('Selected native image differs from exact preserved build: '+p.name)
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
 # Validate every private-provider export against the same pinned original
 # images before conversion. This is static graph evidence, not behavior proof.
 providers={p.name.lower():p for p in images};exports={};privateBindings=[]
 for consumer in images:
  pe=pefile.PE(str(consumer),fast_load=True);pe.parse_data_directories(directories=[1,13])
  for attr in ['DIRECTORY_ENTRY_IMPORT','DIRECTORY_ENTRY_DELAY_IMPORT']:
   for descriptor in getattr(pe,attr,[]):
    name=descriptor.dll.decode('ascii').lower()
    if name not in providers:continue
    if name not in exports:
     target=pefile.PE(str(providers[name]),fast_load=True);target.parse_data_directories(directories=[0]);symbols=getattr(getattr(target,'DIRECTORY_ENTRY_EXPORT',None),'symbols',[])
     exports[name]=({x.name.decode('ascii') for x in symbols if x.name and x.address},{x.ordinal for x in symbols if x.address});target.close()
    named,ordinals=exports[name]
    for symbol in descriptor.imports:
     spelling=symbol.name.decode('ascii') if symbol.name else '#'+str(symbol.ordinal);found=spelling in named if symbol.name else symbol.ordinal in ordinals
     if not found:raise ValueError('Missing original private-provider export: '+name+'!'+spelling)
     privateBindings.append({'consumer':consumer.name,'provider':name,'symbol':spelling,'kind':'normal' if attr=='DIRECTORY_ENTRY_IMPORT' else 'delay','exportDeclared':True})
  pe.close()
 return images,external,duplicates,dynamic,privateBindings

def prepare(source,out,initialized_capture=None):
 source=Path(source).resolve();out=Path(out).resolve()
 if out.exists() or out==source or source in out.parents:raise ValueError('Use a new staging directory outside original inputs')
 main=source/'FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'
 if sha(main)!=PIN:raise ValueError('Exact 6.21 main image hash mismatch')
 images,external,duplicates,dynamic,privateBindings=select_images(source,main)
 out.mkdir(parents=True);stage=out/'GamePayload';stage.mkdir();reports=out/'reports';reports.mkdir();proxies={};records=[]
 initialized=None;normalized=None
 if initialized_capture:
  sys.path.insert(0,str(ROOT.parent/'startup-investigation'))
  from prepare_initialized_image import prepare as normalize_initialized
  normalized=out/'InitializedMain.private.exe'
  initialized=normalize_initialized(main,initialized_capture,normalized,reports/'initialized-preparation.json')
  captured=json.loads((Path(initialized_capture)/'capture.json').read_text())
  regions=captured.get('protections',[]);previous=0
  if not regions or len(regions)>4096:raise ValueError('Measured original image protections required')
  for region in regions:
   rva,n,protect=(region[k] for k in ('rva','bytes','protect'))
   if rva<previous or rva%4096 or n<=0 or n%4096 or rva>=0x6314000 or n>0x6314000-rva or protect not in (1,2,4,8,16,32,64,128):raise ValueError('Invalid measured image protection')
   previous=rva+n
  initialized['protections']=regions;initialized['originalImageBytes']=0x6314000
  initialized['historicalStackRepairApplied']=False

 for i,p in enumerate(images):
  rel=p.relative_to(source).as_posix();original=sha(p)
  if p==main:
   intermediate=out/'ConvertedMain.private.dll';conversion_source=normalized if initialized else p;convert(conversion_source,intermediate,sha(conversion_source))
   dest=stage/'6.21/FortniteGame/Binaries/Win64/Fortnite621.imports.private.dll';dest.parent.mkdir(parents=True)
  else:intermediate=p;dest=stage/p.name
  result=redirect(intermediate,dest,sha(intermediate),preserve_sections=(p==main and bool(initialized)))
  if p==main:
   hostPatch=patch_service_hosts(dest,sha(dest)) if initialized else patch_service_hosts(dest);result['serviceHostPatch']=hostPatch
   (reports/'service-host-patch.json').write_text(json.dumps(hostPatch,indent=2))
   stackPatch=None if initialized else patch_stack_address(dest);result['stackAddressRepair']=stackPatch
   viewportPatch=None;viewportAudit=audit_windowed_viewport(dest,sha(dest)) if initialized else None
   result['windowedViewportPatch']=viewportPatch;result['windowedViewportAudit']=viewportAudit
   codeAudit=audit_original_code(normalized.read_bytes(),dest.read_bytes(),initialized['outputSha256']) if initialized else None
   result['originalCodeAudit']=codeAudit;mainImportMetadata=result['ownedImportMetadata']
   (reports/'original-code-audit.json').write_text(json.dumps(codeAudit,indent=2))
   (reports/'windowed-viewport-audit.json').write_text(json.dumps(viewportAudit,indent=2))
   result['outputSha256']=sha(dest);result['outputBytes']=dest.stat().st_size
   (reports/'stack-address-repair.json').write_text(json.dumps(stackPatch,indent=2))
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
 manifest={'build':'6.21-CL-4526925','originalClientSha256':PIN,'files':records,'privateDependencyExportAudit':{'scope':'Pinned static private-provider exports; runtime behavior and Xbox remain unverified','bindings':privateBindings,'missing':0},'startupDynamicImages':dynamic,'externalPlatformDependencies':sorted(external),'duplicateIdenticalInputs':duplicates,'proxyDlls':list(proxies.values()),'sourceUnchanged':True,'executed':False,'contentPaksIncluded':False,'compatibilityEstablished':False,'serviceHostPatch':hostPatch,'windowedViewportPatch':viewportPatch,'windowedViewportAudit':viewportAudit,'stackAddressRepair':stackPatch,'initializedClient':initialized,'originalCodeAudit':codeAudit,'mainOwnedImportMetadata':mainImportMetadata}
 (stage/'native-package-manifest.json').write_text(json.dumps(manifest,indent=2));(out/'plan.json').write_text(json.dumps(manifest,indent=2));return manifest
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('output');p.add_argument('--initialized-capture');a=p.parse_args();m=prepare(a.source,a.output,a.initialized_capture);print(json.dumps({'images':len(m['files']),'proxies':len(m['proxyDlls']),'externalPlatformDependencies':len(m['externalPlatformDependencies']),'executed':False}))
