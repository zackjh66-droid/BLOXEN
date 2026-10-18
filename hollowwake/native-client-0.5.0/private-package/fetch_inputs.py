"""Source fallback for private hosted builds; never execute archive content.
Mirrors are the exact 6.21 entries in llamaqwerty/fortnite-builds-archive.
Every selected file must still match the previously inspected SHA-256 inventory.
"""
from pathlib import Path
import argparse,json,urllib.request,shutil,subprocess,hashlib,os,re,time,zipfile,zlib
from fetch_boot import fetch,NoRedirect
from fetch_code_prefix import fetch_code_prefix
MIRRORS=['https://public.simplyblk.xyz/6.21.rar','https://builds.rebootfn.org/6.21.rar']
def file_hash(path):
 result=hashlib.sha256()
 with path.open('rb') as stream:
  for chunk in iter(lambda:stream.read(4*1024**2),b''):result.update(chunk)
 return result.hexdigest()
def fallback(manifest,destination):
 out=Path(destination);m=json.loads(Path(manifest).read_text());tool=Path(os.environ.get('ProgramFiles','C:/Program Files'))/'7-Zip/7z.exe'
 if not tool.is_file():raise RuntimeError('Maintained 7-Zip missing from hosted runner')
 version=subprocess.run([str(tool),'i'],capture_output=True,text=True,check=True).stdout
 match=re.search(r'7-Zip\s+(\d+)\.(\d+)',version)
 if not match or (int(match[1]),int(match[2]))<(25,1):raise RuntimeError('Hosted 7-Zip is older than maintained minimum25.01')
 opener=urllib.request.build_opener(NoRedirect());failures=[]
 for index,url in enumerate(MIRRORS):
  archive=out.parent/('Mirror-'+str(index)+'.private.rar')
  try:
   if shutil.disk_usage(out.parent).free<18*1024**3:raise RuntimeError('Mirror download needs18GiB free before transfer')
   curl=Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/curl.exe'
   if not curl.is_file():raise RuntimeError('OS Schannel curl not available; TLS verification remains required')
   # Schannel builds a valid chain using Windows/AIA. Do not disable verification
   # to work around Python's missing intermediate CA error observed in CI.
   process=subprocess.Popen([str(curl),'--fail','--silent','--show-error','--proto','=https','--max-time','480','--output',str(archive),url],stdout=subprocess.PIPE,stderr=subprocess.PIPE)
   while process.poll() is None:
    if archive.exists() and archive.stat().st_size>16*1024**3:process.kill();process.communicate();raise ValueError('Mirror archive exceeds16GiB cap')
    time.sleep(.2)
   stdout,stderr=process.communicate()
   # A verified TLS stream may finish its payload without a close_notify.
   # Never accept other curl errors; archive checks and every selected SHA still apply.
   detail=stderr.decode(errors='replace')
   abruptClose=process.returncode==56 and 'missing close_notify' in detail and archive.is_file() and archive.stat().st_size>0
   if process.returncode and not abruptClose:raise RuntimeError('Verified OS HTTPS download failed: '+detail[:160])
   print(json.dumps({'source':url,'downloadBytes':archive.stat().st_size,'curlExit':process.returncode,'closeNotifyMissing':abruptClose,'tlsVerificationDisabled':False}))
   listing=subprocess.run([str(tool),'l','-slt','-sccUTF-8',str(archive)],capture_output=True,text=True,encoding='utf-8',check=True).stdout
   records=[]
   for block in listing.split('\n\n'):
    row={}
    for line in block.splitlines():
     if ' = ' in line:key,value=line.split(' = ',1);row[key.strip()]=value.strip()
    if 'Size' in row and 'Path' in row:records.append(row)
   if not records or len(records)>100000:raise ValueError('Invalid inventory count')
   expanded=0;names={}
   for row in records:
    name=row['Path'].replace('\\','/');parts=name.split('/')
    if name.startswith('/') or ':' in name or '..' in parts or row.get('Symbolic Link') or row.get('Hard Link') or 'L' in row.get('Attributes',''):raise ValueError('Unsafe archive entry')
    expanded+=int(row.get('Size','0'));key=name.casefold()
    if key in names:raise ValueError('Duplicate archive path')
    names[key]=(name,row)
   if expanded>64*1024**3:raise ValueError('Expanded inventory exceeds64GiB')
   selected=[]
   for item in m['files']:
    expected=item['path'];tail=expected.removeprefix('6.21/').casefold();matches=[value for key,value in names.items() if key==tail or key.endswith('/'+tail)]
    if len(matches)!=1 or int(matches[0][1]['Size'])!=item['bytes']:raise ValueError('Pinned file absent/ambiguous/different size: '+Path(expected).name)
    selected.append((item,matches[0][0]))
   extract=out.parent/('Mirror-'+str(index)+'-selected');selection=out.parent/('Mirror-'+str(index)+'-include.txt');selection.write_text('\n'.join(name for _,name in selected),encoding='utf-8')
   subprocess.run([str(tool),'x','-y','-scsUTF-8','-o'+str(extract),'-i@'+str(selection),str(archive)],check=True,capture_output=True)
   for item,name in selected:
    source=extract/name
    if source.is_symlink() or not source.is_file() or source.stat().st_size!=item['bytes']:raise ValueError('Selected expansion mismatch')
    if hashlib.sha256(source.read_bytes()).hexdigest()!=item['sha256']:raise ValueError('Selected SHA-256 mismatch: '+source.name)
   out.mkdir(exist_ok=True)
   for item,name in selected:
    target=out/item['path'];target.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(extract/name,target)
   return {'source':url,'files':len(selected),'allSelectedHashesMatched':True,'archiveSha256':file_hash(archive),'executed':False,'authenticEpicDistributionClaimed':False}
  except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as e:failures.append({'source':url,'error':type(e).__name__,'message':str(e)[:200]})
 raise RuntimeError('No verified private source available: '+json.dumps(failures))
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('manifest');p.add_argument('output');a=p.parse_args()
 try:result=fetch_code_prefix(a.manifest,a.output)
 except (OSError,ValueError,RuntimeError,zipfile.BadZipFile,zlib.error) as e:
  print(json.dumps({'primarySourceFailure':type(e).__name__,'message':str(e)[:180]}));result=fallback(a.manifest,a.output)
 print(json.dumps(result))
