"""Exact 6.21 DXGI request audit and historical rollback preparation helper.

The current native-HWND route calls audit(), preserving all three original
request instructions. The historical patch() remains for older preparation
recipes; current integrated packaging rejects its rewritten operands.
Neither operation removes a branch, HRESULT check or authentication check.
"""
from pathlib import Path
import hashlib
import pefile
SITES = [
 (0x1be4169, bytes.fromhex('c745cf02000000'), 3, 'initial windowed SwapChain Flags: ALLOW_MODE_SWITCH -> 0'),
 (0x1c01832, bytes.fromhex('c744242802000000'), 4, 'ResizeBuffers Flags: ALLOW_MODE_SWITCH -> 0'),
 (0x1c0183d, bytes.fromhex('ba01000000'), 1, 'ResizeBuffers BufferCount: 1 -> 0 (preserve actual flip-chain count)'),
]
def audit_bytes(before, expected_sha256):
 """Require unchanged original requests for the native-HWND route; no writes."""
 digest=hashlib.sha256(before).hexdigest()
 if digest!=expected_sha256:raise ValueError('Windowed request audit input SHA differs')
 p=pefile.PE(data=before,fast_load=True)
 try:
  if p.FILE_HEADER.Machine!=0x8664 or p.OPTIONAL_HEADER.Magic!=0x20b or not p.FILE_HEADER.Characteristics&0x2000 or p.OPTIONAL_HEADER.DATA_DIRECTORY[4].Size:raise ValueError('Prepared unsigned x64 private DLL required')
  verified=[]
  for rva,instruction,immediate,reason in SITES:
   section=p.get_section_by_rva(rva)
   if section is None or not section.Characteristics&0x20000000 or rva+len(instruction)>section.VirtualAddress+section.SizeOfRawData:raise ValueError('Original request outside executable section')
   if p.get_data(rva,len(instruction))!=instruction:raise ValueError('Original DXGI request was modified: '+hex(rva))
   verified.append({'rva':rva,'bytes':len(instruction)})
  return {'inputSha256':digest,'outputSha256':digest,'verifiedOriginalInstructions':verified,'changes':[],
          'instructionsChanged':False,'controlFlowChanged':False,'checksRemoved':False,
          'strategy':'real native HWND DXGI preserves original windowed mode-switch flag and buffer requests',
          'requiresNativeHwnd':True,'nativeExecutionTested':False,'xboxTested':False}
 finally:p.close()

def audit(path, expected_sha256):
 return audit_bytes(Path(path).read_bytes(),expected_sha256)

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
