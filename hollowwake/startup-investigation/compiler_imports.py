"""Restore measured copies of declared import tables through the native PE loader.

No function is emulated. Each complete, zero-terminated copy must match the
captured declared table in order. New lookup arrays retain the declared symbolic
bindings; the OS loader resolves both tables in the destination process.
"""
import struct

def align(value, unit):
 if unit<=0 or unit&(unit-1):raise ValueError('Unsupported PE alignment')
 return (value+unit-1)&~(unit-1)

def restore_tables(pe, raw, data, start, end, expected_tables, expected_bindings, allowed_extra=None, captured_base=None):
 allowed_extra=allowed_extra or {};base=pe.OPTIONAL_HEADER.ImageBase if captured_base is None else captured_base;size=pe.OPTIONAL_HEADER.SizeOfImage
 if start%8 or end%8 or not 0<start<end<size:raise ValueError('Invalid copied IAT span')
 section=next((s for s in pe.sections if s.VirtualAddress<=start and end<=s.VirtualAddress+s.SizeOfRawData),None)
 if section is None or section.Characteristics&0xa0000000 or not section.Characteristics&0x40000000:raise ValueError('Copied IAT must occupy bounded read-only non-code data')
 def offset(rva,count=8):
  off=pe.get_offset_from_rva(rva)
  if off<0 or off+count>len(data):raise ValueError('Copied import extent outside image')
  return off
 def observed(rva):return struct.unpack_from('<Q',data,offset(rva))[0]
 descriptors=[]
 for d in pe.DIRECTORY_ENTRY_IMPORT:
  if not d.struct.OriginalFirstThunk or not d.struct.FirstThunk:raise ValueError('Declared symbolic import lookup is required')
  first=d.struct.FirstThunk;count=len(d.imports)
  if first<end and start<first+(count+1)*8:raise ValueError('Copied and declared IAT overlap')
  symbols=[x.name.decode('ascii') if x.name else '#'+str(x.ordinal) for x in d.imports]
  values=[observed(first+i*8) for i in range(count)]
  lookup=list(struct.unpack_from('<'+'Q'*count,raw,offset(d.struct.OriginalFirstThunk,count*8)))
  descriptors.append({'provider':d.dll.decode('ascii'),'nameRva':d.struct.Name,'sourceFirstThunkRva':first,'symbols':symbols,'values':values,'lookups':lookup})
 groups=[];cursor=start;seen=set();bindings=0
 while cursor<end:
  first=cursor;values=[]
  while cursor<end and observed(cursor):
   value=observed(cursor)
   if base<=value<base+size:raise ValueError('Unresolved copied import points inside preferred image')
   values.append(value);cursor+=8
  if not values or cursor>=end:raise ValueError('Incomplete copied import table at '+hex(first))
  cursor+=8;count=len(values);matches=[]
  for d in descriptors:
   extra=d['symbols'][count:]
   if len(d['values'])<count or d['values'][:count]!=values:continue
   if extra and extra!=allowed_extra.get(d['provider'].lower()):continue
   matches.append(d)
  if len(matches)!=1:raise ValueError('Copied import table has no unique ordered declared match at '+hex(first))
  d=matches[0];key=d['provider'].lower()
  if key in seen:raise ValueError('Repeated copied module table: '+key)
  seen.add(key);lookups=d['lookups'][:count]
  if any(not x for x in lookups):raise ValueError('Empty declared lookup in copied table')
  struct.pack_into('<'+'Q'*count,data,offset(first,count*8),*lookups)
  groups.append({'provider':d['provider'],'firstThunkRva':first,'sourceFirstThunkRva':d['sourceFirstThunkRva'],'count':count,'symbols':d['symbols'][:count],'nameRva':d['nameRva'],'lookups':lookups});bindings+=count
 if len(groups)!=expected_tables or bindings!=expected_bindings:raise ValueError('Exact copied import inventory differs')
 return groups

