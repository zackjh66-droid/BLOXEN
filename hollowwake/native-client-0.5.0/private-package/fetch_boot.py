"""Fetch only hash-pinned ZIP ranges; no archive code is executed."""
from pathlib import Path
import argparse,json,urllib.request,hashlib,zlib,shutil,zipfile,stat
class RangeUnavailable(Exception):pass
class NoRedirect(urllib.request.HTTPRedirectHandler):
 def redirect_request(self,*args,**kwargs):raise ValueError('Redirect rejected; inspect new archive source first')
def ranged_fetch(manifest,destination):
 m=json.loads(Path(manifest).read_text());out=Path(destination)
 if out.exists():raise ValueError('Use a new private download directory')
 if m['source']!='https://public-build-archives-bucket.erafn.dev/6.21.zip' or len(m['files'])>200:raise ValueError('Unexpected source/manifest size')
 if shutil.disk_usage(out.parent).free<2*1024**3:raise ValueError('Private boot preparation requires 2GiB free')
 out.mkdir();opener=urllib.request.build_opener(NoRedirect())
 for item in m['files']:
  parts=Path(item['path']).parts
  if not parts or parts[0]!='6.21' or any(x in {'.','..'} for x in parts) or '\\' in item['path'] or ':' in item['path']:raise ValueError('Unsafe manifest path')
  if not 0<=item['bytes']<=512*1024**2 or item['method'] not in [0,8] or not 0<=item['offset']<=m['archiveBytes']-item['compressed']:raise ValueError('Invalid archive bounds')
  dest=out/item['path'];dest.parent.mkdir(parents=True,exist_ok=True);remaining=item['compressed'];start=item['offset'];digest=hashlib.sha256();inflater=zlib.decompressobj(-15) if item['method']==8 else None;total=0
  with dest.open('xb') as output:
   while remaining:
    length=min(4*1024**2,remaining);end=start+length-1;request=urllib.request.Request(m['source']+f'?hollowwake_range={start}-{end}',headers={'Cache-Control':'no-cache','Range':f'bytes={start}-{end}','User-Agent':'Hollowwake-PrivateStaticBuild/0.5'})
    with opener.open(request,timeout=90) as response:
     if response.status!=206 or response.headers.get('Content-Range')!=f'bytes {start}-{end}/{m["archiveBytes"]}':raise RangeUnavailable('Archive source did not provide the exact pinned range')
     compressed=response.read(length+1)
    if len(compressed)!=length:raise ValueError('Range length mismatch')
    expanded=inflater.decompress(compressed,item['bytes']-total+1) if inflater else compressed
    total+=len(expanded)
    if total>item['bytes']:raise ValueError('Expanded file exceeds pinned length')
    output.write(expanded);digest.update(expanded);remaining-=length;start+=length
   if inflater:
    tail=inflater.flush();total+=len(tail);output.write(tail);digest.update(tail)
    if not inflater.eof or inflater.unused_data:raise ValueError('Invalid deflate extent')
  if total!=item['bytes'] or digest.hexdigest()!=item['sha256']:raise ValueError('Pinned file SHA256 mismatch: '+dest.name)
 return {'files':len(m['files']),'executed':False}
def fetch(manifest,destination):
 try:return ranged_fetch(manifest,destination)
 except RangeUnavailable:
  m=json.loads(Path(manifest).read_text());out=Path(destination);archive=out.parent/'Original-6.21.private.zip'
  if shutil.disk_usage(out.parent).free<m['archiveBytes']+2*1024**3:raise ValueError('Range refused and insufficient free disk for verified full archive')
  if archive.exists():raise ValueError('Refusing overwrite of preserved private archive')
  digest=hashlib.sha256();total=0;opener=urllib.request.build_opener(NoRedirect())
  with opener.open(urllib.request.Request(m['source'],headers={'User-Agent':'Hollowwake-PrivateStaticBuild/0.5'}),timeout=90) as response,archive.open('xb') as output:
   if response.status!=200:raise ValueError('Full archive download blocked')
   while True:
    chunk=response.read(4*1024**2)
    if not chunk:break
    total+=len(chunk)
    if total>m['archiveBytes']:raise ValueError('Full archive exceeds pinned size')
    output.write(chunk);digest.update(chunk)
  if total!=m['archiveBytes'] or digest.hexdigest()!='bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5':raise ValueError('Full archive size/hash mismatch')
  with zipfile.ZipFile(archive) as package:
   infos=package.infolist()
   if len(infos)>100000 or sum(i.file_size for i in infos)>64*1024**3:raise ValueError('Archive inventory exceeds expected bounds')
   names={}
   for i in infos:
    name=i.filename
    if name in names or name.startswith(('/', '\\')) or '\\' in name or ':' in name or '..' in Path(name).parts or stat.S_ISLNK(i.external_attr>>16):raise ValueError('Unsafe or duplicate archive path')
    names[name]=i
   for item in m['files']:
    info=names.get(item['path'])
    if not info or info.file_size!=item['bytes'] or info.compress_size!=item['compressed'] or info.compress_type!=item['method']:raise ValueError('Selected ZIP metadata differs from pinned inventory')
    target=out/item['path'];target.parent.mkdir(parents=True,exist_ok=True);digest=hashlib.sha256();total=0
    with package.open(info) as source,target.open('wb') as output:
     while True:
      chunk=source.read(1024**2)
      if not chunk:break
      total+=len(chunk)
      if total>item['bytes']:raise ValueError('Selected expansion exceeds bounds')
      output.write(chunk);digest.update(chunk)
    if total!=item['bytes'] or digest.hexdigest()!=item['sha256']:raise ValueError('Selected source hash mismatch')
  return {'files':len(m['files']),'fullArchiveHashVerified':True,'executed':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('manifest');p.add_argument('output');a=p.parse_args();print(json.dumps(fetch(a.manifest,a.output)))
