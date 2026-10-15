from pathlib import Path
import subprocess,json,base64
root=Path('/workspace/hollowwake');repo='zackjh66-droid/BLOXEN';branch='hollowwake/client-runtime-0.3.2'
def api(path,data=None,method=None):
 cmd=['gh','api','repos/'+repo+'/'+path]
 if data is not None:cmd+=['--method',method or 'POST','--input','-']
 r=subprocess.run(cmd,input=json.dumps(data) if data is not None else None,text=True,capture_output=True)
 if r.returncode:raise RuntimeError(r.stderr[:500])
 return json.loads(r.stdout) if r.stdout.strip() else {}
head=api('git/ref/heads/main')['object']['sha'];base=api('git/commits/'+head)['tree']['sha']
workflow=(root/'client-runtime/workflow.yml').read_text()
# Register guarded manual-only workflow on main. No automatic jobs or paid runner.
t=api('git/trees',{'base_tree':base,'tree':[{'path':'.github/workflows/hollowwake-client-runtime.yml','mode':'100644','type':'blob','content':workflow}]})
c=api('git/commits',{'message':'Register guarded original client-runtime checks','tree':t['sha'],'parents':[head]});api('git/refs/heads/main',{'sha':c['sha'],'force':False},'PATCH')
entries=[]
for f in (root/'client-runtime').rglob('*'):
 if f.is_file() and not {'evidence','__pycache__'}&set(f.relative_to(root).parts):
  entries.append({'path':'hollowwake/'+f.relative_to(root).as_posix(),'mode':'100644','type':'blob','content':f.read_text()})
t=api('git/trees',{'base_tree':t['sha'],'tree':entries});b=api('git/commits',{'message':'Implement original OS-assigned static TLS donor and CoreWindow DXGI bridge','tree':t['sha'],'parents':[c['sha']]})
try:api('git/refs',{'ref':'refs/heads/'+branch,'sha':b['sha']})
except:api('git/refs/heads/'+branch,{'sha':b['sha'],'force':False},'PATCH')
api('actions/workflows/hollowwake-client-runtime.yml/dispatches',{'ref':branch,'inputs':{'run_checks':True}})
r={'commit':b['sha'],'branch':branch,'workflow':'hollowwake-client-runtime.yml','dispatched':True};(root/'client-runtime/evidence/source.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
