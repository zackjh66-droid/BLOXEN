"""Reproduce the signed data-only ZIP index from the unchanged verified archive.
Reads metadata and hashes only; never extracts or executes archive members.
"""
import argparse, hashlib, json, pathlib, stat, struct, zipfile

def build_index(archive, plan):
    pin = plan['archive']
    if archive.stat().st_size != pin['bytes']:
        raise ValueError('Original archive size differs')
    digest = hashlib.sha256()
    with archive.open('rb') as source:
        while block := source.read(4 * 1024 * 1024):
            digest.update(block)
    if digest.hexdigest() != pin['sha256']:
        raise ValueError('Original archive SHA-256 differs; do not substitute')
    records = []
    with zipfile.ZipFile(archive) as zipped, archive.open('rb') as raw:
        infos = zipped.infolist()
        if len(infos) > 100000 or len({x.filename for x in infos}) != len(infos):
            raise ValueError('Unsafe original archive inventory')
        for entry in infos:
            kind = stat.S_IFMT(entry.external_attr >> 16)
            if entry.filename.startswith('/') or '\\' in entry.filename or ':' in entry.filename or '\0' in entry.filename or any(x in ('.', '..') for x in entry.filename.split('/')) or kind not in (0, stat.S_IFREG, stat.S_IFDIR):
                raise ValueError('Unsafe original archive path/link')
        for selected in plan['files']:
            entry = zipped.getinfo(selected['path'])
            if entry.file_size != selected['bytes'] or entry.compress_type not in (0, 8) or entry.flag_bits & 1:
                raise ValueError('Unsupported or changed pinned data member')
            raw.seek(entry.header_offset)
            header = raw.read(30)
            if len(header) != 30 or header[:4] != b'PK\x03\x04':
                raise ValueError('Invalid ZIP local header')
            name_bytes, extra_bytes = struct.unpack_from('<HH', header, 26)
            encoded_name = raw.read(name_bytes)
            name = encoded_name.decode('utf-8' if entry.flag_bits & 0x800 else 'cp437')
            method, flags = struct.unpack_from('<H', header, 8)[0], struct.unpack_from('<H', header, 6)[0]
            offset = entry.header_offset + 30 + name_bytes + extra_bytes
            if name != selected['path'] or method != entry.compress_type or flags != entry.flag_bits or offset > pin['bytes'] - entry.compress_size:
                raise ValueError('Local and central ZIP metadata differ')
            records.append(dict(selected, offset=offset, compressed=entry.compress_size, method=entry.compress_type))
    return dict(schema=1, build=plan['build'], archiveSha256=pin['sha256'], archiveBytes=pin['bytes'], entries=records)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('archive', type=pathlib.Path)
    parser.add_argument('plan', type=pathlib.Path)
    parser.add_argument('index', type=pathlib.Path)
    parser.add_argument('--verify', action='store_true', help='Read-only comparison; do not change the supplied signed-index source')
    args = parser.parse_args()
    result = build_index(args.archive, json.loads(args.plan.read_text()))
    if args.verify:
        if result != json.loads(args.index.read_text()):
            raise ValueError('Reproduced index differs')
    else:
        if args.index.exists():
            raise ValueError('Refusing to overwrite an index; use a separate output or --verify')
        args.index.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(result='PASS exact original archive SHA-256 and selected local-header extents', archiveSha256=result['archiveSha256'], files=len(result['entries']), compressedBytes=sum(x['compressed'] for x in result['entries']), expandedBytes=sum(x['bytes'] for x in result['entries']), extracted=False, gameExecuted=False)))

if __name__ == '__main__':
    main()
