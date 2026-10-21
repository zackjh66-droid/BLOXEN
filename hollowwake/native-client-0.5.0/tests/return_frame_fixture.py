"""Owned complete x64 frame fixture. No original game instructions are loaded.
Uses the production repair generator, then actually returns through the saved
frame into a computed continuation. No OS APIs are emulated by this fixture.
"""
import argparse,importlib.util,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('repair',root/'private-package/patch_stack_address.py');repair=importlib.util.module_from_spec(spec);spec.loader.exec_module(repair)

def build(mode='full'):
 code=bytearray();fixups={'code':[],'stack':[],'output':[]}
 def emit(b):code.extend(b)
 def ptr(op,key):emit(bytes.fromhex(op));fixups[key].append(len(code));emit(bytes(8))
 # Snapshot actual incoming nonvolatile registers and original host stack.
 ptr('49ba','output');emit(bytes.fromhex('49896a1049895a184989722049896228'))
 emit(bytes.fromhex('4989e3'));ptr('48bc','stack')
 call=len(code);emit(bytes(5))
 # This is the ordinary CALL return. A correct dispatch chooses the computed
 # continuation below instead. Mark an accidental ordinary return separately.
 emit(bytes.fromhex('4c89dcb801000000c3'))
 continuation=len(code)
 emit(bytes.fromhex('9c58'));ptr('49ba','output')
 emit(bytes.fromhex('4989420849896a3049895a38498972404c89dc49896248b86d020000418902c3'))
 while len(code)%16:emit(b'\x90')
 helper=len(code);code[call:call+5]=b'\xe8'+struct.pack('<i',helper-call-5)
 # Full frame matching the observed epilogue: saved RBP at +80, return at +88.
 emit(bytes.fromhex('554881ec800000004889e548895d7048897578'))
 # Independent hardware flag reference for the original ECX arithmetic.
 emit(repair.POINTER_CALCULATION);emit(bytes.fromhex('9c58'));ptr('49ba','output');emit(bytes.fromhex('49894250'))
 # Compute the dispatch address from a 32-bit RVA and 64-bit module base.
 emit(b'\xb8'+struct.pack('<I',continuation));ptr('49ba','code');emit(bytes.fromhex('4c01d0'))
 site=len(code)
 if mode=='original':emit(repair.ORIGINAL)
 else:emit(bytes(len(repair.ORIGINAL)))
 emit(repair.EPILOGUE)
 while len(code)%16:emit(b'\x90')
 trampoline=len(code)
 if mode=='full':body=repair.trampoline(repair.SITE_RVA+trampoline-site)
 elif mode=='partial':
  body=bytes.fromhex('8945508b45508d8d9000000083e904488d8d8c0000008901')
  body+=repair.branch(trampoline+len(body),site+len(repair.ORIGINAL))
 elif mode=='original':body=b''
 else:raise ValueError(mode)
 emit(body)
 if mode!='original':code[site:site+len(repair.ORIGINAL)]=repair.branch(site,trampoline)+b'\x90'*(len(repair.ORIGINAL)-5)
 return bytes(code),fixups

def relocate(code,fixups,code_base,stack_top,output):
 data=bytearray(code)
 for key,value in [('code',code_base),('stack',stack_top),('output',output)]:
  for position in fixups[key]:struct.pack_into('<Q',data,position,value)
 return bytes(data)

def header(path):
 code,fixups=build();lines=['// Generated from the actual production repair generator; owned test only.','#pragma once',
 'static const unsigned char kOwnedReturnCode[]={' + ','.join(str(x) for x in code)+'};']
 for key in fixups:lines.append('static const size_t k'+key.title()+'Fixups[]={'+','.join(str(x) for x in fixups[key])+'};')
 Path(path).write_text('\n'.join(lines)+'\n')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--header',required=True);a=p.parse_args();header(a.header)
