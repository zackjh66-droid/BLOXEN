"""Private, bounded exact-build service-host patch. No code execution, TLS bypass,
function-address hooks, authorization patch or instruction changes.
"""
from pathlib import Path
import hashlib,json,re
import pefile
INPUT_SHA256='95196e6a0e5ab1e23a15dabfe60c0948825adf1b93cb58d87b10655efb037f92'
TARGET='hollowwake.jihnjohn10.chatgpt.site'
def patch(path,expected_sha256=INPUT_SHA256):
 path=Path(path);data=path.read_bytes()
 if hashlib.sha256(data).hexdigest()!=expected_sha256:raise ValueError('Unexpected exact converted client; no host patch applied')
 pe=pefile.PE(data=data,fast_load=True);out=bytearray(data);changes=[]
 # Only standalone account/Fortnite service hostname literals. Presence and
 # unrelated services are intentionally untouched; they need their own transport.
 for match in re.finditer(rb'(?:[\x20-\x7e]\0){12,}',data):
  host=match.group().decode('utf-16-le')
  if not re.fullmatch(r'(?:account|fortnite)-public-service-[a-z0-9-]+\.ol\.epicgames\.com',host):continue
  start,end=match.span();section=next((s for s in pe.sections if s.PointerToRawData<=start and end+2<=s.PointerToRawData+s.SizeOfRawData),None)
  if section is None or section.Characteristics&0x20000000 or data[end:end+2]!=b'\0\0':raise ValueError('Service literal is not bounded nonexecutable data')
  replacement=TARGET.encode('utf-16-le')+b'\0\0'
  if len(replacement)>end+2-start:raise ValueError('Host does not fit original allocation')
  out[start:end+2]=replacement.ljust(end+2-start,b'\0')
  changes.append({'host':host,'fileOffset':start,'rva':pe.get_rva_from_offset(start),'originalBytes':end+2-start,'targetHost':TARGET})
 pe.close()
 if len(changes)!=27 or sum(c['host'].startswith('account-') for c in changes)!=2:raise ValueError('Exact-build service literal inventory changed')
 path.write_bytes(out)
 return {'inputSha256':expected_sha256,'outputSha256':hashlib.sha256(out).hexdigest(),'targetOrigin':'https://'+TARGET,'changes':changes,'instructionsChanged':False,'tlsValidationDisabled':False,'executed':False,'nativeClientLoginTested':False,'scope':'Literal host changes only; assembled URLs and acceptance require execution evidence'}
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('image');p.add_argument('report');a=p.parse_args();r=patch(a.image);Path(a.report).write_text(json.dumps(r,indent=2));print(json.dumps({'patchedLiterals':len(r['changes']),'sha256':r['outputSha256'],'executed':False}))
