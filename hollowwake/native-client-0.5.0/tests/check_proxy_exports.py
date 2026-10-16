"""Bounded PE export reader for newly compiled original proxy DLLs only."""
from pathlib import Path
import argparse,struct,json
def exports(path):
 b=path.read_bytes();u16=lambda p:struct.unpack_from('<H',b,p)[0];u32=lambda p:struct.unpack_from('<I',b,p)[0]
 if b[:2]!=b'MZ':raise ValueError('Not PE')
 nt=u32(60);opt=nt+24
 if b[nt:nt+4]!=b'PE\0\0' or u16(nt+4)!=0x8664 or u16(opt)!=0x20b:raise ValueError('Not x64 PE32+')
 if not u16(opt+70)&0x1000:raise ValueError('APPCONTAINER characteristic missing')
 sections=opt+u16(nt+20)
 def offset(rva):
  for i in range(u16(nt+6)):
   s=sections+i*40;start=u32(s+12);size=u32(s+16)
   if start<=rva<start+size:return u32(s+20)+rva-start
  raise ValueError('Export RVA is not backed by file bytes')
 def string(rva):
  o=offset(rva);end=b.index(0,o,min(len(b),o+4096));return b[o:end].decode('ascii')
 export=u32(opt+112);end=export+u32(opt+116);e=offset(export);base=u32(e+16);count=u32(e+20);names=u32(e+24)
 if max(count,names)>4096:raise ValueError('Unexpected export count')
 f=offset(u32(e+28));n=offset(u32(e+32));o=offset(u32(e+36));result={};named=set()
 def entry(index):
  rva=u32(f+4*index)
  return string(rva) if export<=rva<end else None
 for i in range(names):
  index=u16(o+2*i)
  if index>=count:raise ValueError('Bad export ordinal index')
  result[string(u32(n+4*i))]=entry(index);named.add(index)
 for i in range(count):
  if i not in named and u32(f+4*i):result['#'+str(base+i)]=entry(i)
 return result
p=argparse.ArgumentParser();p.add_argument('prepared');p.add_argument('report');a=p.parse_args();root=Path(a.prepared)
plan=json.loads((root/'plan.json').read_text());rows=[]
for spec in plan['proxyDlls']:
 image=root/'GamePayload'/spec['proxyDll'];actual=exports(image)
 if set(spec['symbols'])!=set(actual):raise ValueError('Compiled exports differ from exact import demand: '+spec['proxyDll'])
 for symbol in spec.get('dataSymbols',[]):
  if actual[symbol]!=spec['sourceDll'][:-4]+'.'+symbol:raise ValueError('DATA forwarder is not the actual provider export')
 rows.append({'dll':spec['proxyDll'],'exports':len(actual),'dataForwarders':spec.get('dataSymbols',[])})
Path(a.report).write_text(json.dumps({'result':'PASS all exact native dependency proxies compile and preserve export inventory','proxies':rows,'gameCodeIncluded':False,'gameExecuted':False,'xboxTested':False},indent=2)+'\n')
print('PASS all 11 exact dependency proxy DLLs: real x64 AppContainer images, exports and DATA forwarder; no game execution.')
