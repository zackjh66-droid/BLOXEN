"""Exact 6.21 private port: request supported windowed composition buffers.

Changes only three DXGI request immediate operands. Does not modify a branch,
HRESULT check, framebuffer operation, import check or authentication behavior.
The runtime still rejects exclusive fullscreen, mode-switch and unsupported
buffer requests. Original game input remains unchanged.
"""
from pathlib import Path
import hashlib
import pefile
SITES = [
 (0x1be4169, bytes.fromhex('c745cf02000000'), 3, 'initial windowed SwapChain Flags: ALLOW_MODE_SWITCH -> 0'),
 (0x1c01832, bytes.fromhex('c744242802000000'), 4, 'ResizeBuffers Flags: ALLOW_MODE_SWITCH -> 0'),
 (0x1c0183d, bytes.fromhex('ba01000000'), 1, 'ResizeBuffers BufferCount: 1 -> 0 (preserve actual flip-chain count)'),
]
def patch(path, expected_sha256):
 path=Path(path);before=path.read_bytes()
 if hashlib.sha256(before).hexdigest()!=expected_sha256:raise ValueError('Windowed port input SHA differs')
 p=pefile.PE(data=before,fast_load=True)
 try:
  if p.FILE_HEADER.Machine!=0x8664 or p.OPTIONAL_HEADER.Magic!=0x20b or not p.FILE_HEADER.Characteristics&0x2000 or p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size:raise ValueError('Prepared unsigned x64 private DLL required')
  out=bytearray(before);changes=[]
  for rva,instruction,immediate,reason in SITES:
   section=next((s for s in p.sections if s.VirtualAddress<=rva and rva+len(instruction)<=s.VirtualAddress+s.SizeOfRawData),None)
   if section is None or not section.Characteristics&0x20000000:raise ValueError('Viewport request instruction outside code')
   offset=p.get_offset_from_rva(rva)
   if before[offset:offset+len(instruction)]!=instruction:raise ValueError('Exact 6.21 viewport request differs: '+hex(rva))
   out[offset+immediate:offset+immediate+4]=b'\0'*4
   changes.append({'rva':rva,'fileOffset':offset+immediate,'bytes':4,'before':int.from_bytes(instruction[immediate:immediate+4],'little'),'after':0,'reason':reason})
  changed={i for i,(a,b) in enumerate(zip(before,out)) if a!=b}
  expected={x['fileOffset'] for x in changes}
  if changed!=expected:raise ValueError('Unexpected instruction/control-flow changes')
  path.write_bytes(out)
  return {'inputSha256':expected_sha256,'outputSha256':hashlib.sha256(out).hexdigest(),'changes':changes,'windowedOnly':True,'controlFlowChanged':False,'checksRemoved':False,'fullscreenSupported':False,'scope':'Exact original 6.21 converted private client DXGI request operands; actual execution still required','executed':False}
 finally:p.close()
