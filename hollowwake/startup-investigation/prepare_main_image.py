"""Private controlled EXE-main-image comparison; no inputs are executed here.

Reuse the existing bounded converter's append/extent checks. Restore the exact
original executable characteristics and replace only its OWN DllMain
shim with LEA RAX,original_entry; JMP RAX. All original game instructions,
imports, TLS and relocations remain as supplied. This is not an Xbox bootstrap.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import struct

PIN = '9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
ROOT = Path(__file__).resolve().parents[1]


def prepare(source, destination):
    source, destination = Path(source), Path(destination)
    original = source.read_bytes()
    if hashlib.sha256(original).hexdigest() != PIN:
        raise ValueError('Exact original client SHA-256 required')
    spec = importlib.util.spec_from_file_location('converter', ROOT / 'runtime-native-route/native-image/convert_image.py')
    converter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(converter)
    receipt = converter.convert(source, destination, PIN)
    data = bytearray(destination.read_bytes())
    nt = struct.unpack_from('<I', data, 0x3c)[0]
    optional = nt + 24
    sections = struct.unpack_from('<H', data, nt + 6)[0]
    table = optional + struct.unpack_from('<H', data, nt + 20)[0]
    shim = receipt['newDllEntryRva']
    raw = None
    section_header = None
    for index in range(sections):
        at = table + 40 * index
        virtual_size, rva, size, offset = struct.unpack_from('<4I', data, at + 8)
        if rva <= shim and shim + 9 <= rva + size:
            raw = offset + shim - rva
            section_header = at
            break
    if raw is None or data[raw:raw + 9] != bytes.fromhex('b801000000c3000000'):
        raise ValueError('Owned converter shim differs')
    delta = receipt['originalEntryRva'] - (shim + 7)
    data[raw:raw + 9] = b'\x48\x8d\x05' + struct.pack('<i', delta) + b'\xff\xe0'
    struct.pack_into('<I', data, section_header + 8, max(virtual_size, shim - rva + 9))
    original_nt = struct.unpack_from('<I', original, 0x3c)[0]
    flags = struct.unpack_from('<H', original, original_nt + 22)[0]
    if flags & 0x2000:
        raise ValueError('Original must be an EXE')
    struct.pack_into('<H', data, nt + 22, flags)
    destination.write_bytes(data)
    if source.read_bytes() != original:
        raise AssertionError('Original changed')
    return {'originalSha256': PIN, 'outputSha256': hashlib.sha256(data).hexdigest(),
            'originalEntryRva': receipt['originalEntryRva'], 'ownedShimRva': shim,
            'entryRaxRestoredToOriginalEntry': True,
            'originalExeCharacteristicsRestored': True,
            'originalGameInstructionsChanged': False, 'importsRedirected': False,
            'originalInputUnchanged': True, 'executed': False, 'xboxBootstrap': False}


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('destination')
    parser.add_argument('report')
    args = parser.parse_args()
    report = prepare(args.source, args.destination)
    Path(args.report).write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report))
