"""Real execution of owned complete frames; no game execution or mocked OS API."""
import ctypes,mmap,os,resource,signal,struct,sys,unittest
from return_frame_fixture import build,relocate,repair
@unittest.skipUnless(sys.platform=='linux' and os.uname().machine=='x86_64','Linux x64 owned fixture')
class CompleteFrame(unittest.TestCase):
 def setUp(self):
  self.lib=ctypes.CDLL(None);self.lib.mmap.restype=ctypes.c_void_p;self.lib.mmap.argtypes=[ctypes.c_void_p,ctypes.c_size_t,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_long];self.lib.munmap.argtypes=[ctypes.c_void_p,ctypes.c_size_t];self.maps=[]
 def tearDown(self):
  for address in self.maps:self.lib.munmap(address,4096)
 def allocate(self,low=False):
  address=self.lib.mmap(None,4096,7,0x22|(0x40 if low else 0),-1,0);self.assertNotEqual(address,ctypes.c_void_p(-1).value);self.maps.append(address);self.assertEqual(address<1<<32,low);return address
 def run_fixture(self,mode,low_stack=False,low_code=False):
  code=self.allocate(low_code);stack=self.allocate(low_stack);output=(ctypes.c_uint64*11)();data,fixups=build(mode);data=relocate(data,fixups,code,stack+3072,ctypes.addressof(output));ctypes.memmove(code,data,len(data));value=ctypes.CFUNCTYPE(ctypes.c_uint64)(code)();return value,list(output)
 def test_full_frame_low_and_high_stack_and_dispatch_code(self):
  for low_stack in [False,True]:
   for low_code in [False,True]:
    with self.subTest(low_stack=low_stack,low_code=low_code):
     value,output=self.run_fixture('full',low_stack,low_code);self.assertEqual(value,621);self.assertEqual(output[0],621);self.assertEqual(output[2:6],output[6:10]);self.assertEqual(output[1]&0x8d5,output[10]&0x8d5)
 def test_previous_partial_repair_fails_actual_return(self):
  pid=os.fork()
  if pid==0:
   resource.setrlimit(resource.RLIMIT_CORE,(0,0));self.run_fixture('partial');os._exit(1)
  _,status=os.waitpid(pid,0);self.assertTrue(os.WIFSIGNALED(status));self.assertEqual(os.WTERMSIG(status),signal.SIGSEGV)
 def test_original_sequence_fails_on_high_stack(self):
  pid=os.fork()
  if pid==0:
   resource.setrlimit(resource.RLIMIT_CORE,(0,0));self.run_fixture('original');os._exit(1)
  _,status=os.waitpid(pid,0);self.assertTrue(os.WIFSIGNALED(status));self.assertEqual(os.WTERMSIG(status),signal.SIGSEGV)
class Bounds(unittest.TestCase):
 def test_production_trampoline_returns_to_unchanged_epilogue(self):
  at=0x6316600;body=repair.trampoline(at);self.assertEqual(at+len(body)+struct.unpack('<i',body[-4:])[0],repair.SITE_RVA+len(repair.ORIGINAL))
 def test_branch_refuses_out_of_range(self):
  with self.assertRaises(ValueError):repair.branch(0,1<<40)
if __name__=='__main__':unittest.main(verbosity=2)
