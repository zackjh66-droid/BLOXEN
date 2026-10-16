"""Original bounded EXE-to-DLL experiment. Not a portability or DRM bypass.
Retains imports, code, relocations, TLS, unwind and prior exports. Native OS
loading, platform API compatibility and any game execution require separate proof.
"""
from pathlib import Path
import argparse,hashlib,json,struct

def convert(source,destination,expected):
 source=Path(source);destination=Path(destination)
 if source.resolve()==destination.resolve():raise ValueError('Input and output must differ')
 raw=source.read_bytes();digest=hashlib.sha256(raw).hexdigest()
 if digest.lower()!=expected.lower():raise ValueError('Input SHA-256 differs from pinned input')
 if not 1024<=len(raw)<=512*1024*1024:raise ValueError('Image size outside limits')
 def fit(pos,size,total=None):
  total=len(raw) if total is None else total
  if pos<0 or size<0 or pos>total or size>total-pos:raise ValueError('Out-of-bounds image data')
 def u16(pos):fit(pos,2);return struct.unpack_from('<H',raw,pos)[0]
 def u32(pos):fit(pos,4);return struct.unpack_from('<I',raw,pos)[0]
 def align(n,a):
  if not a or a&(a-1):raise ValueError('Invalid alignment')
  return (n+a-1)&~(a-1)
 if raw[:2]!=b'MZ':raise ValueError('Missing DOS signature')
 pe=u32(0x3c);fit(pe,264)
 if pe<64 or raw[pe:pe+4]!=b'PE\0\0' or u16(pe+4)!=0x8664:raise ValueError('Not an x64 PE')
 count=u16(pe+6);optsize=u16(pe+20);flags=u16(pe+22);opt=pe+24
 if optsize!=240 or u16(opt)!=0x20b or u32(opt+108)<16 or not 1<=count<96:raise ValueError('Unsupported PE32+ headers')
 if flags&0x2001:raise ValueError('Input is already a DLL or has stripped relocations')
 entry=u32(opt+16);imageSize=u32(opt+56);headers=u32(opt+60);sa=u32(opt+32);fa=u32(opt+36)
 if sa<fa or sa<4096 or fa<512 or fa>65536:raise ValueError('Unsupported image alignment')
 align(0,sa);align(0,fa)
 sections=[];table=opt+optsize
 for i in range(count):
  p=table+i*40;fit(p,40);vs,va,rs,rp=struct.unpack_from('<4I',raw,p+8);fit(rp,rs)
  if va<headers or va+max(vs,rs)>imageSize:raise ValueError('Invalid virtual section extent')
  for old in sections:
   if rs and old['rs'] and rp<old['rp']+old['rs'] and old['rp']<rp+rs:raise ValueError('Overlapping raw sections')
   if va<old['va']+max(old['vs'],old['rs']) and old['va']<va+max(vs,rs):raise ValueError('Overlapping virtual sections')
  sections.append({'va':va,'vs':vs,'rs':rs,'rp':rp,'flags':u32(p+36),'header':p})
 if not any(s['va']<=entry<s['va']+s['vs'] and s['flags']&0x20000000 for s in sections):raise ValueError('Original entry is not in executable section')
 newHeader=table+count*40;fit(newHeader,40)
 firstRaw=min(s['rp'] for s in sections if s['rs'])
 headerRoom=newHeader+40<=min(headers,firstRaw) and not any(raw[newHeader:newHeader+40])
 def offset(rva,n=1):
  if rva<headers:fit(rva,n);return rva
  for s in sections:
   if s['va']<=rva and n<=s['rs']-(rva-s['va']):return s['rp']+rva-s['va']
  raise ValueError('RVA has no bounded file-backed data')
 def string(rva):
  start=offset(rva);end=raw.find(b'\0',start,min(start+4096,len(raw)))
  if end<0:raise ValueError('Unterminated export string')
  value=raw[start:end];offset(rva,len(value)+1);value.decode('ascii');return value
 relocRva,relocSize=struct.unpack_from('<II',raw,opt+112+5*8)
 if not relocRva or not relocSize:raise ValueError('Relocation directory required')
 offset(relocRva,relocSize)
 exportRva,exportSize=struct.unpack_from('<II',raw,opt+112)
 funcs=[];names=[];base=1;forwarders={}
 if exportSize:
  if exportSize<40:raise ValueError('Invalid export directory')
  p=offset(exportRva,40);base,nf,nn,af,an,ao=struct.unpack_from('<6I',raw,p+16)
  if nf>65534 or nn>65534:raise ValueError('Export count outside limits')
  funcs=list(struct.unpack_from('<'+'I'*nf,raw,offset(af,nf*4))) if nf else []
  np=offset(an,nn*4) if nn else 0;op=offset(ao,nn*2) if nn else 0
  for i in range(nn):
   ordinal=u16(op+i*2)
   if ordinal>=nf:raise ValueError('Export name ordinal outside table')
   names.append((string(u32(np+i*4)),ordinal))
  for i,address in enumerate(funcs):
   if exportRva<=address<exportRva+exportSize:forwarders[i]=string(address)
   elif address:offset(address)
 symbol=b'StartHollowwakeClient'
 if any(n==symbol for n,_ in names) or base+len(funcs)>65535:raise ValueError('New export conflicts or overflows ordinals')
 originalExports=len(funcs);names.append((symbol,len(funcs)));funcs.append(entry);names.sort()
 rp=align(len(raw),fa)
 extend=None
 if headerRoom:
  va=align(max(imageSize,max(s['va']+max(s['vs'],s['rs']) for s in sections)),sa)
 else:
  # Some inspected images have nonzero data immediately after their section
  # table. Do not overwrite it or shift existing RVAs. Extend an already-readable,
  # non-writable/non-discardable final section only when every extent permits it.
  extend=max(sections,key=lambda x:x['rp']+x['rs'])
  if extend['vs']>extend['rs'] or not extend['flags']&0x40000000 or extend['flags']&0x82000000:raise ValueError('No safe final section for export data')
  end=extend['va']+max(extend['vs'],extend['rs'])
  if end<max(x['va']+max(x['vs'],x['rs']) for x in sections):raise ValueError('Final raw and virtual sections differ')
  overlay=len(raw)-(extend['rp']+extend['rs'])
  certOffset,certSize=struct.unpack_from('<II',raw,opt+112+4*8)
  if overlay and (overlay>1024*1024 or certOffset!=extend['rp']+extend['rs'] or certSize!=overlay):raise ValueError('Unknown overlay cannot be incorporated safely')
  va=extend['va']+rp-extend['rp']
  if va<end:raise ValueError('Appended exports overlap existing virtual data')
 body=bytearray(40+len(funcs)*4+len(names)*4+len(names)*2)
 eat=40;nt=eat+len(funcs)*4;ot=nt+len(names)*4
 def add(value):
  p=len(body);body.extend(value+b'\0');return va+p
 dllName=add(b'Hollowwake.private.dll')
 for i,forward in forwarders.items():funcs[i]=add(forward)
 for i,(name,ordinal) in enumerate(names):
  struct.pack_into('<I',body,nt+i*4,add(name));struct.pack_into('<H',body,ot+i*2,ordinal)
 for i,address in enumerate(funcs):struct.pack_into('<I',body,eat+i*4,address)
 struct.pack_into('<IIHH7I',body,0,0,0,0,0,dllName,base,len(funcs),len(names),va+eat,va+nt,va+ot)
 rs=align(len(body),fa);newImage=align(max(imageSize,va+len(body)),sa)
 if rp+rs>0xffffffff or newImage>0xffffffff:raise ValueError('PE field overflow')
 result=bytearray(raw);result.extend(b'\0'*(rp+rs-len(result)));result[rp:rp+len(body)]=body
 if headerRoom:struct.pack_into('<H',result,pe+6,count+1)
 struct.pack_into('<H',result,pe+22,flags|0x2000)
 struct.pack_into('<I',result,opt+16,0);struct.pack_into('<I',result,opt+56,newImage);struct.pack_into('<I',result,opt+64,0)
 struct.pack_into('<II',result,opt+112,va,len(body));struct.pack_into('<II',result,opt+112+4*8,0,0)
 if headerRoom:struct.pack_into('<8s6I2HI',result,newHeader,b'.hwload\0',len(body),va,rs,rp,0,0,0,0,0x40000040)
 else:
  struct.pack_into('<I',result,extend['header']+8,va-extend['va']+len(body))
  struct.pack_into('<I',result,extend['header']+16,rp+rs-extend['rp'])
 with destination.open('xb') as f:f.write(result)
 if hashlib.sha256(source.read_bytes()).hexdigest()!=digest:raise RuntimeError('Original input changed during conversion')
 return {'inputSha256':digest,'outputSha256':hashlib.sha256(result).hexdigest(),'inputBytes':len(raw),'outputBytes':len(result),'originalInputUnchanged':True,'exportStorage':'new read-only section' if headerRoom else 'extended existing final section; original protection retained','headerPaddingPreserved':headerRoom or result[newHeader:newHeader+40]==raw[newHeader:newHeader+40],'originalEntryRva':entry,'newDllEntryRva':0,'entryExport':symbol.decode(),'priorExportSlotsPreserved':originalExports,'priorExportNamesPreserved':len(names)-1,'forwardersPreserved':len(forwarders),'changes':['DLL characteristic','zero DLL entry; original EXE entry exposed for explicit invocation','appended export data preserving prior exports and forwarders','checksum and invalidated Authenticode directory cleared'],'nativeLoadTested':False,'fortniteExecuted':False,'xboxTested':False,'portableClientClaimed':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('source');p.add_argument('destination');p.add_argument('--expected-sha256',required=True);p.add_argument('--report',required=True);a=p.parse_args()
 result=convert(a.source,a.destination,a.expected_sha256);Path(a.report).write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
