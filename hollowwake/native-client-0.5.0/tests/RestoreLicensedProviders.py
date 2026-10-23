"""Restore exact previously tested licensed providers, never game code.

A per-run encrypted lease supplies a GET-only URL. Hash the complete archive
before extraction; retain corresponding sources and licenses. No URL is logged.
"""
import argparse, hashlib, json, os, pathlib, shutil, stat, tempfile, time, urllib.request, zipfile
SHA = '233ed20ffcf8d87230c126453e6a99d36060dddb5f32ac994ec9ba6619f200dd'
BYTES = 39172799
HOST = 'ricswvi50gagbcuh.private.blob.vercel-storage.com'
PATH = '/licensed-providers/' + SHA + '/providers.zip'

class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        raise ValueError('Provider archive redirect rejected')

def restore(lease_path, destination):
    lease = json.loads(pathlib.Path(lease_path).read_text(encoding='utf-8-sig'))
    if lease['runId'] != os.environ['GITHUB_RUN_ID'] or lease['expiresAt'] < time.time()*1000 + 60000:
        raise ValueError('Provider lease identity/expiry differs')
    record = lease['licensedProviders']
    from urllib.parse import urlsplit
    url = urlsplit(record['url'])
    if (record['sha256'],record['bytes']) != (SHA,BYTES) or url.scheme != 'https' or url.netloc != HOST or url.path != PATH or url.username or url.password or url.fragment:
        raise ValueError('Provider archive origin/identity differs')
    destination = pathlib.Path(destination)
    if any((destination / group).exists() for group in ['audio','mesa']):
        raise ValueError('Provider output already exists')
    with tempfile.SpooledTemporaryFile(max_size=64*1024*1024,mode='w+b') as archive, tempfile.TemporaryDirectory(prefix='hw-provider-verified-') as work:
        digest=hashlib.sha256();total=0
        opener=urllib.request.build_opener(NoRedirect())
        with opener.open(record['url'],timeout=90) as response:
            if response.status != 200:
                raise ValueError('Provider archive HTTP status differs')
            while True:
                chunk=response.read(1024*1024)
                if not chunk: break
                total+=len(chunk)
                if total>BYTES: raise ValueError('Provider archive exceeded pinned size')
                digest.update(chunk);archive.write(chunk)
        if total!=BYTES or digest.hexdigest()!=SHA: raise ValueError('Provider archive SHA-256 differs')
        archive.seek(0)
        stage=pathlib.Path(work)/'files'
        with zipfile.ZipFile(archive) as package:
            entries=package.infolist();names=set();expanded=0
            if len(entries)!=36: raise ValueError('Provider archive file count differs')
            for entry in entries:
                path=pathlib.PurePosixPath(entry.filename);expanded+=entry.file_size
                if path.is_absolute() or '..' in path.parts or '\\' in entry.filename or len(path.parts)!=2 or path.parts[0] not in ('audio','mesa') or ':' in entry.filename or stat.S_ISLNK(entry.external_attr>>16) or entry.filename.casefold() in names or expanded>256*1024*1024:
                    raise ValueError('Invalid provider archive member')
                names.add(entry.filename.casefold())
            package.extractall(stage)
        for group in ['audio','mesa']:
            root=stage/group;inventory=json.loads((root/'provider-imports.json').read_text(encoding='utf-8-sig'))
            for item in inventory['files']:
                name=item['file']
                if pathlib.PurePosixPath(name).name!=name or '\\' in name or ':' in name: raise ValueError('Invalid provider inventory path')
                raw=(root/name).read_bytes()
                if len(raw)!=item['bytes'] or hashlib.sha256(raw).hexdigest()!=item['sha256']: raise ValueError('Provider inventory hash differs')
        destination.mkdir(parents=True,exist_ok=True)
        for group in ['audio','mesa']: shutil.move(str(stage/group),str(destination/group))
    print('PASS exact licensed provider archive restored; whole ZIP and binary hashes verified; source and licenses retained')

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('lease');parser.add_argument('destination');args=parser.parse_args()
    try: restore(args.lease,args.destination)
    except Exception:
        raise SystemExit('Licensed provider restoration failed; private URL and credential details omitted')
