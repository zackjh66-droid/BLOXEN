"""Owned static transport fixtures; no EXE contents are executed."""
import unittest,tempfile,io,json,zipfile,hashlib,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'private-package'))
from fetch_code_prefix import fetch_code_prefix,SOURCE
class Response(io.BytesIO):
 def __init__(self,data,status=200,header=None):super().__init__(data);self.status=status;self.headers={} if header is None else {'Content-Range':header}
class Opener:
 def __init__(self,data,status=200,header=None):self.data=data;self.status=status;self.header=header
 def open(self,*args,**kwargs):return Response(self.data,self.status,self.header)
class Checks(unittest.TestCase):
 def fixture(self,root):
  b=io.BytesIO()
  with zipfile.ZipFile(b,'w') as z:
   z.writestr('6.21/Owned.dll',b'owned non-executable fixture'*80,compress_type=8)
   z.writestr('6.21/Owned.exe',b'another inert fixture',compress_type=0)
   z.writestr('6.21/Splash.bmp',b'non-code not needed for native packaging')
  data=b.getvalue();files=[]
  with zipfile.ZipFile(io.BytesIO(data)) as z:
   for i in z.infolist():
    at=i.header_offset;name,extra=struct.unpack_from('<HH',data,at+26)
    files.append({'path':i.filename,'offset':at+30+name+extra,'compressed':i.compress_size,'bytes':i.file_size,'method':i.compress_type,'sha256':hashlib.sha256(z.read(i)).hexdigest()})
  p=root/'manifest.json';p.write_text(json.dumps({'source':SOURCE,'archiveBytes':12528679454,'files':files}));return p,data,files
 def test_ignored_range_validates_selected_code(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);p,data,files=self.fixture(root);out=root/'out';r=fetch_code_prefix(p,out,Opener(data))
   self.assertEqual(r['codeFiles'],2);self.assertFalse(r['completeArchiveDownloaded']);self.assertEqual(len([p for p in out.rglob('*') if p.is_file()]),2)
   for i in files[:2]:self.assertEqual(hashlib.sha256((out/i['path']).read_bytes()).hexdigest(),i['sha256'])
 def test_corruption_never_stages_code(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);p,data,files=self.fixture(root);bad=bytearray(data);bad[files[1]['offset']]^=1
   with self.assertRaisesRegex(ValueError,'SHA256'):fetch_code_prefix(p,root/'out',Opener(bad))
   self.assertFalse((root/'out').exists())
 def test_truncation_never_stages_code(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);p,data,files=self.fixture(root)
   with self.assertRaisesRegex(ValueError,'Truncated'):fetch_code_prefix(p,root/'out',Opener(data[:files[0]['offset']]))
   self.assertFalse((root/'out').exists())
 def test_wrong_range_refused(self):
  with tempfile.TemporaryDirectory() as d:
   root=Path(d);p,data,_=self.fixture(root)
   with self.assertRaisesRegex(ValueError,'Incorrect source range'):fetch_code_prefix(p,root/'out',Opener(data,206,'bytes 1-3/5'))
if __name__=='__main__':unittest.main()
