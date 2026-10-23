"""Recheck original executable raw sections against pinned initialized input.
Header/import placement changes are separate; this is not execution proof.
"""
import hashlib,struct

def sections(raw):
 if raw[:2]!=b'MZ' or len(raw)<64:raise ValueError('Missing DOS header')
 pe=struct.unpack_from('<I',raw,60)[0]
 if pe<64 or pe+264>len(raw) or raw[pe:pe+4]!=b'PE\0\0':raise ValueError('Invalid PE extent')
 machine,count=struct.unpack_from('<HH',raw,pe+4)
 if machine!=0x8664 or not 1<=count<=96 or struct.unpack_from('<H',raw,pe+20)[0]!=240:raise ValueError('Unexpected PE image')
 records=[]
 for i in range(count):
  at=pe+264+i*40
  if at+40>len(raw):raise ValueError('Truncated section table')
  vs,va,size,offset=struct.unpack_from('<4I',raw,at+8);flags=struct.unpack_from('<I',raw,at+36)[0]
  if offset>len(raw) or size>len(raw)-offset:raise ValueError('Truncated section data')
  records.append((va,vs,size,offset,flags))
 return records

def audit_bytes(original,prepared,expected):
 if hashlib.sha256(original).hexdigest()!=expected:raise ValueError('Initialized source hash differs')
 target=sections(prepared);result=[]
 for va,vs,size,offset,flags in sections(original):
  if not flags&0x20000000:continue
  matches=[s for s in target if s[0]==va]
  if len(matches)!=1:raise ValueError('Original executable section moved or missing')
  _,target_vs,target_size,target_offset,target_flags=matches[0]
  if target_vs<vs or target_size<size or target_flags!=flags:raise ValueError('Executable section extent/protection changed')
  before=original[offset:offset+size];after=prepared[target_offset:target_offset+size]
  if before!=after:raise ValueError('Original executable section bytes changed: '+hex(va))
  result.append({'rva':va,'bytes':size,'sha256':hashlib.sha256(before).hexdigest()})
 if not result:raise ValueError('No executable sections audited')
 return {'initializedInputSha256':expected,'originalExecutableSections':result,'originalExecutableBytesPreserved':True,'executionEstablished':False}
