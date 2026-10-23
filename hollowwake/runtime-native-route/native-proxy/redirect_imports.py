"""Bounded PE import preparation, with a section-preserving native route.
Proxy names do not implement APIs or establish client/Xbox compatibility.
"""
from pathlib import Path
import argparse,hashlib,json,struct
SHORT={'kernel32.dll':'HWK32.dll','kernelbase.dll':'HWKB.dll','ntdll.dll':'HWNT.dll','msvcrt.dll':'HWCRT.dll','user32.dll':'HWU32.dll','dxgi.dll':'HWDG.dll','d3d11.dll':'HWD11.dll','ws2_32.dll':'HWWS2.dll','winhttp.dll':'HWHTTP.dll','wininet.dll':'HWINET.dll','wldap32.dll':'HWLDAP.dll','dnsapi.dll':'HWDNS.dll'}
def alias(name):
 key=name.lower()
 if key in SHORT:return SHORT[key]
 if key.startswith('api-ms-win-core-'):return 'HW'+hashlib.sha256(key.encode('ascii')).hexdigest()[:6].upper()+'.dll'
 return None

def redirect_data(raw,expected,preserve_sections=False):
 if hashlib.sha256(raw).hexdigest()!=expected.lower():raise ValueError('Pinned input SHA-256 mismatch')
 if not 1024<=len(raw)<=512*1024*1024:raise ValueError('Invalid image size')
 def fit(p,n):
  if p<0 or n<0 or p>len(raw) or n>len(raw)-p:raise ValueError('Image bounds exceeded')
 def u16(p):fit(p,2);return struct.unpack_from('<H',raw,p)[0]
 def u32(p):fit(p,4);return struct.unpack_from('<I',raw,p)[0]
 if raw[:2]!=b'MZ':raise ValueError('DOS signature absent')
 pe=u32(60);fit(pe,264);opt=pe+24
 if pe<64 or raw[pe:pe+4]!=b'PE\0\0' or u16(pe+4)!=0x8664 or u16(pe+20)!=240 or u16(opt)!=0x20b or u32(opt+108)<16:raise ValueError('Unsupported PE32+ input')
 count=u16(pe+6)
 if not 1<=count<=96:raise ValueError('Invalid section count')
 headers=u32(opt+60);sections=[];layout=[]
 if headers<opt+240+count*40 or headers>len(raw):raise ValueError('Invalid header extent')
 for i in range(count):
  p=opt+240+i*40;fit(p,40);vs,va,rs,rp=struct.unpack_from('<4I',raw,p+8);fit(rp,rs)
  if (rs and rp<headers) or va<headers or va+max(vs,rs)>u32(opt+56):raise ValueError('Invalid section extent')
  for old in layout:
   if rs and old['rs'] and rp<old['rp']+old['rs'] and old['rp']<rp+rs:raise ValueError('Overlapping raw sections')
   if va<old['va']+max(old['vs'],old['rs']) and old['va']<va+max(vs,rs):raise ValueError('Overlapping virtual sections')
  sections.append((va,rs,rp));layout.append({'header':p,'va':va,'vs':vs,'rs':rs,'rp':rp,'flags':u32(p+36)})
 def off(rva,n=1):
  if 0<=rva<headers and n<=headers-rva:fit(rva,n);return rva
  found=[rp+rva-va for va,rs,rp in sections if va<=rva and n<=rs-(rva-va)]
  if len(found)!=1:raise ValueError('RVA not uniquely file backed')
  fit(found[0],n);return found[0]
 def name(rva):
  p=off(rva);end=raw.find(b'\0',p,min(p+4096,len(raw)))
  if end<0:raise ValueError('Unterminated name')
  off(rva,end-p+1);return raw[p:end].decode('ascii')
 changes=[];proxies={};result=bytearray(raw);occupied={};normal=[];normal_aliases={};delay_aliases=[]
 for directory,width,kind in [(1,20,'normal'),(13,32,'delay')]:
  rva,size=struct.unpack_from('<2I',raw,opt+112+directory*8)
  if not size:continue
  ended=False
  for used in range(0,size-width+1,width):
   p=off(rva+used,width)
   if kind=='normal':
    original,stamp,chain,nrva,iat=struct.unpack_from('<5I',raw,p);lookup=original or iat
   else:
    flags,nrva,module,iat,lookup,bound,unload,stamp=struct.unpack_from('<8I',raw,p)
    if nrva and flags!=1:raise ValueError('Unsupported VA-based delay imports')
   if not nrva:
    if preserve_sections and any(raw[p:p+width]):raise ValueError('Nonzero import terminator')
    ended=True;break
   if kind=='normal':normal.append(bytes(raw[p:p+width]))
   dll=name(nrva);replacement=alias(dll)
   if not replacement:continue
   if len(replacement)>len(dll):raise ValueError('Alias exceeds original name extent')
   if not lookup:raise ValueError('Import lookup table absent')
   symbols=[];done=False
   for i in range(32768):
    value=struct.unpack_from('<Q',raw,off(lookup+i*8,8))[0]
    if not value:done=True;break
    if value>>63:symbol='#'+str(value&65535)
    else:
     if value>0xffffffff:raise ValueError('Bound-only import cannot be reconstructed')
     symbol=name(value+2)
    symbols.append(symbol)
   if not done:raise ValueError('Unbounded thunk list')
   key=replacement.lower()
   if key in proxies and proxies[key]['sourceDll'].lower()!=dll.lower():raise ValueError('Alias collision')
   spec=proxies.setdefault(key,{'sourceDll':dll,'proxyDll':replacement,'symbols':[]})
   spec['symbols']=sorted(set(spec['symbols'])|set(symbols))
   location=off(nrva,len(dll)+1)
   if preserve_sections:
    if kind=='normal':normal_aliases[len(normal)-1]=replacement
    else:delay_aliases.append(dll)
   else:
    encoded=replacement.encode()+b'\0'*(len(dll)+1-len(replacement))
    for byte in range(location,location+len(encoded)):
     if byte in occupied and occupied[byte]!=encoded[byte-location]:raise ValueError('Conflicting import-name writes')
     occupied[byte]=encoded[byte-location]
    result[location:location+len(encoded)]=encoded
    if kind=='normal':struct.pack_into('<I',result,p+4,0)
    else:struct.pack_into('<I',result,p+20,0);struct.pack_into('<I',result,p+28,0)
   changes.append({'kind':kind,'nameRva':nrva,'rawOffset':location,'original':dll,'replacement':replacement,'symbolCount':len(symbols),'originalNamePreserved':bool(preserve_sections)})
  if not ended:raise ValueError('Unterminated import descriptors')
 owned=None
 if preserve_sections:
  if not normal or not normal_aliases:raise ValueError('No normal import aliases to relocate')
  fa=u32(opt+36);sa=u32(opt+32)
  if not 512<=fa<=65536 or fa&(fa-1) or sa<4096 or sa<fa or sa&(sa-1):raise ValueError('Invalid PE alignments')
  def align(n,a):return (n+a-1)&~(a-1)
  last=max(layout,key=lambda s:s['rp']+s['rs'])
  if not last['flags']&0x40000000 or not last['flags']&0x20000000 or last['flags']&0x82000000 or last['vs']>last['rs']:raise ValueError('Owned import tail requires a final RX section without zero fill')
  if last['rp']+last['rs']!=len(raw) or last['va']+last['rs']!=max(s['va']+max(s['vs'],s['rs']) for s in layout):raise ValueError('Unknown overlay or non-final virtual section')
  at=align(len(raw),fa);rva=last['va']+at-last['rp']
  body=bytearray(b''.join(normal)+bytes(20));strings={}
  for index,replacement in sorted(normal_aliases.items()):
   if replacement not in strings:strings[replacement]=rva+len(body);body.extend(replacement.encode('ascii')+b'\0')
   struct.pack_into('<I',body,index*20+12,strings[replacement])
  # Rebind in the destination process; no captured/bound import addresses.
  for index in range(len(normal)):struct.pack_into('<I',body,index*20+4,0)
  end=align(at+len(body),fa);virtual=at-last['rp']+len(body);image=align(max(u32(opt+56),last['va']+virtual),sa)
  if max(end,virtual,image,rva+len(body))>0xffffffff:raise ValueError('Owned import metadata extent overflow')
  result.extend(bytes(end-len(result)));result[at:at+len(body)]=body
  struct.pack_into('<II',result,opt+112+8,rva,(len(normal)+1)*20)
  struct.pack_into('<I',result,last['header']+8,virtual)
  struct.pack_into('<I',result,last['header']+16,end-last['rp']);struct.pack_into('<I',result,opt+56,image)
  # Original delay descriptors are referenced directly by compiled helper
  # thunks. Keep them in place, including source names; the already registered
  # signed source-DLL aliases resolve their real LoadLibrary/GetProcAddress calls.
  for section in layout:
   start,size=section['rp'],section['rs']
   if size and result[start:start+size]!=raw[start:start+size]:raise ValueError('Original section bytes changed during import relocation')
  owned={'rva':rva,'bytes':len(body),'normalDescriptors':len(normal),'originalSectionBytesPreserved':True,'originalDelayDescriptorsPreserved':True,'lookupAndIatRvasRetained':True,'runtimeDelaySourceAliasesRequired':sorted(set(delay_aliases))}
 struct.pack_into('<I',result,opt+64,0)
 struct.pack_into('<2I',result,opt+112+4*8,0,0)
 struct.pack_into('<2I',result,opt+112+11*8,0,0)
 report={'inputSha256':expected.lower(),'outputSha256':hashlib.sha256(result).hexdigest(),'inputBytes':len(raw),'outputBytes':len(result),'inputUnchanged':True,'changes':changes,'proxies':list(proxies.values()),'ownedImportMetadata':owned,'signatureInvalidated':True,'fortniteExecuted':False,'xboxTested':False}
 return bytes(result),report

def redirect(source,destination,expected,preserve_sections=False):
 source=Path(source);destination=Path(destination)
 if source.resolve()==destination.resolve():raise ValueError('Original input must not be overwritten')
 result,report=redirect_data(source.read_bytes(),expected,preserve_sections)
 with destination.open('xb') as f:f.write(result)
 if hashlib.sha256(source.read_bytes()).hexdigest()!=expected.lower():raise RuntimeError('Original input changed')
 return report
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('destination');p.add_argument('--expected-sha256',required=True);p.add_argument('--report',required=True);p.add_argument('--preserve-original-sections',action='store_true');a=p.parse_args();r=Path(a.report)
 if r.exists() or r.resolve() in {Path(a.source).resolve(),Path(a.destination).resolve()}:raise ValueError('Report must be a new distinct file')
 report=redirect(a.source,a.destination,a.expected_sha256,a.preserve_original_sections)
 with r.open('x') as f:json.dump(report,f,indent=2)
 print(json.dumps({'redirectedDescriptors':len(report['changes']),'proxyDlls':len(report['proxies']),'outputSha256':report['outputSha256'],'fortniteExecuted':False}))