def append_directory(pe, raw, data, groups):
 # Caller has already removed the original bounded certificate overlay.
 count=pe.FILE_HEADER.NumberOfSections;opt=pe.DOS_HEADER.e_lfanew+24;table=opt+pe.FILE_HEADER.SizeOfOptionalHeader;new_header=table+count*40
 first_raw=min(s.PointerToRawData for s in pe.sections if s.SizeOfRawData)
 header_room=count<95 and new_header+40<=min(first_raw,pe.OPTIONAL_HEADER.SizeOfHeaders) and not any(data[new_header:new_header+40])
 extend=None
 if not header_room:
  extend=max(pe.sections,key=lambda x:x.PointerToRawData+x.SizeOfRawData)
  if extend.Misc_VirtualSize>extend.SizeOfRawData or extend.Characteristics&0x82000000 or extend.Characteristics&0x60000000!=0x60000000 or extend.VirtualAddress+extend.SizeOfRawData<max(x.VirtualAddress+max(x.Misc_VirtualSize,x.SizeOfRawData) for x in pe.sections):raise ValueError('No safe existing immutable final section for import metadata')
 last=max(s.PointerToRawData+s.SizeOfRawData for s in pe.sections)
 if len(data)!=last:raise ValueError('Unknown overlay before import restoration')
 sa=pe.OPTIONAL_HEADER.SectionAlignment;fa=pe.OPTIONAL_HEADER.FileAlignment;va=align(pe.OPTIONAL_HEADER.SizeOfImage,sa);rp=align(len(data),fa)
 if extend is not None:va=extend.VirtualAddress+rp-extend.PointerToRawData
 old=pe.OPTIONAL_HEADER.DATA_DIRECTORY[1];old_off=pe.get_offset_from_rva(old.VirtualAddress)
 directory_bytes=(len(pe.DIRECTORY_ENTRY_IMPORT)+len(groups)+1)*20;body=bytearray(align(directory_bytes,8))
 for i,d in enumerate(pe.DIRECTORY_ENTRY_IMPORT):
  entry=bytearray(raw[old_off+i*20:old_off+(i+1)*20]);struct.pack_into('<I',entry,4,0);body[i*20:(i+1)*20]=entry
 for i,g in enumerate(groups):
  lookup_rva=va+len(body);body.extend(struct.pack('<'+'Q'*(g['count']+1),*(g['lookups']+[0])))
  at=(len(pe.DIRECTORY_ENTRY_IMPORT)+i)*20;struct.pack_into('<IIIII',body,at,lookup_rva,0,0,g['nameRva'],g['firstThunkRva'])
 rs=align(len(body),fa);data.extend(b'\0'*(rp+rs-len(data)));data[rp:rp+len(body)]=body
 if extend is None:
  header=bytearray(40);header[:8]=b'.hwiat\0\0';struct.pack_into('<IIII',header,8,len(body),va,rs,rp);struct.pack_into('<I',header,36,0x40000040);data[new_header:new_header+40]=header
  struct.pack_into('<H',data,pe.DOS_HEADER.e_lfanew+6,count+1);struct.pack_into('<I',data,opt+8,pe.OPTIONAL_HEADER.SizeOfInitializedData+rs)
 else:
  header=extend.get_file_offset();struct.pack_into('<I',data,header+8,va+len(body)-extend.VirtualAddress);struct.pack_into('<I',data,header+16,rp+rs-extend.PointerToRawData);struct.pack_into('<I',data,opt+4,pe.OPTIONAL_HEADER.SizeOfCode+rs)
 struct.pack_into('<I',data,opt+56,align(va+len(body),sa));struct.pack_into('<II',data,opt+112+8,va,directory_bytes)
 # Bound-import metadata is invalid for an expanded symbolic import directory.
 struct.pack_into('<II',data,opt+112+11*8,0,0)
 return {'tables':len(groups),'bindings':sum(g['count'] for g in groups),'groups':[{k:v for k,v in g.items() if k not in ('lookups','nameRva')} for g in groups],'nativeLoaderRequired':True,'capturedImportAddressesRetained':False,'instructionsChanged':False,'importDirectoryRva':va,'sectionBytes':rs,'metadataStorage':'new-readonly-section' if extend is None else 'extended-original-immutable-rx-section','scope':'Complete measured compiler-table copies rebound from declared symbolic imports by the destination OS loader; execution unverified'}
