"""Restore measured copies of declared import tables through the native PE loader.

No function is emulated. Each complete, zero-terminated copy must match the
captured declared table in order. The file retains symbolic unbound values; the native host replays the measured
copy phase from its current declared IAT before any original entry executes.
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

def describe_restore_plan(groups):
 return {'tables':len(groups),'bindings':sum(g['count'] for g in groups),'groups':[{**{k:v for k,v in g.items() if k not in ('lookups','nameRva')},'unboundValues':[hex(x) for x in g['lookups']]} for g in groups],'nativeHostRebindingRequired':True,'capturedImportAddressesRetained':False,'instructionsChanged':False,'scope':'Measured compiler-table copies require current-process native-host import rebinding before entry; this file is not a standalone executable'}
