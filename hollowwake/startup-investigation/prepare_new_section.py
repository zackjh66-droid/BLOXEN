"""Private exact-build conversion with a distinct owned RX section.

The previous fallback extends the client's final code section. This comparison
instead moves the NT header/section table back 40 bytes within the DOS stub,
creating one real section-header slot before the original opaque header data.
All nine original sections retain their exact RVAs, extents, flags and bytes.
No original code, checks, TLS, unwind or relocation entries are patched.
"""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import struct
import pefile

PIN = '9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
ROOT = Path(__file__).resolve().parents[1]


def prepare(source, destination, report_path):
    source, destination, report_path = Path(source), Path(destination), Path(report_path)
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != PIN or destination.exists() or report_path.exists():
        raise ValueError('Exact original client and fresh output paths required')
    original = pefile.PE(data=data, fast_load=True)
    shifted = None
    try:
        nt = original.DOS_HEADER.e_lfanew
        if nt != 0x1d0 or original.FILE_HEADER.NumberOfSections != 9 or original.OPTIONAL_HEADER.SizeOfHeaders != 0x600:
            raise ValueError('Exact observed header layout required')
        new_nt = nt - 40
        size = 24 + original.FILE_HEADER.SizeOfOptionalHeader + original.FILE_HEADER.NumberOfSections * 40
        old_end = nt + size
        if old_end != 0x440 or new_nt < 0x80:
            raise ValueError('Header relocation would overlap DOS requirements or opaque data')
        intermediate = bytearray(data)
        intermediate[new_nt:new_nt + size] = data[nt:old_end]
        intermediate[new_nt + size:old_end] = bytes(40)
        struct.pack_into('<I', intermediate, 0x3c, new_nt)
        assert intermediate[old_end:0x600] == data[old_end:0x600]
        shifted = destination.with_name(destination.name + '.header-stage')
        if shifted.exists():
            raise ValueError('Fresh header stage required')
        shifted.write_bytes(intermediate)
        spec = importlib.util.spec_from_file_location('converter', ROOT / 'runtime-native-route/native-image/convert_image.py')
        converter = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(converter)
        conversion = converter.convert(shifted, destination, hashlib.sha256(intermediate).hexdigest())
        if conversion['exportStorage'] != 'new RX section':
            raise AssertionError('Original code-section extension must not be used')
        result = pefile.PE(str(destination), fast_load=True)
        try:
            if result.FILE_HEADER.NumberOfSections != 10:
                raise AssertionError('Distinct owned section absent')
            for old, new in zip(original.sections, result.sections[:9]):
                assert old.__pack__() == new.__pack__(), 'Original section header changed'
                assert old.get_data() == new.get_data(), 'Original section bytes changed'
            for index in [1, 2, 3, 5, 6, 9, 10, 12, 13]:
                assert original.OPTIONAL_HEADER.DATA_DIRECTORY[index].__pack__() == result.OPTIONAL_HEADER.DATA_DIRECTORY[index].__pack__()
            original.parse_data_directories(directories=[0])
            result.parse_data_directories(directories=[0])
            exports = lambda p: {(s.ordinal, s.name, s.address, s.forwarder) for s in p.DIRECTORY_ENTRY_EXPORT.symbols}
            assert exports(original).issubset(exports(result)), 'Original export changed'
            assert result.__data__[0x440:0x600] == data[0x440:0x600]
            receipt = {'originalInputSha256': PIN, 'outputSha256': hashlib.sha256(destination.read_bytes()).hexdigest(),
                       'outputBytes': destination.stat().st_size, 'oldNtHeaderOffset': nt, 'newNtHeaderOffset': new_nt,
                       'originalSectionHeadersAndContentsUnchanged': True, 'opaqueHeaderDataPreserved': True,
                       'originalExportsPreserved': len(exports(original)), 'newSectionName': result.sections[-1].Name.rstrip(b'\0').decode(),
                       'dosStubRichTailChanged': True, 'originalInputUnchanged': source.read_bytes() == data,
                       'conversion': conversion, 'gameExecuted': False, 'xboxTested': False}
            report_path.write_text(json.dumps(receipt, indent=2) + '\n')
            return receipt
        finally:
            result.close()
    finally:
        original.close()
        if shifted is not None:
            shifted.unlink(missing_ok=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('source')
    parser.add_argument('destination')
    parser.add_argument('report')
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.destination, args.report)))
