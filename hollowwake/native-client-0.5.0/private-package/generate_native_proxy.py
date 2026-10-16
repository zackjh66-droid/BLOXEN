"""Generate original x64 ABI-preserving static jump exports. No copied binaries.
Unresolved functions cause DLL initialization failure, not fabricated API success.
"""
from pathlib import Path
import argparse,json,re

def emit(spec,output):
 out=Path(output);out.mkdir(parents=True,exist_ok=False)
 dll=spec['sourceDll'];proxy=spec['proxyDll'];symbols=spec['symbols'];data=set(spec.get('dataSymbols',[]))
 if not data.issubset(set(symbols)):raise ValueError('Data export not in symbol inventory')
 if not re.fullmatch(r'[A-Za-z0-9_.-]{1,200}\.dll',dll,re.I) or not re.fullmatch(r'[A-Za-z0-9_-]{1,100}\.dll',proxy,re.I):raise ValueError('Invalid DLL basename')
 if not symbols or len(set(symbols))!=len(symbols) or len(symbols)>4096:raise ValueError('Invalid export list')
 asm=['option casemap:none','EXTERN HWProxyTargets:QWORD','.code'];exports=['LIBRARY '+proxy,'EXPORTS']
 for i,symbol in enumerate(symbols):
  if not isinstance(symbol,str) or not 1<=len(symbol)<=4096 or any(ord(c)<33 or ord(c)>126 or c in '\\"' for c in symbol):raise ValueError('Invalid export name')
  if symbol in data:
   if not re.fullmatch(r'[A-Za-z0-9_?@$]+',symbol):raise ValueError('Data forwarder requires a named export')
   exports.append(' '+symbol+'='+dll[:-4]+'.'+symbol+' DATA')
   continue
  label='HWProxyFn'+str(i)
  asm+=['PUBLIC '+label,label+' PROC',' jmp QWORD PTR [HWProxyTargets+'+str(i*8)+']',label+' ENDP']
  if symbol.startswith('#'):
   if not symbol[1:].isdigit() or not 1<=int(symbol[1:])<=65535:raise ValueError('Invalid ordinal')
   exports.append(' HWOrdinal'+symbol[1:]+'='+label+' @'+symbol[1:]+' NONAME')
  else:
   if not re.fullmatch(r'[A-Za-z0-9_?@$]+',symbol):raise ValueError('Unsupported export-name syntax')
   exports.append(' '+symbol+'='+label)
 asm.append('END')
 cells=((len(symbols)*8+4095)//4096)*512
 cpp='''// Original generated proxy: only real resolved operations can be exported.
#include <windows.h>
extern "C" __declspec(dllimport) void* __cdecl HWResolvePackagedImport(const char*,const char*);
#pragma section(".hwptr",read,write)
extern "C" {__declspec(allocate(".hwptr")) __declspec(align(4096)) void* HWProxyTargets[CELLS]={};}
static const char* symbols[]={SYMBOLS};
static const bool dataExports[]={DATAFLAGS};
extern "C" BOOL WINAPI DllMain(HINSTANCE,DWORD reason,LPVOID){
 if(reason==DLL_PROCESS_ATTACH){
  for(unsigned i=0;i<COUNT;++i){if(dataExports[i])continue;HWProxyTargets[i]=HWResolvePackagedImport(DLL,symbols[i]);if(!HWProxyTargets[i])return FALSE;}
  DWORD previous=0;if(!VirtualProtectFromApp(HWProxyTargets,sizeof(HWProxyTargets),PAGE_READONLY,&previous))return FALSE;
 }return TRUE;
}
'''.replace('DATAFLAGS',','.join('true' if x in data else 'false' for x in symbols)).replace('CELLS',str(cells)).replace('SYMBOLS',','.join(json.dumps(x) for x in symbols)).replace('COUNT',str(len(symbols))).replace('DLL,',json.dumps(dll)+',')
 (out/'proxy.cpp').write_text(cpp);(out/'proxy.asm').write_text('\n'.join(asm)+'\n');(out/'proxy.def').write_text('\n'.join(exports)+'\n')
 (out/'proxy.json').write_text(json.dumps(spec,indent=2)+'\n')
 return out
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('spec');p.add_argument('output');a=p.parse_args();emit(json.loads(Path(a.spec).read_text(encoding='utf-8-sig')),a.output)
