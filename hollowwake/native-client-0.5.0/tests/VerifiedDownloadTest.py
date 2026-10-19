"""Real HTTP truncation/resume fixtures; no game code or archive execution."""
import sys,hashlib,tempfile,threading,unittest
from pathlib import Path
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'private-package'))
from verified_download import download
DATA=(b'Original Hollowwake transfer fixture.\n'*10000)
class DownloadTest(unittest.TestCase):
 def run_case(self,mode):
  requests=[]
  class Handler(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):
    value=self.headers.get('Range');requests.append(value);start=int(value[6:-1]) if value else 0
    if not value:
     self.send_response(200);self.send_header('Content-Length',str(len(DATA)));self.send_header('ETag','"owned-fixture"');self.end_headers();self.wfile.write(DATA[:12345]);self.close_connection=True;return
    self.send_response(206 if mode!='ignored' else 200);self.send_header('Content-Length',str(len(DATA)-start));self.send_header('Content-Range',f'bytes {start}-{len(DATA)-1}/{len(DATA)}');self.send_header('ETag','"changed"' if mode=='changed' else '"owned-fixture"');self.end_headers();self.wfile.write(DATA[start:])
  server=ThreadingHTTPServer(('127.0.0.1',0),Handler);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
  try:
   with tempfile.TemporaryDirectory() as d:
    target=Path(d)/'owned.bin';url=f'http://127.0.0.1:{server.server_port}/fixture'
    if mode=='good':
     download(url,target,len(DATA),hashlib.sha256(DATA).hexdigest());self.assertEqual(target.read_bytes(),DATA);self.assertEqual(requests,[None,'bytes=12345-'])
    else:
     with self.assertRaises(ValueError):download(url,target,len(DATA),hashlib.sha256(DATA).hexdigest())
  finally:server.shutdown();server.server_close();thread.join()
 def test_actual_truncation_and_exact_resume(self):self.run_case('good')
 def test_changed_etag_is_rejected(self):self.run_case('changed')
 def test_ignored_resume_range_is_rejected(self):self.run_case('ignored')
if __name__=='__main__':unittest.main(testRunner=unittest.TextTestRunner(stream=sys.stdout,verbosity=2))
