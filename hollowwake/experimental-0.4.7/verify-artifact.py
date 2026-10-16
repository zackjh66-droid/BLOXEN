import io,zipfile,json,hashlib,base64,sys,xml.etree.ElementTree as ET
from pathlib import Path
from cryptography import x509
import pefile
p=Path(sys.argv[1]);out=Path(sys.argv[2]);z=zipfile.ZipFile(p);root="Hollowwake_0.4.7.0_x64_Test/";a=zipfile.ZipFile(io.BytesIO(z.read(root+"Hollowwake_0.4.7.0_x64.appx")));v=zipfile.ZipFile(io.BytesIO(z.read(root+"Dependencies/x64/Microsoft.VCLibs.x64.14.00.appx")))
exports={}
for n in v.namelist():
 if n.lower().endswith(".dll"):
  q=pefile.PE(data=v.read(n));exports[n.rsplit("/",1)[-1].lower()]={s.name.decode() for s in getattr(q,"DIRECTORY_ENTRY_EXPORT",type("X",(),{"symbols":[]})()).symbols if s.name}
r=[]
for n in ["HollowwakeClientRuntime.dll","HollowwakeTlsDonor.dll","HollowwakeTlsSlot01.dll"]:
 q=pefile.PE(data=a.read(n));imports=[]
 for imp in getattr(q,"DIRECTORY_ENTRY_IMPORT",[]):
  dll=imp.dll.decode().lower();names=[x.name.decode() if x.name else "#"+str(x.ordinal) for x in imp.imports];missing=[x for x in names if dll in exports and x not in exports[dll]]
  imports.append({"dll":dll,"count":len(names),"checkedAgainstSuppliedFramework":dll in exports,"missingFromFramework":missing})
 r.append({"file":n,"sha256":hashlib.sha256(a.read(n)).hexdigest(),"machine":hex(q.FILE_HEADER.Machine),"appContainer":bool(q.OPTIONAL_HEADER.DllCharacteristics&0x1000),"imports":imports})
assert not any(i["missingFromFramework"] for x in r for i in x["imports"])
ns={"b":"http://schemas.microsoft.com/appx/2010/blockmap"};bm=ET.fromstring(a.read("AppxBlockMap.xml"));blocks=0
for f in bm.findall("b:File",ns):
 name=f.attrib["Name"].replace(chr(92),"/");data=a.read(name);assert len(data)==int(f.attrib["Size"]);offset=0
 for b in f.findall("b:Block",ns):
  assert base64.b64encode(hashlib.sha256(data[offset:offset+65536]).digest()).decode()==b.attrib["Hash"],name
  offset+=65536;blocks+=1
cert=x509.load_der_x509_certificate(z.read("HollowwakeExperiment.cer"))
result={"scope":"Static package block-map hashes, PE architecture/AppContainer flags and native imports against supplied x64 VCLibs; Windows signature verification is in signature.log; Xbox unverified","artifactSha256":hashlib.sha256(p.read_bytes()).hexdigest(),"blockMapBlocksVerified":blocks,"certificateSubject":cert.subject.rfc4514_string(),"certificateExpires":str(cert.not_valid_after_utc),"files":r}
out.write_text(json.dumps(result,indent=2));print(json.dumps({"blockMapBlocksVerified":blocks,"nativeModulesChecked":len(r),"missingVCLibsExports":0,"certificateExpires":result["certificateExpires"]}))
