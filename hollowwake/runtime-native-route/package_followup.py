from pathlib import Path
import subprocess,json,base64,zipfile,hashlib,urllib.request
root=Path(__file__).parent
out=Path('/workspace/hollowwake/native-route-delivery/Hollowwake-Native-Loader-Source.zip')
# Export own scoped source and safe verification logs only; never private input binaries.
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(root.rglob('*')):
  if not p.is_file() or '__pycache__' in p.parts:continue
  rel=p.relative_to(root)
  if rel.parts[0]=='evidence':
   if p.name not in {'namespace-build.log','namespace-results.json'}:continue
  elif p.suffix not in {'.cpp','.h','.md','.ps1','.yml','.vcxproj'}:continue
  data=p.read_bytes()
  if b'-----BEGIN PRIVATE KEY-----' in data:raise RuntimeError('Private key marker detected')
  z.writestr('runtime-native-route/'+str(rel),data)
repo='zackjh66-droid/BLOXEN';branch='hollowwake/native-system-route'
def api(path,payload=None,method='POST'):
 cmd=['gh','api','repos/'+repo+'/'+path]
 if payload is not None:cmd+=['--method',method,'--input','-']
 p=subprocess.run(cmd,input=json.dumps(payload) if payload is not None else None,capture_output=True,text=True,check=True)
 return json.loads(p.stdout)
blob=api('git/blobs',{'content':base64.b64encode(out.read_bytes()).decode(),'encoding':'base64'})
head=api('git/ref/heads/'+branch)['object']['sha'];base=api('git/commits/'+head)['tree']['sha']
path='native-route-downloads/'+out.name
result=json.loads((root/'evidence/namespace-results.json').read_text())
tree=api('git/trees',{'base_tree':base,'tree':[{'path':path,'mode':'100644','type':'blob','sha':blob['sha']},{'path':'native-route-downloads/Native-Loader-Results.json','mode':'100644','type':'blob','content':json.dumps(result,indent=2)+'\n'}]})
commit=api('git/commits',{'message':'Retain native loader source and actual hosted verification logs; no Xbox release','parents':[head],'tree':tree['sha']})['sha']
api('git/refs/heads/'+branch,{'sha':commit,'force':False},'PATCH')
url='https://raw.githubusercontent.com/'+repo+'/'+commit+'/'+path
with urllib.request.urlopen(url,timeout=45) as response:data=response.read()
if data!=out.read_bytes():raise RuntimeError('Source download verification failed')
info={'commit':commit,'url':url,'size':len(data),'sha256':hashlib.sha256(data).hexdigest(),'fortniteExecuted':False,'xboxTested':False,'appxProduced':False}
(out.parent/'loader-source-links.json').write_text(json.dumps(info,indent=2)+'\n')
print(json.dumps(info))
