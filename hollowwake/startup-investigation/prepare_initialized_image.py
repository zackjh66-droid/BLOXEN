"""Private native experiment: normalize exact original module initialization.

Runs no code. Accepts only the completed capture of the unmodified original
client at its first call-return. Restores loader-owned bindings and normalizes
only recorded DIR64 relocations, requiring every captured relocation to match
its source plus actual loader delta. Never patches branches, skips an entry
call, substitutes APIs, or edits the PEB. A resulting file is an experimental
input, not proof of portable initialization; BSS is not in this capture.
Measured compiler import-table copies retain symbolic unbound values and
require current-process native-host rebinding before entry.
"""
import argparse,bisect,hashlib,json,struct
from pathlib import Path
import pefile
from compiler_imports import restore_tables,describe_restore_plan
PIN='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
MASK=(1<<64)-1

def prepare(source,capture,destination,report):
 source,capture,destination,report=map(Path,(source,capture,destination,report))
 if destination.exists() or report.exists() or source.resolve()==destination.resolve():raise ValueError('Fresh distinct outputs required')
 raw=source.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=PIN:raise ValueError('Exact source SHA required')
 m=json.loads((capture/'capture.json').read_text())
 if not(m['captureComplete'] and m['comparisonEqual'] and m['normalOriginalInitializationExecuted']) or m['clientChecksPatched'] or m['originalSha256']!=PIN or m['phase']!='original_entry_first_call_return':raise ValueError('Completed normal original initialization required')
 p=pefile.PE(data=raw);base=int(m['loadedBase']);pref=p.OPTIONAL_HEADER.ImageBase;delta=base-pref
 if pref!=int(m['preferredBase']) or len(m['sections'])!=len(p.sections):raise ValueError('Capture layout/base mismatch')
 headers=raw[:p.OPTIONAL_HEADER.SizeOfHeaders]
 if (capture/'source-headers.bin').read_bytes()!=headers:raise ValueError('Source header mismatch')
 # The original initializer changes this field independently of PE DIR64
 # relocations. Keep a loadable preferred-base file, but require the native
 # startup path to reproduce the measured transformation using ITS own base.
 field=p.DOS_HEADER.e_lfanew+24+24
 runtime_headers=(capture/'runtime-headers.bin').read_bytes()
 expected_headers=bytearray(headers);struct.pack_into('<Q',expected_headers,field,base)
 if runtime_headers!=expected_headers:raise ValueError('Unrecognized original runtime-header transformation')
 header_initialization={'field':'OptionalHeader.ImageBase','offset':field,'sourceValue':str(pref),'observedValue':str(base),'rule':'current-loaded-module-base','otherHeaderBytesUnchanged':True,'capturedAddressReplayed':False}
 data=bytearray(raw);starts=[];spans=[]
 for i,(s,c) in enumerate(zip(p.sections,m['sections'])):
  if (c['index'],c['rva'],c['rawOffset'],c['rawBytes'],c['virtualBytes'],c['characteristics'])!=(i,s.VirtualAddress,s.PointerToRawData,s.SizeOfRawData,s.Misc_VirtualSize,s.Characteristics):raise ValueError('Section receipt mismatch')
  b=(capture/f'section-{i:02}.bin').read_bytes()
  if len(b)!=s.SizeOfRawData:raise ValueError('Incomplete section')
  data[s.PointerToRawData:s.PointerToRawData+len(b)]=b;starts.append(s.VirtualAddress);spans.append((s.VirtualAddress,s.PointerToRawData,s.SizeOfRawData))
 def offset(rva,size):
  if rva<p.OPTIONAL_HEADER.SizeOfHeaders and rva+size<=p.OPTIONAL_HEADER.SizeOfHeaders:return rva
  at=bisect.bisect_right(starts,rva)-1
  if at<0:raise ValueError('RVA outside captured spans')
  va,off,n=spans[at]
  if rva+size>va+n:raise ValueError('RVA needs uncaptured BSS or spans')
  return off+rva-va
 relocations=set()
 for block in p.DIRECTORY_ENTRY_BASERELOC:
  for e in block.entries:
   if e.type==0:continue
   if e.type!=10 or e.rva in relocations:raise ValueError('Unsupported/duplicate relocation')
   off=offset(e.rva,8);before=struct.unpack_from('<Q',raw,off)[0];observed=struct.unpack_from('<Q',data,off)[0]
   if observed!=(before+delta)&MASK:raise ValueError(f'Runtime changed relocated field {e.rva:x}; no inferred correction allowed')
   struct.pack_into('<Q',data,off,before);relocations.add(e.rva)
 if field in relocations:raise ValueError('Header initialization already belongs to the loader relocation plan')
 header_initialization['outsideDir64']=True
 # The original initializer also copies 819 declared bindings into the compiler
 # IAT. Those addresses belong to the capture process and must be snapped again.
 compiler_groups=restore_tables(p,raw,data,0x3747000,0x3748b00,45,819,{'kernel32.dll':['AddVectoredExceptionHandler','RemoveVectoredExceptionHandler']},captured_base=base)
 restored=0
 for desc in getattr(p,'DIRECTORY_ENTRY_IMPORT',[]):
  # Include the zero terminator, which is part of the original thunk array.
  n=(len(desc.imports)+1)*8;off=offset(desc.struct.FirstThunk,n);data[off:off+n]=raw[off:off+n];restored+=len(desc.imports)
 delay=0
 for desc in getattr(p,'DIRECTORY_ENTRY_DELAY_IMPORT',[]):
  if desc.struct.grAttrs!=1:raise ValueError('Unsupported VA-based delay descriptor')
  n=(len(desc.imports)+1)*8;off=offset(desc.struct.pIAT,n);data[off:off+n]=raw[off:off+n];delay+=len(desc.imports)
  if desc.struct.phmod:
   rva=desc.struct.phmod
   section=next((s for s in p.sections if s.VirtualAddress<=rva and rva+8<=s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData)),None)
   if section is None:raise ValueError('Delay module handle outside image')
   if rva+8<=section.VirtualAddress+section.SizeOfRawData:
    off=offset(rva,8);data[off:off+8]=raw[off:off+8]
   elif rva<section.VirtualAddress+section.SizeOfRawData:raise ValueError('Delay handle straddles raw/BSS')
   # A wholly BSS-backed delay-loader module cache starts zero in the new OS
   # mapping. No captured runtime handle is copied into that uncaptured span.

 # Reject new image-absolute pointers in writable raw state that have no source
 # relocation. Do not "fix" them by scanning and subtracting an assumed base.
 for s in p.sections:
  if not s.Characteristics&0x80000000:continue
  for n in range(0,s.SizeOfRawData-7,8):
   rva=s.VirtualAddress+n;v=struct.unpack_from('<Q',data,s.PointerToRawData+n)[0]
   if base<=v<base+p.OPTIONAL_HEADER.SizeOfImage and rva not in relocations:raise ValueError(f'Unrecorded runtime image pointer {rva:x}')
 # Changed code cannot retain the original Authenticode claim. Remove only
 # the exact bounded certificate overlay and clear its directory/checksum.
 # Retaining orphan overlay bytes would correctly fail the existing converter.
 cert=p.OPTIONAL_HEADER.DATA_DIRECTORY[4];end=max(s.PointerToRawData+s.SizeOfRawData for s in p.sections)
 if cert.VirtualAddress!=end or cert.VirtualAddress+cert.Size!=len(raw):raise ValueError('Unexpected source certificate overlay')
 del data[end:]
 opt=p.DOS_HEADER.e_lfanew+24;struct.pack_into('<I',data,opt+64,0);struct.pack_into('<II',data,opt+112+4*8,0,0)
 if struct.unpack_from('<I',data,opt+16)[0]!=p.OPTIONAL_HEADER.AddressOfEntryPoint:raise AssertionError('Entry changed')
 compiler_imports=describe_restore_plan(compiler_groups)
 destination.write_bytes(data)
 receipt={'originalSha256':PIN,'outputSha256':hashlib.sha256(data).hexdigest(),'capturePhase':m['phase'],'originalInitializationExecuted':True,'headerInitialization':header_initialization,'dir64Restored':len(relocations),'normalIatBindingsRestored':restored,'delayIatBindingsRestored':delay,'entryCallRetained':True,'checksPatched':False,'uncapturedBss':True,'compilerImports':compiler_imports,'nativeExecutionTested':False,'xboxTested':False,'lobbyDemonstrated':False}
 report.write_text(json.dumps(receipt,indent=2)+'\n');return receipt
if __name__=='__main__':
 a=argparse.ArgumentParser();a.add_argument('source');a.add_argument('capture');a.add_argument('destination');a.add_argument('report');v=a.parse_args();print(json.dumps(prepare(v.source,v.capture,v.destination,v.report)))
