from pathlib import Path
import subprocess,json,base64,hashlib,time
out=Path('/workspace/hollowwake-experiment-release-0.4.5');repo='zackjh66-droid/BLOXEN';branch='hollowwake/client-experiment-0.4.5'
def api(path,data=None,method=None):
 cmd=['gh','api','repos/'+repo+'/'+path]
 if data is not None:cmd+=['--method',method or 'POST','--input','-']
 for attempt in range(4):
  p=subprocess.run(cmd,input=json.dumps(data) if data is not None else None,text=True,capture_output=True)
  if not p.returncode:break
  if attempt==3 or not any(code in p.stderr for code in ['HTTP 503','HTTP 502','HTTP 504']):raise RuntimeError(p.stderr[:450])
  time.sleep(2**attempt)
 return json.loads(p.stdout) if p.stdout.strip() else {}
head=api('git/ref/heads/'+branch)['object']['sha'];base=api('git/commits/'+head)['tree']['sha'];entries=[];links={}
for f in sorted(out.iterdir()):
 if not f.is_file() or f.name=='download-links.json':continue
 assert f.name in {'Hollowwake-Client-Experiment-0.4.5-x64.zip','Hollowwake-Dependencies-x64.zip','Hollowwake-Verification-0.4.5.zip','Hollowwake-Source-0.4.5.zip','Hollowwake_0.4.5.0_x64.appx','HollowwakeExperiment.cer','ARTIFACTS.json','RELEASE-NOTES.md'}
 data=f.read_bytes();blob=api('git/blobs',{'content':base64.b64encode(data).decode(),'encoding':'base64'});assert blob['sha']==hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
 entries.append({'path':'downloads/'+f.name,'mode':'100644','type':'blob','sha':blob['sha']});links[f.name]={'url':'https://raw.githubusercontent.com/'+repo+'/'+branch+'/downloads/'+f.name,'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()};print('Verified own-artifact blob:',f.name,flush=True)
t=api('git/trees',{'base_tree':base,'tree':entries});c=api('git/commits',{'message':'Deliver signed experimental client initialization runtime, x64 dependencies, public certificate, complete source and scoped verification; no game files','tree':t['sha'],'parents':[head]});api('git/refs/heads/'+branch,{'sha':c['sha'],'force':False},'PATCH')
(out/'download-links.json').write_text(json.dumps({'repository':repo,'branch':branch,'commit':c['sha'],'downloads':links},indent=2));print(json.dumps({'commit':c['sha'],'artifactCount':len(links)}))
