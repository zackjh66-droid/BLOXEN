"""Private data-only inventory from the immutable inspected ZIP. Never executes code.
Output is server configuration, not permission to redistribute the underlying files.
"""
from pathlib import Path
import argparse,hashlib,json,re,zipfile,stat
PIN='bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5'
def digest(stream):
 h=hashlib.sha256()
 for b in iter(lambda:stream.read(1024**2),b''):h.update(b)
 return h.hexdigest()
def generate(archive,output):
 source=Path(archive);target=Path(output)
 if target.exists():raise ValueError('Refuse to replace a preserved manifest')
 with source.open('rb') as f:
  if source.stat().st_size!=12528679454 or digest(f)!=PIN:raise ValueError('Original archive differs from inspected pin')
 files=[];seen=set()
 with zipfile.ZipFile(source) as z:
  for i in z.infolist():
   n=i.filename;parts=n.split('/');kind=(i.external_attr>>16)&0xf000
   if n.casefold() in seen or n.startswith('/') or re.search(r'[\\:\x00-\x1f%]',n) or any(x in {'.','..'} or x.endswith((' ','.')) for x in parts) or kind not in {0,stat.S_IFREG,stat.S_IFDIR}:raise ValueError('Unsafe archive inventory')
   seen.add(n.casefold())
   if i.is_dir():continue
   if not n.startswith(('6.21/FortniteGame/Content/','6.21/Engine/Binaries/ThirdParty/CEF3/Win64/')):continue
   if not re.search(r'\.(pak|sig|mp4|srt|png|jpg|bmp|txt|bin|dat)$',n,re.I):continue
   if i.file_size>12*1024**3:raise ValueError('Content member exceeds limit')
   with z.open(i) as f:hash_value=digest(f)
   files.append({'path':n,'bytes':i.file_size,'sha256':hash_value})
 value={'schema':1,'build':'6.21-CL-4526925','archive':{'url':'https://public-build-archives-bucket.erafn.dev/6.21.zip','bytes':12528679454,'sha256':PIN},'files':sorted(files,key=lambda x:x['path'])}
 if not files or sum(f['bytes'] for f in files)>64*1024**3:raise ValueError('Invalid data inventory')
 target.parent.mkdir(parents=True,exist_ok=True);target.write_text(json.dumps(value,indent=2)+'\n');target.chmod(0o600)
 return {'files':len(files),'expandedBytes':sum(f['bytes'] for f in files),'nativeCodeIncluded':False,'gameExecuted':False,'published':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('archive');p.add_argument('output');a=p.parse_args();print(json.dumps(generate(a.archive,a.output)))
