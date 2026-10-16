# Data-only approximation of loader graph validation. No downloaded code executes.
import pefile,re,json,collections
from pathlib import Path
p=Path('/workspace/hollowwake/experimental-0.4');root=Path('/workspace/hollowwake/build-work');rows=re.findall(r'\{L"([^"]+)","([^"]+)","([^"]+)"\}',(p/'Native/KnownModules.h').read_text());known=collections.defaultdict(list)
for path,name,h in rows:known[name.lower()].append(root/path.replace('\\\\','/'))
cache={};fail=[];slots=0
hooks={'xinput1_3.dll','d3d11.dll','dxgi.dll'}
def load(key):
 global slots
 if key in cache:return cache[key]
 files=known[key];preferred=[f for f in files if f.parent==root/'6.21/FortniteGame/Binaries/Win64'];f=(preferred or files)[0];q=pefile.PE(str(f),fast_load=True);q.parse_data_directories(directories=[0,1,3,9,13]);cache[key]=q;o=q.OPTIONAL_HEADER
 if q.FILE_HEADER.SizeOfOptionalHeader!=240 or o.NumberOfRvaAndSizes<16:raise ValueError('invalid dependency PE limits')
 if hasattr(q,'DIRECTORY_ENTRY_TLS'):
  slots+=1
  if slots>=32:raise ValueError('dependency TLS limits')
 for ex in getattr(q,'DIRECTORY_ENTRY_EXCEPTION',[]):
  e=ex.struct
  if e.BeginAddress>=e.EndAddress or e.EndAddress>o.SizeOfImage:raise ValueError('dependency unwind range')
  if e.UnwindData>=o.SizeOfImage:raise ValueError('dependency unwind data')
 for d in [*getattr(q,'DIRECTORY_ENTRY_IMPORT',[]),*getattr(q,'DIRECTORY_ENTRY_DELAY_IMPORT',[])]:
  if hasattr(d.struct,'grAttrs') and d.struct.grAttrs!=1:raise ValueError('dependency delay-import format')
  for x in d.imports:resolve(d.dll.decode(),x.name.decode() if x.name else '#'+str(x.ordinal))
 return q
def resolve(dll,name):
 key=dll.lower()
 if key in hooks or key in ['kernel32.dll','kernelbase.dll'] or key.startswith('api-ms-win-core-'):return
 if key not in known:return
 try:
  q=load(key)
  if not hasattr(q,'DIRECTORY_ENTRY_EXPORT'):raise ValueError('dependency export table absent')
  e=q.DIRECTORY_ENTRY_EXPORT
  if e.struct.NumberOfNames>65536:raise ValueError('dependency export name count')
  for x in e.symbols:
   if x.name and x.name.decode()==name or name=='#'+str(x.ordinal):
    if x.forwarder:
     module,symbol=x.forwarder.decode().split('.',1);resolve(module+'.dll',symbol)
    return
 except Exception as err:fail.append({'module':key,'symbol':name,'failure':str(err)})
q=pefile.PE(str(root/'6.21/FortniteGame/Binaries/Win64/FortniteClient-Win64-Shipping.exe'),fast_load=True);q.parse_data_directories(directories=[1,13])
for d in [*q.DIRECTORY_ENTRY_IMPORT,*q.DIRECTORY_ENTRY_DELAY_IMPORT]:
 for x in d.imports:resolve(d.dll.decode(),x.name.decode() if x.name else '#'+str(x.ordinal))
r={'scope':'Static graph only; OS allocation/TLS/protection/unwind API outcomes cannot be reproduced here','loadedModules':len(cache),'tlsDonorsNeeded':slots,'failures':fail};(p/'evidence/static-graph-result.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2))
