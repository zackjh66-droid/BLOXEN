"""Private hosted input preparation. No game code or archive launcher is executed.

Keep the preserved original archive in the analysis workspace. This command uses
only its own new disposable download and may remove THAT copy after verification.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import time
import urllib.request
import zipfile
from bundle_content import PIN, SIZE, records, inspect, stage
from fetch_boot import NoRedirect

SOURCE = 'https://public-build-archives-bucket.erafn.dev/6.21.zip'

def acquire(work, content_manifest, code_manifest):
    work = Path(work)
    if work.exists():
        raise ValueError('Use a new private work directory')
    work.parent.mkdir(parents=True, exist_ok=True)
    free = shutil.disk_usage(work.parent).free
    if free < 44 * 1024**3:
        raise ValueError(f'Full private staging needs at least 44 GiB free; available bytes={free}')
    raw = Path(content_manifest).read_bytes()
    content = json.loads(raw)
    files = records(content)
    if content['archive']['sha256'] != PIN or content['archive']['bytes'] != SIZE or content['archive']['url'] != SOURCE:
        raise ValueError('Content archive pin differs')
    code = json.loads(Path(code_manifest).read_text())
    if code['source'] != SOURCE or code['archiveBytes'] != SIZE:
        raise ValueError('Code source differs')
    code_files = [f for f in code['files'] if f['path'].lower().endswith(('.exe', '.dll'))]
    if not code_files or len(code_files) > 200:
        raise ValueError('Invalid pinned code inventory')
    work.mkdir(mode=0o700)
    archive = work / 'DisposableVerifiedSource.zip'
    h = hashlib.sha256()
    count = 0
    last = time.monotonic()
    request = urllib.request.Request(SOURCE, headers={'User-Agent': 'Hollowwake-PrivateStaticBuild/0.5'})
    with urllib.request.build_opener(NoRedirect()).open(request, timeout=90) as response, archive.open('xb') as output:
        if response.status != 200 or response.headers.get('Content-Length') not in (None, str(SIZE)):
            raise ValueError('Unexpected archive status/length')
        while True:
            block = response.read(4 * 1024**2)
            if not block:
                break
            count += len(block)
            if count > SIZE:
                raise ValueError('Archive exceeds pinned size')
            h.update(block)
            output.write(block)
            if time.monotonic() - last >= 20:
                print(json.dumps({'downloadedBytes': count, 'expectedBytes': SIZE}), flush=True)
                last = time.monotonic()
    if count != SIZE or h.hexdigest() != PIN:
        raise ValueError(f'Complete archive mismatch: bytes={count}, sha256={h.hexdigest()}')
    with zipfile.ZipFile(archive) as z:
        result = inspect(z, files)
        # The complete ZIP inventory was checked before any member extraction.
        names = set()
        for f in code_files:
            name = f['path']
            if not name.startswith('6.21/') or name.casefold() in names or z.getinfo(name).file_size != f['bytes']:
                raise ValueError('Invalid code inventory path/size')
            names.add(name.casefold())
            destination = work / 'OriginalCode' / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            h = hashlib.sha256()
            expanded = 0
            with z.open(name) as source, destination.open('xb') as output:
                while True:
                    block = source.read(1024**2)
                    if not block:
                        break
                    expanded += len(block)
                    if expanded > f['bytes']:
                        raise ValueError('Code expansion exceeds pin')
                    h.update(block)
                    output.write(block)
            if expanded != f['bytes'] or h.hexdigest() != f['sha256']:
                raise ValueError('Pinned code hash mismatch: ' + name)
        stage(z, files, work / 'FullContent', raw)
    result.update(archiveSha256=PIN, archiveBytes=SIZE, codeFiles=len(code_files),
                  fullContentStaged=True, gameExecuted=False,
                  preservedAnalysisArchiveModified=False,
                  disposableRunnerArchiveRemoved=True)
    (work / 'full-input-audit.json').write_text(json.dumps(result, indent=2) + '\n')
    # This is only the new disposable runner copy, not the preserved input.
    archive.unlink()
    print(json.dumps(result), flush=True)

if __name__ == '__main__':
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('work', type=Path)
    p.add_argument('content_manifest', type=Path)
    p.add_argument('code_manifest', type=Path)
    a = p.parse_args()
    acquire(a.work, a.content_manifest, a.code_manifest)
