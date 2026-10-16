# Static bounded x64 control-flow trace. Never executes the inspected image.
import pefile,capstone,json,collections,sys
from pathlib import Path
path=Path(sys.argv[1]);q=pefile.PE(str(path));base=q.OPTIONAL_HEADER.ImageBase
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_64);md.detail=True
imports={i.address:d.dll.decode()+'!'+(i.name.decode() if i.name else '#'+str(i.ordinal)) for d in q.DIRECTORY_ENTRY_IMPORT for i in d.imports}
sections=[(base+s.VirtualAddress,base+s.VirtualAddress+max(s.Misc_VirtualSize,s.SizeOfRawData)) for s in q.sections if s.Characteristics&0x20000000]
def executable(addr):return any(lo<=addr<hi for lo,hi in sections)
todo=[(base+q.OPTIONAL_HEADER.AddressOfEntryPoint,0)];seen=set();found={};indirect=[];bounded=False
while todo:
 addr,depth=todo.pop()
 if depth>64 or len(seen)>50000:bounded=True;continue
 if not executable(addr) or addr in seen:continue
 for x in md.disasm(q.get_data(addr-base,512),addr):
  if x.address in seen:break
  seen.add(x.address)
  if x.mnemonic in ['ret','retf','int3','ud2']:break
  if x.mnemonic=='call' or x.mnemonic.startswith('j'):
   op=x.operands[0]
   if op.type==capstone.x86.X86_OP_IMM:
    if executable(op.imm):todo.append((op.imm,depth+(x.mnemonic=='call')))
   elif op.type==capstone.x86.X86_OP_MEM and op.mem.base==capstone.x86.X86_REG_RIP:
    slot=x.address+x.size+op.mem.disp
    if slot in imports:found[imports[slot]]=hex(x.address-base)
    else:indirect.append(hex(x.address-base))
   else:indirect.append(hex(x.address-base))
   if x.mnemonic=='jmp':break
 else:
  # Continue a large basic block at the end of this bounded decode window.
  if 'x' in locals():todo.append((x.address+x.size,depth))
print(json.dumps({'scope':'Static bounded basic-block trace from exact UCRT entry; follows conditional branches and direct calls; includes error/detach branches. Indirect calls and initialization tables remain unresolved; this is not execution evidence.','instructionsVisited':len(seen),'budgetReached':bounded,'reachableImports':dict(sorted(found.items())),'unresolvedIndirectCalls':sorted(set(indirect))},indent=2))
