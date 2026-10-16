from pathlib import Path
import subprocess,json,base64,sys
root=Path('/workspace/hollowwake');p=root/'experimental-0.4.1';repo='zackjh66-droid/BLOXEN';branch='hollowwake/client-experiment-0.4.1'
def api(path,data=None,method=None):
 cmd=['gh','api','repos/'+repo+'/'+path]
 if data is not None:cmd+=['--method',method or 'POST','--input','-']
 r=subprocess.run(cmd,input=json.dumps(data) if data is not None else None,text=True,capture_output=True)
 if r.returncode:raise RuntimeError(r.stderr[:500])
 return json.loads(r.stdout) if r.stdout.strip() else {}
try:head=api('git/ref/heads/'+branch)['object']['sha']
except:head=api('git/ref/heads/main')['object']['sha']
base=api('git/commits/'+head)['tree']['sha'];entries=[]
for source in [p,root/'client-runtime']:
 for f in sorted(source.rglob('*')):
  if not f.is_file() or {'evidence','bin','obj','artifacts','AppPackages','__pycache__'}&set(f.relative_to(source).parts):continue
  if f.suffix.lower() in {'.pfx','.key','.sqlite','.dll','.exe'}:continue
  path='hollowwake/'+f.relative_to(root).as_posix()
  if f.suffix.lower() in {'.png','.jpg'}:
   b=api('git/blobs',{'content':base64.b64encode(f.read_bytes()).decode(),'encoding':'base64'});entries.append({'path':path,'mode':'100644','type':'blob','sha':b['sha']})
  else:entries.append({'path':path,'mode':'100644','type':'blob','content':f.read_text()})
entries.append({'path':'.github/workflows/hollowwake-client-experiment-041.yml','mode':'100644','type':'blob','content':(p/'workflow.yml').read_text()})
t=api('git/trees',{'base_tree':base,'tree':entries});c=api('git/commits',{'message':'Integrate real-client image loading stages, resumable private import and UWP execution policy logs','tree':t['sha'],'parents':[head]})
try:api('git/refs',{'ref':'refs/heads/'+branch,'sha':c['sha']})
except:api('git/refs/heads/'+branch,{'sha':c['sha'],'force':False},'PATCH')
# Register guarded manual-only workflow on main, preserving all existing files.
h=api('git/ref/heads/main')['object']['sha'];bt=api('git/commits/'+h)['tree']['sha'];tr=api('git/trees',{'base_tree':bt,'tree':[entries[-1]]});mc=api('git/commits',{'message':'Register guarded client experiment workflow','tree':tr['sha'],'parents':[h]});api('git/refs/heads/main',{'sha':mc['sha'],'force':False},'PATCH')
if '--no-dispatch' not in sys.argv:api('actions/workflows/hollowwake-client-experiment-041.yml/dispatches',{'ref':branch,'inputs':{'build':True}})
r={'commit':c['sha'],'branch':branch,'dispatched':'--no-dispatch' not in sys.argv};(p/'evidence/source.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
