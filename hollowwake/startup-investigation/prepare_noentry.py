"""Exact-build no-entry DLL comparison. No section/code/import/TLS rewriting.

This build has static TLS and a null callback list. A no-entry DLL lets the OS
map the image/assign TLS without executing EXE CRT startup under the loader
lock. The original EXE entry is invoked separately by its pinned RVA. No game
startup check is removed and no replacement DllMain success is supplied.
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import pefile

PIN = '9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'


def prepare(source, destination):
    source, destination = Path(source), Path(destination)
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != PIN or destination.exists():
        raise ValueError('Exact source and new output path required')
    pe = pefile.PE(data=data, fast_load=True)
    try:
        pe.parse_data_directories(directories=[9])
        tls = pe.DIRECTORY_ENTRY_TLS.struct
        callbacks_rva = tls.AddressOfCallBacks - pe.OPTIONAL_HEADER.ImageBase
        if not tls.AddressOfCallBacks or pe.get_data(callbacks_rva, 8) != bytes(8):
            raise ValueError('No-entry comparison requires the observed null TLS callback array')
        if pe.OPTIONAL_HEADER.AddressOfEntryPoint != 0x369e098:
            raise ValueError('Original entry differs')
        result = bytearray(data)
        struct.pack_into('<H', result, pe.FILE_HEADER.get_field_absolute_offset('Characteristics'), pe.FILE_HEADER.Characteristics | 0x2000)
        struct.pack_into('<I', result, pe.OPTIONAL_HEADER.get_field_absolute_offset('AddressOfEntryPoint'), 0)
        struct.pack_into('<I', result, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), 0)
        security = pe.OPTIONAL_HEADER.DATA_DIRECTORY[4]
        struct.pack_into('<II', result, security.get_file_offset(), 0, 0)
        for section in pe.sections:
            start, size = section.PointerToRawData, section.SizeOfRawData
            assert result[start:start + size] == data[start:start + size]
            assert result[section.get_file_offset():section.get_file_offset() + 40] == data[section.get_file_offset():section.get_file_offset() + 40]
        for index, directory in enumerate(pe.OPTIONAL_HEADER.DATA_DIRECTORY):
            if index != 4:
                at = directory.get_file_offset()
                assert result[at:at + 8] == data[at:at + 8]
        destination.write_bytes(result)
        return {'inputSha256': PIN, 'outputSha256': hashlib.sha256(result).hexdigest(),
                'bytes': len(result), 'originalEntryRva': 0x369e098,
                'allSectionHeadersAndContentsUnchanged': True,
                'originalExportsImportsTlsUnwindRelocationsUnchanged': True,
                'originalInputUnchanged': source.read_bytes() == data,
                'dllEntryRva': 0, 'tlsCallbacksOriginallyEmpty': True,
                'signatureInvalidatedDirectoryCleared': True,
                'executed': False, 'xboxTested': False}
    finally:
        pe.close()


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('destination')
    parser.add_argument('report')
    args = parser.parse_args()
    result = prepare(args.source, args.destination)
    Path(args.report).write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))
