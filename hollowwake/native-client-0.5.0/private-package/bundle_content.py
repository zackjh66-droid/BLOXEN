"""Stage pinned non-code content for a PRIVATE signed package. Never runs game code.

Default is inspection only. --stage requires enough disk and an absent destination.
No trimming/repacking of opaque PAKs and no substitution of archive or manifest.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import stat
import zipfile

BUILD = '6.21-CL-4526925'
PIN = 'bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5'
SIZE = 12528679454

def digest(stream):
    h = hashlib.sha256()
    for block in iter(lambda: stream.read(1024 * 1024), b''):
        h.update(block)
    return h.hexdigest()

def safe(path):
    parts = path.split('/')
    return (path.startswith(('6.21/FortniteGame/Content/', '6.21/Engine/Binaries/ThirdParty/CEF3/Win64/'))
            and not re.search(r'[\\:\x00-\x1f%]', path)
            and all(p and p not in ('.', '..') and not p.endswith((' ', '.'))
                    and not re.match(r'^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)', p, re.I) for p in parts)
            and re.search(r'\.(pak|sig|mp4|srt|png|jpg|bmp|txt|bin|dat)$', path, re.I))

def records(plan):
    if plan.get('schema') != 1 or plan.get('build') != BUILD:
        raise ValueError('Wrong content manifest build/schema')
    seen = set()
    for f in plan['files']:
        key = f['path'].casefold()
        if (not safe(f['path']) or key in seen or type(f['bytes']) is not int
                or not 0 <= f['bytes'] <= 12 * 1024**3
                or not re.fullmatch('[0-9a-f]{64}', f['sha256'])):
            raise ValueError('Unsafe/duplicate content manifest entry')
        seen.add(key)
    if not seen or len(seen) > 10000 or sum(f['bytes'] for f in plan['files']) > 64 * 1024**3:
        raise ValueError('Invalid content count')
    return plan['files']

def inspect(z, files):
    seen = set()
    for entry in z.infolist():
        path = entry.filename.rstrip('/')
        kind = (entry.external_attr >> 16) & 0xf000
        if (path.casefold() in seen or path.startswith('/') or re.search(r'[\\:\x00-\x1f%]', path)
                or any(p in ('', '.', '..') or p.endswith((' ', '.')) for p in path.split('/'))
                or kind not in (0, stat.S_IFREG, stat.S_IFDIR)):
            raise ValueError('Unsafe archive inventory')
        seen.add(path.casefold())
    if sum(e.file_size for e in z.infolist()) > 64 * 1024**3:
        raise ValueError('Excessive archive expansion')
    for f in files:
        if z.getinfo(f['path']).file_size != f['bytes']:
            raise ValueError('Content member size differs')
    return {'files': len(files), 'expandedBytes': sum(f['bytes'] for f in files),
            'sourceZipMemberCompressedBytes': sum(z.getinfo(f['path']).compress_size for f in files),
            'sizeIsNotAnMsixPrediction': True, 'trimmedFiles': [], 'gameExecuted': False}

def stage(z, files, destination, manifest_bytes):
    target = Path(destination)
    if target.exists():
        raise ValueError('Destination exists; preserved unchanged')
    if shutil.disk_usage(target.parent).free < sum(f['bytes'] for f in files) + 1024**3:
        raise ValueError('Insufficient disk: expanded content plus 1 GiB staging headroom required; signing needs additional space')
    target.mkdir(mode=0o700)
    # On failure keep the partial directory, WITHOUT a ready marker, for inspection.
    for f in files:
        path = target / f['path']
        path.parent.mkdir(parents=True, exist_ok=True)
        with z.open(f['path']) as source, path.open('xb') as output:
            h = hashlib.sha256()
            count = 0
            for block in iter(lambda: source.read(1024 * 1024), b''):
                count += len(block)
                if count > f['bytes']:
                    raise ValueError('Expanded member exceeds pin')
                h.update(block)
                output.write(block)
        if count != f['bytes'] or h.hexdigest() != f['sha256']:
            raise ValueError('Content SHA-256 mismatch: ' + f['path'])
    (target / 'bundled-content.json').write_text(json.dumps({
        'schema': 1, 'build': BUILD, 'mode': 'installed-package-only',
        'contentPlanSha256': hashlib.sha256(manifest_bytes).hexdigest(),
        'files': len(files), 'expandedBytes': sum(f['bytes'] for f in files)
    }, indent=2) + '\n')

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('archive', type=Path, nargs='?')
    p.add_argument('--manifest', type=Path, default=Path(__file__).parents[1] / 'App/ContentPlan621.json')
    p.add_argument('--stage', type=Path)
    p.add_argument('--prepared', type=Path)
    p.add_argument('--copy-to', type=Path)
    a = p.parse_args()
    raw = a.manifest.read_bytes()
    plan = json.loads(raw)
    files = records(plan)
    if plan['archive']['bytes'] != SIZE or plan['archive']['sha256'] != PIN:
        raise ValueError('Manifest archive pin differs')
    if a.prepared:
        if a.archive or a.stage or not a.copy_to:
            raise ValueError('Prepared mode requires --copy-to only')
        expected = {'schema': 1, 'build': BUILD, 'mode': 'installed-package-only',
                    'contentPlanSha256': hashlib.sha256(raw).hexdigest(),
                    'files': len(files), 'expandedBytes': sum(f['bytes'] for f in files)}
        if json.loads((a.prepared / 'bundled-content.json').read_text()) != expected:
            raise ValueError('Bundled marker differs from pinned inventory')
        # Reject links and unexpected files; never copy arbitrary preparation additions.
        allowed = {f['path'] for f in files} | {'bundled-content.json'}
        for root, dirs, names in os.walk(a.prepared, followlinks=False):
            for name in dirs + names:
                item = Path(root) / name
                if item.is_symlink() or getattr(item.lstat(), 'st_file_attributes', 0) & 0x400:
                    raise ValueError('Linked/reparse content rejected')
            for name in names:
                if (Path(root) / name).relative_to(a.prepared).as_posix() not in allowed:
                    raise ValueError('Unexpected prepared file')
        for f in files:
            item = a.prepared / f['path']
            if item.stat().st_size != f['bytes']:
                raise ValueError('Prepared content size differs')
            with item.open('rb') as stream:
                if digest(stream) != f['sha256']:
                    raise ValueError('Prepared content hash differs: ' + f['path'])
        if shutil.disk_usage(a.copy_to).free < expected['expandedBytes'] + 1024**3:
            raise ValueError('Insufficient disk for package staging')
        for f in files:
            dest = a.copy_to / f['path']
            if dest.exists():
                raise ValueError('Refuse to overwrite existing package data: ' + f['path'])
            dest.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(a.prepared / f['path'], dest)
        (a.copy_to / 'bundled-content.json').write_text(json.dumps(expected, indent=2) + '\n')
        print(json.dumps({'staged': True, 'verifiedFiles': len(files), 'gameExecuted': False}))
        return
    if not a.archive or a.copy_to:
        raise ValueError('Archive mode requires an archive')
    if a.archive.stat().st_size != SIZE or plan['archive']['bytes'] != SIZE or plan['archive']['sha256'] != PIN:
        raise ValueError('Archive pin differs')
    with a.archive.open('rb') as source:
        if digest(source) != PIN:
            raise ValueError('Archive SHA-256 differs')
    with zipfile.ZipFile(a.archive) as z:
        result = inspect(z, files)
        if a.stage:
            stage(z, files, a.stage, raw)
        result['staged'] = bool(a.stage)
        print(json.dumps(result, indent=2))

if __name__ == '__main__':
    main()
