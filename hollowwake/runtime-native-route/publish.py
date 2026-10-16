from pathlib import Path
import subprocess,json
root=Path(__file__).parent;repo='zackjh66-droid/BLOXEN';branch='hollowwake/native-system-route';workflow='hollowwake-native-system.yml'
def api(path,data=None,method='POST'):
 cmd=['gh','api','repos/'+repo+'/'+path]
 if data is not None:cmd+=['--method',method,'--input','-']
 p=subprocess.run(cmd,input=json.dumps(data) if data is not None else None,text=True,capture_output=True)
 if p.returncode:raise RuntimeError(p.stderr[:350])
 return json.loads(p.stdout) if p.stdout.strip() else {}
try:head=api('git/ref/heads/'+branch)['object']['sha'];exists=True
except RuntimeError:head=api('git/ref/heads/hollowwake/client-experiment-0.4.7')['object']['sha'];exists=False
entries=[]
for f in root.rglob('*'):
 if not f.is_file() or {'__pycache__','evidence'}&set(f.relative_to(root).parts):continue
 if f.suffix not in {'.cpp','.h','.md','.ps1','.py','.yml','.vcxproj'}:continue
 entries.append({'path':'hollowwake/runtime-native-route/'+f.relative_to(root).as_posix(),'mode':'100644','type':'blob','content':f.read_text()})
for name,source in [('Runtime.cpp','Runtime.integration.cpp'),('Native.vcxproj','Native.integration.vcxproj'),('ModuleLoader.cpp','ModuleLoader.integration.cpp'),('ModuleLoader.h','ModuleLoader.integration.h')]:entries.append({'path':'hollowwake/experimental-0.4.7/Native/'+name,'mode':'100644','type':'blob','content':(root/source).read_text()})
entry={'path':'.github/workflows/'+workflow,'mode':'100644','type':'blob','content':(root/'workflow.yml').read_text()};entries.append(entry)
base=api('git/commits/'+head)['tree']['sha'];tree=api('git/trees',{'base_tree':base,'tree':entries});c=api('git/commits',{'message':'Investigate original loaded-system API resolver; owned-code checks only, no game content or Xbox release','parents':[head],'tree':tree['sha']})
if exists:api('git/refs/heads/'+branch,{'sha':c['sha'],'force':False},'PATCH')
else:api('git/refs',{'ref':'refs/heads/'+branch,'sha':c['sha']})
h=api('git/ref/heads/main')['object']['sha'];t=api('git/trees',{'base_tree':api('git/commits/'+h)['tree']['sha'],'tree':[entry]});mc=api('git/commits',{'message':'Register guarded native-system investigation workflow','parents':[h],'tree':t['sha']});api('git/refs/heads/main',{'sha':mc['sha'],'force':False},'PATCH')
print(json.dumps({'commit':c['sha'],'branch':branch,'workflow':workflow}))
