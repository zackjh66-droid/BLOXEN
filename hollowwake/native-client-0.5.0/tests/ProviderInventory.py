"""Inventory newly compiled, licensed provider DLLs separately from the fixed
original Fortnite import baseline. Static PE evidence, never execution proof.
"""
import argparse,hashlib,json
from pathlib import Path
import pefile

def inventory(directory, names, output):
 root=Path(directory);records=[];pairs=set()
 for name in names:
  if Path(name).name!=name or not name.lower().endswith('.dll'):raise ValueError('Only explicit compiled provider basenames are accepted')
  file=root/name
  if not file.is_file() or file.is_symlink() or not 1024<=file.stat().st_size<=128*1024*1024:raise ValueError('Invalid provider file '+name)
  raw=file.read_bytes();pe=pefile.PE(data=raw,fast_load=True)
  if pe.FILE_HEADER.Machine!=0x8664 or pe.OPTIONAL_HEADER.Magic!=0x20b or not pe.OPTIONAL_HEADER.DllCharacteristics&0x1000:raise ValueError('Expected x64 AppContainer DLL '+name)
  pe.parse_data_directories(directories=[0,1,13]);imports=[]
  for attr,kind in [('DIRECTORY_ENTRY_IMPORT','normal'),('DIRECTORY_ENTRY_DELAY_IMPORT','delay')]:
   for module in getattr(pe,attr,[]):
    dll=module.dll.decode('ascii')
    if Path(dll).name!=dll or '/' in dll or '\\' in dll or ':' in dll:raise ValueError('Unexpected import module')
    for symbol in module.imports:
     function=symbol.name.decode('ascii') if symbol.name else '#'+str(symbol.ordinal)
     imports.append({'module':dll,'symbol':function,'kind':kind});pairs.add((dll.lower(),function))
  exports=[]
  for symbol in getattr(getattr(pe,'DIRECTORY_ENTRY_EXPORT',None),'symbols',[]):
   exports.append({'ordinal':symbol.ordinal,'name':symbol.name.decode('ascii') if symbol.name else None,'forwarder':symbol.forwarder.decode('ascii') if symbol.forwarder else None})
  records.append({'file':name,'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest(),'imports':imports,'exports':exports});pe.close()
 result={'scope':'Static additional provider dependency inventory; original 838-pair baseline remains unchanged. Export presence is not behavior or Xbox validation.','files':records,'additionalUniquePairs':len(pairs),'gameExecuted':False,'xboxTested':False}
 Path(output).write_text(json.dumps(result,indent=2));print('Inventoried '+str(len(records))+' actual compiled providers, '+str(len(pairs))+' additional dependency pairs; no execution claims')
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('directory');p.add_argument('output');p.add_argument('names',nargs='+');a=p.parse_args();inventory(a.directory,a.names,a.output)
