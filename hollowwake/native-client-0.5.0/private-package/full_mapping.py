"""MakeAppx mapping and streamed final-package audit; never loads game images."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import zipfile
import xml.etree.ElementTree as ET
from bundle_content import records, digest

def plan(content, manifest):
    raw = Path(manifest).read_bytes()
    p = json.loads(raw)
    files = records(p)
    marker = json.loads((Path(content) / 'bundled-content.json').read_text())
    if (marker.get('mode') != 'installed-package-only' or marker.get('build') != p['build']
            or marker.get('contentPlanSha256') != hashlib.sha256(raw).hexdigest()
            or marker.get('files') != len(files) or marker.get('expandedBytes') != sum(f['bytes'] for f in files)):
        raise ValueError('Bundled marker does not match signed metadata')
    return files, marker

def mapping(layout, content, manifest, output):
    layout, content = Path(layout), Path(content)
    files, marker = plan(content, manifest)
    manifest_path = layout / 'AppxManifest.xml'
    raw_manifest = manifest_path.read_text(encoding='utf-8-sig')
    tree = ET.parse(manifest_path)
    identity = next(e for e in tree.iter() if e.tag.endswith('}Identity'))
    if identity.get('Name') != 'Hollowwake.NativeClientExperiment' or identity.get('ProcessorArchitecture') != 'x64':
        raise ValueError('Unexpected native experiment identity')
    # Preserve all original namespace declarations, including IgnorableNamespaces.
    raw_manifest, replacements = re.subn(r'(<(?:\w+:)?Identity\b[^>]*\bVersion=")[^"]+("[^>]*>)', r'\g<1>0.5.2.0\2', raw_manifest, count=1)
    if replacements != 1:
        raise ValueError('Compiled manifest version was not uniquely located')
    manifest_path.write_text(raw_manifest, encoding='utf-8')
    native = json.loads((layout / 'native-package-manifest.json').read_text())
    if native.get('build') != '6.21-CL-4526925' or not native.get('files'):
        raise ValueError('Real client package metadata absent')
    native.update(contentPaksIncluded=True, contentManifestSha256=marker['contentPlanSha256'], compatibilityEstablished=False)
    (layout / 'native-package-manifest.json').write_text(json.dumps(native, indent=2) + '\n')
    entries = []
    for source in sorted(layout.rglob('*')):
        if source.is_symlink():
            raise ValueError('Linked layout rejected')
        if source.is_file() and source.name not in ('AppxSignature.p7x', 'AppxBlockMap.xml', '[Content_Types].xml'):
            entries.append((source, source.relative_to(layout).as_posix()))
    entries += [(content / f['path'], f['path']) for f in files]
    entries.append((content / 'bundled-content.json', 'bundled-content.json'))
    seen = set()
    for source, name in entries:
        if name.casefold() in seen or not source.is_file() or any(c in str(source) + name for c in '\r\n"'):
            raise ValueError('Mapping source/name collision or invalid path')
        seen.add(name.casefold())
    Path(output).write_text('[Files]\n' + ''.join(f'"{source.resolve()}" "{name.replace(chr(47), chr(92))}"\n' for source, name in entries), encoding='utf-8')
    print(json.dumps({'mappedFiles': len(entries), 'contentFiles': len(files), 'version': '0.5.2.0', 'gameExecuted': False}))

def audit(package, content, manifest, prepared_plan, output):
    files, marker = plan(content, manifest)
    native = json.loads(Path(prepared_plan).read_text())
    with zipfile.ZipFile(package) as z:
        seen = set()
        for item in z.infolist():
            n = item.filename
            if n.casefold() in seen or n.startswith('/') or '\\' in n or ':' in n or '..' in n.split('/'):
                raise ValueError('Invalid final package inventory')
            seen.add(n.casefold())
        if 'AppxSignature.p7x' not in z.namelist() or 'AppxBlockMap.xml' not in z.namelist():
            raise ValueError('Final package has no signature/blockmap')
        for f in files + [{'path': f['packageRelativePath'], 'bytes': f['bytes'], 'sha256': f['sha256']} for f in native['files']]:
            entry = z.getinfo(f['path'])
            if entry.file_size != f['bytes']:
                raise ValueError('Final package size differs: ' + f['path'])
            with z.open(entry) as stream:
                if digest(stream) != f['sha256']:
                    raise ValueError('Final package SHA256 differs: ' + f['path'])
        for proxy in native['proxyDlls']:
            if proxy['proxyDll'] not in z.namelist():
                raise ValueError('Native compatibility DLL absent')
        if json.loads(z.read('bundled-content.json')) != marker:
            raise ValueError('Packaged marker changed')
    with Path(package).open('rb') as stream:
        package_hash = digest(stream)
    result = {'result': 'PASS private full-content MSIX inventory and pinned hashes',
              'packageBytes': Path(package).stat().st_size, 'packageSha256': package_hash,
              'nativeImages': len(native['files']), 'contentFiles': len(files),
              'expandedContentBytes': sum(f['bytes'] for f in files),
              'fortniteExecuted': False, 'entryReached': False, 'lobbyReached': False,
              'xboxTested': False, 'matchmakingAvailable': False,
              'signatureTrustVerification': 'Requires separate Windows signtool result; not performed by this Python audit'}
    Path(output).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest='command', required=True)
    m = sub.add_parser('mapping')
    for name in ('layout', 'content', 'manifest', 'output'):
        m.add_argument(name)
    a = sub.add_parser('audit')
    for name in ('package', 'content', 'manifest', 'prepared_plan', 'output'):
        a.add_argument(name)
    args = vars(parser.parse_args())
    command = args.pop('command')
    globals()[command](**args)
