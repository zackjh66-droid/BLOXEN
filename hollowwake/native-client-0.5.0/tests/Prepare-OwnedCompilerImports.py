"""Owned DLL fixture only; deliberately invalid captured pointer must never execute."""
import sys,struct,json
from pathlib import Path
import pefile
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'startup-investigation'))
from compiler_imports import restore_tables,append_directory
source,destination=map(Path,sys.argv[1:]);raw=source.read_bytes();data=bytearray(raw);p=pefile.PE(data=raw,fast_load=True);p.parse_data_directories(directories=[0,1])
export=next(x for x in p.DIRECTORY_ENTRY_EXPORT.symbols if x.name==b'OwnedCopiedImportSlots');d=next(d for d in p.DIRECTORY_ENTRY_IMPORT if d.dll.lower()==b'startupprovider.dll');assert len(d.imports)==1
poison=0x123456780000;struct.pack_into('<Q',data,p.get_offset_from_rva(d.struct.FirstThunk),poison);struct.pack_into('<Q',data,p.get_offset_from_rva(export.address),poison)
groups=restore_tables(p,raw,data,export.address,export.address+16,1,1)
# The declared loader table is restored from its original symbolic file bytes.
for desc in p.DIRECTORY_ENTRY_IMPORT:
 n=(len(desc.imports)+1)*8;offset=p.get_offset_from_rva(desc.struct.FirstThunk);data[offset:offset+n]=raw[offset:offset+n]
report=append_directory(p,raw,data,groups);assert poison.to_bytes(8,'little') not in data;assert source.resolve()!=destination.resolve() and not destination.exists();destination.write_bytes(data)
q=pefile.PE(data=data,fast_load=True);q.parse_data_directories(directories=[1]);copies=[x for x in q.DIRECTORY_ENTRY_IMPORT if x.dll.lower()==b'startupprovider.dll'];assert len(copies)==2 and all(len(x.imports)==1 for x in copies)
print(json.dumps({'scope':'Owned fixture metadata only; next native executable must prove OS-loaded addresses and both calls','tables':report['tables'],'bindings':report['bindings'],'poisonNotPersisted':poison.to_bytes(8,'little') not in data,'metadataStorage':report['metadataStorage']}))
