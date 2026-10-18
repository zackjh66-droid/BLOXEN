"""Read only pinned code extents from a ZIP stream; never execute inputs.
A prefix is not a complete archive. Every output still needs its recorded SHA256.
This avoids requiring the central directory or a multi-GB game-data transfer.
"""
from pathlib import Path
import json,hashlib,urllib.request,zlib,shutil
from fetch_boot import NoRedirect
SOURCE='https://public-build-archives-bucket.erafn.dev/6.21.zip'
def fetch_code_prefix(manifest,destination,opener=None):
 m=json.loads(Path(manifest).read_text());out=Path(destination)
 if m['source']!=SOURCE or m['archiveBytes']!=12528679454:raise ValueError('Unexpected pinned source')
 files=[x for x in m['files'] if x['path'].lower().endswith(('.exe','.dll'))]
 if not files or len(files)>200:raise ValueError('Invalid code inventory')
 names=set();bounds=[]
 for item in files:
  name=item['path'];parts=Path(name).parts
  if not parts or parts[0]!='6.21' or name.startswith('/') or '\\' in name or ':' in name or any(x in {'.','..'} for x in parts) or name.casefold() in names:raise ValueError('Unsafe code path')
  names.add(name.casefold());start=item['offset'];count=item['compressed'];size=item['bytes']
  if not 0<=start or not 0<count<=512*1024**2 or not 0<size<=512*1024**2 or start+count>m['archiveBytes'] or item['method'] not in (0,8):raise ValueError('Invalid pinned extent')
  if len(item['sha256'])!=64 or any(c not in '0123456789abcdef' for c in item['sha256']):raise ValueError('Invalid SHA256')
  bounds.append((start,start+count))
 bounds.sort()
 if any(a[1]>b[0] for a,b in zip(bounds,bounds[1:])):raise ValueError('Overlapping code extents')
 required=max(b for a,b in bounds)
 if required>512*1024**2:raise ValueError('Code prefix exceeds bounded acquisition policy')
 if out.exists():raise ValueError('Use new isolated code input directory')
 out.parent.mkdir(parents=True,exist_ok=True)
 if shutil.disk_usage(out.parent).free<2*1024**3:raise ValueError('Code acquisition requires2GiB free')
 prefix=out.parent/(out.name+'.unverified-prefix')
 if prefix.exists():raise ValueError('Refusing overwrite of prior source evidence')
 opener=opener or urllib.request.build_opener(NoRedirect())
 request=urllib.request.Request(SOURCE,headers={'Range':f'bytes=0-{required-1}','User-Agent':'Hollowwake-PrivateStaticBuild/0.5'})
 with opener.open(request,timeout=90) as response,prefix.open('xb') as output:
  if response.status not in (200,206):raise ValueError('Source refused prefix request')
  if response.status==206 and response.headers.get('Content-Range')!=f'bytes 0-{required-1}/{m["archiveBytes"]}':raise ValueError('Incorrect source range')
  left=required
  while left:
   chunk=response.read(min(left,1024**2))
   if not chunk:raise ValueError('Truncated code prefix')
   output.write(chunk);left-=len(chunk)
 with prefix.open('rb') as source:
  if source.read(4)!=b'PK\x03\x04':raise ValueError('Source is not a ZIP local-header stream')
  # Do not publish or stage any file until every selected extent is verified.
  verified=[]
  for item in files:
   source.seek(item['offset']);compressed=source.read(item['compressed'])
   if len(compressed)!=item['compressed']:raise ValueError('Truncated pinned extent')
   if item['method']==8:
    inflater=zlib.decompressobj(-15);data=inflater.decompress(compressed,item['bytes']+1)
    if not inflater.eof or inflater.unused_data or inflater.unconsumed_tail:raise ValueError('Invalid bounded deflate stream')
   else:data=compressed
   if len(data)!=item['bytes'] or hashlib.sha256(data).hexdigest()!=item['sha256']:raise ValueError('Pinned code SHA256 mismatch: '+Path(item['path']).name)
   verified.append(item)
  out.mkdir()
  for item in verified:
   source.seek(item['offset']);compressed=source.read(item['compressed']);data=zlib.decompress(compressed,-15) if item['method']==8 else compressed
   target=out/item['path'];target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
 return {'source':SOURCE,'codeFiles':len(files),'prefixBytes':required,'allSelectedHashesMatched':True,'completeArchiveDownloaded':False,'archiveSha256Claimed':False,'executed':False,'omittedNonCodeFiles':[x['path'] for x in m['files'] if x not in files]}
