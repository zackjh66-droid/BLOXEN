"""Isolate PE metadata changes while retaining the original OS entry point.
No original code, branches, checks, imports or runtime state are patched.
Private native Windows investigation only; produces no Xbox package.
"""
import argparse,hashlib,importlib.util,json,struct
from pathlib import Path
PIN='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('destination');p.add_argument('report');p.add_argument('--variant',choices=['certificate','converted-original-entry'],required=True);a=p.parse_args()
src=Path(a.source);dst=Path(a.destination);raw=src.read_bytes()
assert src.resolve()!=dst.resolve() and hashlib.sha256(raw).hexdigest()==PIN
nt=struct.unpack_from('<I',raw,0x3c)[0];opt=nt+24;entry=struct.unpack_from('<I',raw,opt+16)[0]
if a.variant=='converted-original-entry':
 root=Path(__file__).resolve().parents[1]
 spec=importlib.util.spec_from_file_location('converter',root/'runtime-native-route/native-image/convert_image.py');mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
 mod.convert(src,dst,PIN);data=bytearray(dst.read_bytes())
 struct.pack_into('<H',data,nt+22,struct.unpack_from('<H',raw,nt+22)[0])
 struct.pack_into('<I',data,opt+16,entry)
else:
 data=bytearray(raw);struct.pack_into('<I',data,opt+64,0);struct.pack_into('<II',data,opt+112+4*8,0,0)
assert data[opt+16:opt+20]==raw[opt+16:opt+20]
count=struct.unpack_from('<H',raw,nt+6)[0];table=opt+struct.unpack_from('<H',raw,nt+20)[0]
for i in range(count):
 size,off=struct.unpack_from('<II',raw,table+i*40+16)
 assert data[off:off+size]==raw[off:off+size]
dst.write_bytes(data)
r={'variant':a.variant,'originalSha256':PIN,'outputSha256':hashlib.sha256(data).hexdigest(),'originalOsEntryRetained':True,'originalSectionBytesUnchanged':True,'checksPatched':False,'executed':False,'xboxTested':False}
Path(a.report).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r))
