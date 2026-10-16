from pathlib import Path
import pefile,capstone,struct,bisect,json,collections
p=Path('/workspace/hollowwake/build-work/6.21/FortniteGame/Binaries/Win64/dbghelp.dll');q=pefile.PE(str(p));base=q.OPTIONAL_HEADER.ImageBase
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
imports={i.address:(d.dll.decode(),i.name.decode() if i.name else '#'+str(i.ordinal)) for d in q.DIRECTORY_ENTRY_IMPORT for i in d.imports}
ranges=sorted((base+e.struct.BeginAddress,base+e.struct.EndAddress) for e in q.DIRECTORY_ENTRY_EXCEPTION);starts=[x[0] for x in ranges]
def function(address):
 i=bisect.bisect_right(starts,address)-1
 if i>=0 and ranges[i][0]<=address<ranges[i][1]:return ranges[i]
 return address,address+16
ctors=[base+q.OPTIONAL_HEADER.AddressOfEntryPoint]+[x for x in struct.unpack('<2Q',q.get_data(0x1660,16)) if x]+[x for x in struct.unpack('<17Q',q.get_data(0x15d0,136)) if x]
report=[]
for ctor in ctors:
 todo=[(ctor,0)];seen=set();found={};indirect=[]
 while todo:
  addr,depth=todo.pop()
  if addr in seen or depth>64 or len(seen)>20000:continue
  seen.add(addr);lo,hi=function(addr)
  if lo!=addr and depth>0:lo=addr
  for inst in md.disasm(q.get_data(lo-base,hi-lo),lo):
   if inst.mnemonic not in ['call','jmp']:continue
   op=inst.operands[0]
   if op.type==capstone.x86.X86_OP_IMM:
    target=op.imm
    if base<=target<base+q.OPTIONAL_HEADER.SizeOfImage:todo.append((target,depth+1))
   elif op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
    slot=inst.address+inst.size+op.mem.disp
    if slot in imports:
     dll,name=imports[slot];found[dll+'!'+name]={'callsiteRva':hex(inst.address-base),'depth':depth}
    else:indirect.append(hex(inst.address-base))
 report.append({'initializerRva':hex(ctor-base),'visitedFunctions':len(seen),'reachableImports':found,'unresolvedIndirectCalls':indirect[:40]})
print(json.dumps(report,indent=2))
