"""Execute owned minimal arithmetic fixtures, never a Fortnite image.

On Linux x64, compare the original low-address behavior with the repaired
behavior on low and high addresses. Exercise real machine writes and EFLAGS.
Static image mutation checks are separate and never load the game.
"""
import ctypes
import importlib.util
import mmap
import os
from pathlib import Path
import signal
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('stack_repair', ROOT / 'private-package/patch_stack_address.py')
repair = importlib.util.module_from_spec(spec)
spec.loader.exec_module(repair)

@unittest.skipUnless(sys.platform == 'linux' and os.uname().machine == 'x86_64', 'Owned Linux x64 machine-code fixture')
class ArithmeticBehavior(unittest.TestCase):
    def setUp(self):
        self.libc = ctypes.CDLL(None)
        self.libc.mmap.restype = ctypes.c_void_p
        self.libc.mmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_int,
                                  ctypes.c_int, ctypes.c_int, ctypes.c_long]
        self.libc.munmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t]
        self.low = self.libc.mmap(None, 4096, 3, 0x22 | 0x40, -1, 0)  # MAP_32BIT
        self.assertNotIn(self.low, (None, ctypes.c_void_p(-1).value))
        self.assertLess(self.low + 4096, 1 << 32)
        self.high_map = mmap.mmap(-1, 4096, prot=mmap.PROT_READ | mmap.PROT_WRITE)
        self.high = ctypes.addressof(ctypes.c_char.from_buffer(self.high_map))
        self.assertGreater(self.high, 1 << 32)
        self.code = mmap.mmap(-1, 4096, prot=7)
        self.entry = ctypes.addressof(ctypes.c_char.from_buffer(self.code))
        # SysV owned wrapper: save RBP; RBP=first argument; EAX=second.
        self.prefix = bytes.fromhex('554889fd89f0')
        # The original write remains after the patched span. Capture flags
        # before restoring RBP. Neither POP nor RET changes arithmetic flags.
        self.tail = repair.WRITE + bytes.fromhex('9c585dc3')
        self.call = ctypes.CFUNCTYPE(ctypes.c_uint64, ctypes.c_uint64, ctypes.c_uint32)(self.entry)

    def tearDown(self):
        self.code.close()
        self.high_map.close()
        self.libc.munmap(self.low, 4096)

    def install(self, fixed):
        offset = len(self.prefix)
        at = 128
        sequence = (repair.branch(offset, at) + bytes(4)) if fixed else repair.ORIGINAL
        self.code.seek(0)
        self.code.write(self.prefix + sequence + self.tail)
        if fixed:
            # Same production trampoline bytes; only fixture jump origins differ.
            body = repair.trampoline(repair.SITE_RVA + at - offset)
            self.code.seek(at)
            self.code.write(body)

    def test_low_address_write_and_arithmetic_flags_unchanged(self):
        self.install(False)
        expected = self.call(self.low + 128 - 0x8c, 0x621621)
        self.assertEqual(ctypes.c_uint32.from_address(self.low + 128).value, 0x621621)
        self.install(True)
        actual = self.call(self.low + 128 - 0x8c, 0x4526925)
        self.assertEqual(ctypes.c_uint32.from_address(self.low + 128).value, 0x4526925)
        self.assertEqual(actual & 0x8d5, expected & 0x8d5)

    def test_high_address_write_uses_full_pointer_and_keeps_original_flags(self):
        # Reference executes the original arithmetic but substitutes the known
        # full pointer only immediately before the store, without touching flags.
        self.code.seek(0)
        self.code.write(self.prefix + repair.ORIGINAL + repair.RESTORE_POINTER + self.tail)
        expected = self.call(self.high + 128 - 0x8c, 0x621621)
        self.install(True)
        actual = self.call(self.high + 128 - 0x8c, 0x4526925)
        self.assertEqual(ctypes.c_uint32.from_address(self.high + 128).value, 0x4526925)
        self.assertEqual(actual & 0x8d5, expected & 0x8d5)

    def test_original_high_address_fault_reproduced_in_child(self):
        self.install(False)
        pid = os.fork()
        if pid == 0:
            import resource
            resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
            self.call(self.high + 128 - 0x8c, 621)
            os._exit(1)
        _, status = os.waitpid(pid, 0)
        self.assertTrue(os.WIFSIGNALED(status))
        self.assertEqual(os.WTERMSIG(status), signal.SIGSEGV)

class Bounds(unittest.TestCase):
    def test_branch_refuses_out_of_range(self):
        with self.assertRaises(ValueError):
            repair.branch(0, 1 << 40)

    def test_trampoline_returns_to_original_write(self):
        at = 0x6316600
        body = repair.trampoline(at)
        self.assertTrue(body.startswith(repair.ORIGINAL + repair.RESTORE_POINTER))
        displacement = struct.unpack('<i', body[-4:])[0]
        self.assertEqual(at + len(body) + displacement, repair.SITE_RVA + len(repair.ORIGINAL))

if __name__ == '__main__':
    unittest.main(verbosity=2)
