from pathlib import Path
import json,zipfile,hashlib,re
root=Path('/workspace/hollowwake');out=root/'startup-continuation/Hollowwake-Native-Startup-Source.zip';entries={}
allowed={'.cs','.cpp','.h','.vcxproj','.csproj','.xaml','.appxmanifest','.py','.ps1','.yml','.json','.asm','.def','.md','.txt','.svg','.png','.jpg','.ttf','.xml'}
for folder in ['native-client-0.5.0','client-runtime/graphics']:
 for p in (root/folder).rglob('*'):
  rel=p.relative_to(root/folder)
  if not p.is_file() or set(rel.parts)&{'evidence','__pycache__','research','tools','bin','obj','artifacts','node_modules','GamePayload','ProxySources'} or p.suffix not in allowed:continue
  entries['hollowwake/'+p.relative_to(root).as_posix()]=p
entries['.github/workflows/hollowwake-native-client-050.yml']=root/'native-client-0.5.0/workflow.yml'
for name in ['NativeSystem.cpp','NativeSystem.h','NativeContracts.generated.h','NativeProxyDispatch.cpp','NativeProxyDispatch.h','GuestThreads.cpp','GuestThreads.h','ThreadImports.cpp','PackagedNamespace.h','native-image/convert_image.py','native-proxy/redirect_imports.py']:
 p=root/'runtime-native-route'/name;entries['hollowwake/'+p.relative_to(root).as_posix()]=p
for name in ['README.md','TEST-REPORT.md','REMAINING-MAIN.txt','RESULTS.json','make_source_bundle.py']:
 p=root/'startup-continuation'/name
 if p.exists():entries['hollowwake/startup-continuation/'+name]=p
for p in (root/'startup-continuation/evidence').rglob('*'):
 if p.is_file() and p.suffix in {'.json','.log','.txt'} and '.private.' not in p.name:entries['hollowwake/'+p.relative_to(root).as_posix()]=p
# Verify local quoted C/C++ include closure before packaging.
for name,p in list(entries.items()):
 if p.suffix not in {'.cpp','.h'}:continue
 for target in re.findall(r'^\s*#include "([^"]+)"',p.read_text(),re.M):
  q=(p.parent/target.replace('\\','/')).resolve()
  if q.exists() and 'hollowwake/'+q.relative_to(root).as_posix() not in entries:raise ValueError('Missing source include '+str(q))
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for name,p in sorted(entries.items()):
  data=p.read_bytes()
  if p.suffix not in {'.png','.jpg','.ttf'}:
   if any(v in data for v in [b'-----BEGIN '+b'PRIVATE KEY-----',b'-----BEGIN '+b'RSA PRIVATE KEY-----']) or re.search(rb'gh[pousr]_[A-Za-z0-9]{30,}',data):raise ValueError('Potential secret in '+name)
  z.writestr(name,data)
 z.writestr('README.txt','Hollowwake native startup source and actual hosted evidence. Not a working Fortnite game release. See hollowwake/startup-continuation/README.md and TEST-REPORT.md. No game binaries, private manifests, credentials, signing keys or live account data. Existing website/backend/rollback deployments were not changed.\n')
result={'file':str(out),'bytes':out.stat().st_size,'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'sourceEntries':len(entries),'gameFilesIncluded':False}
(root/'startup-continuation/source-hash.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
