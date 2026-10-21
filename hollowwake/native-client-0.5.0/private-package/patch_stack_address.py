"""Exact-build pointer-width repair for the observed Xbox startup write.

Private preparation only: never executes a game image. Preserve the computed target, arithmetic flags and epilogue; adapt the
complete pointer/value/slot sequence to a 64-bit return address. This is
not a general instruction rewriter or a claim of completed engine startup.
"""
from pathlib import Path
import hashlib
import struct

INPUT_SHA256 = '8548e7edc221a48d12648c98500092bfeb3c0c6d0e4e69d66e417a245c2015af'
SITE_RVA = 0x05FA3A6C
ORIGINAL = bytes.fromhex('8945508b45508d8d9000000083e9048901')
EPILOGUE = bytes.fromhex('488b5d70488b7578488da5800000005dc3')
POINTER_CALCULATION = bytes.fromhex('8d8d9000000083e904')
FULL_VALUE_ROUNDTRIP = bytes.fromhex('48894550488b4550')
FULL_WRITE = bytes.fromhex('488901')
# Replay the original ECX arithmetic first so EFLAGS are unchanged. LEA RCX
# then addresses the 8-byte return slot at RBP+0x88 without changing flags.
# RAX retains the full RVA+module-base value; no fixed return/success is supplied.
RESTORE_POINTER = bytes.fromhex('488d8d88000000')

def branch(source_rva, destination_rva):
    displacement = destination_rva - (source_rva + 5)
    if not -(1 << 31) <= displacement < (1 << 31):
        raise ValueError('Direct branch is outside signed rel32 range')
    return b'\xe9' + struct.pack('<i', displacement)

def trampoline(at_rva):
    body = FULL_VALUE_ROUNDTRIP + POINTER_CALCULATION + RESTORE_POINTER + FULL_WRITE
    return body + branch(at_rva + len(body), SITE_RVA + len(ORIGINAL))

def patch(path):
    path = Path(path)
    original = path.read_bytes()
    digest = hashlib.sha256(original).hexdigest()
    if digest != INPUT_SHA256:
        raise ValueError('Stack repair requires the exact preserved converted client SHA-256')
    import pefile
    pe = pefile.PE(data=original, fast_load=True)
    try:
        if pe.FILE_HEADER.Machine != 0x8664 or pe.OPTIONAL_HEADER.Magic != 0x20b:
            raise ValueError('Stack repair requires PE32+ x64')
        site = pe.get_offset_from_rva(SITE_RVA)
        if original[site:site + len(ORIGINAL) + len(EPILOGUE)] != ORIGINAL + EPILOGUE:
            raise ValueError('Observed fault instruction window differs')
        section = max(pe.sections, key=lambda s: s.PointerToRawData + s.SizeOfRawData)
        if (section.Characteristics & 0xe0000000) != 0x60000000:
            raise ValueError('Final section must retain RX, never writable executable protection')
        if section.PointerToRawData + section.SizeOfRawData != len(original):
            raise ValueError('Unknown trailing overlay')
        if section.Misc_VirtualSize > section.SizeOfRawData:
            raise ValueError('Final section has virtual-only data')
        if any(s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData) >
               section.VirtualAddress + section.SizeOfRawData for s in pe.sections):
            raise ValueError('Final virtual and raw section order differs')
        if pe.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size:
            raise ValueError('Authenticode input must be prepared before this repair')
        alignment = pe.OPTIONAL_HEADER.FileAlignment
        section_alignment = pe.OPTIONAL_HEADER.SectionAlignment
        if any(not a or a & (a - 1) for a in (alignment, section_alignment)):
            raise ValueError('Invalid image alignment')
        at_rva = section.VirtualAddress + section.SizeOfRawData
        body = trampoline(at_rva)
        virtual_size = section.SizeOfRawData + len(body)
        raw_size = (virtual_size + alignment - 1) & ~(alignment - 1)
        image_size = (section.VirtualAddress + virtual_size + section_alignment - 1) & ~(section_alignment - 1)
        output = bytearray(original)
        output.extend(body)
        output.extend(bytes(section.PointerToRawData + raw_size - len(output)))
        output[site:site + len(ORIGINAL)] = branch(SITE_RVA, at_rva) + b'\x90' * (len(ORIGINAL) - 5)
        struct.pack_into('<I', output, section.get_field_absolute_offset('Misc_VirtualSize'), virtual_size)
        struct.pack_into('<I', output, section.get_field_absolute_offset('SizeOfRawData'), raw_size)
        struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'), image_size)
        struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfCode'),
                         pe.OPTIONAL_HEADER.SizeOfCode + raw_size - section.SizeOfRawData)
        struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), 0)
        # No import/export/TLS/unwind/relocation record is removed or retargeted.
        # Both new branches are direct. No stack operation or call is introduced.
        if output[site + len(ORIGINAL):site + len(ORIGINAL) + len(EPILOGUE)] != EPILOGUE:
            raise AssertionError('Original epilogue changed')
        report = {'inputSha256': digest, 'outputSha256': hashlib.sha256(output).hexdigest(),
                  'inputBytes': len(original), 'outputBytes': len(output),
                  'siteRva': SITE_RVA, 'previousWriteFaultRva': 0x05FA3A7B, 'observedReturnFaultRva': 0x05FA3A8D,
                  'trampolineRva': at_rva, 'originalHex': ORIGINAL.hex(),
                  'trampolineHex': body.hex(), 'returnWritePreservedWithCorrect64BitWidth': True, 'originalEpiloguePreserved': True,
                  'returnSlotOffset': 0x88, 'returnPointerBytes': 8,
                  'originalArithmeticFlagsPreserved': True,
                  'gameExecuted': False, 'xboxTested': False}
    finally:
        pe.close()
    path.write_bytes(output)
    return report
