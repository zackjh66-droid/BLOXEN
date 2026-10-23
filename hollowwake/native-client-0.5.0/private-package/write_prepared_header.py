"""Compile-time client pins from an exact verified private preparation only."""
import argparse,hashlib,json,re
from pathlib import Path
PIN='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
def write(prepared,destination):
 root=Path(prepared);plan=json.loads((root/'plan.json').read_text());m=plan.get('initializedClient')
 if plan.get('originalClientSha256')!=PIN or not isinstance(m,dict):raise ValueError('Verified normal initialization preparation required')
 if not m.get('originalInitializationExecuted') or not m.get('entryCallRetained') or m.get('checksPatched') is not False or m.get('historicalStackRepairApplied') is not False or plan.get('stackAddressRepair') is not None:raise ValueError('Original initialization/entry/check provenance differs')
 if (m.get('dir64Restored'),m.get('normalIatBindingsRestored'),m.get('delayIatBindingsRestored'),m.get('originalImageBytes'))!=(1018761,821,260,0x6314000):raise ValueError('Exact original loader inventory differs')
 copies=m.get('compilerImports',{});groups=copies.get('groups',[])
 if copies.get('nativeHostRebindingRequired') is not True or copies.get('tables')!=45 or copies.get('bindings')!=819 or copies.get('capturedImportAddressesRetained') is not False or copies.get('instructionsChanged') is not False or len(groups)!=45:raise ValueError('Complete native compiler import-table restoration required')
 copy_lines=[];seen=set()
 for g in groups:
  src,dst,n=g['sourceFirstThunkRva'],g['firstThunkRva'],g['count'];symbols=g['symbols'];provider=g['provider'];unbound=g['unboundValues']
  if type(src)!=int or type(dst)!=int or type(n)!=int or src%8 or dst%8 or not 0<n<=204 or min(src,dst)<0 or max(src,dst)+n*8>0x6314000 or len(symbols)!=n or len(unbound)!=n:raise ValueError('Invalid compiler import-copy extent')
  for i,symbol in enumerate(symbols):
   if dst+i*8 in seen:raise ValueError('Duplicate compiler import destination')
   seen.add(dst+i*8);copy_lines.append('{%d,%d,0x%xULL,%s,%s},'%(src+i*8,dst+i*8,int(unbound[i],16),json.dumps(provider),json.dumps(symbol)))
 if len(copy_lines)!=819:raise ValueError('Compiler import binding count differs')
 main=[x for x in plan['files'] if x['mainImage']]
 if len(main)!=1 or main[0]['originalSha256']!=PIN:raise ValueError('One exact original main image required')
 record=main[0];rel=record['packageRelativePath']
 if rel!='6.21/FortniteGame/Binaries/Win64/Fortnite621.imports.private.dll':raise ValueError('Unexpected main image path')
 raw=(root/'GamePayload'/rel).read_bytes();sha=hashlib.sha256(raw).hexdigest()
 if len(raw)!=record['bytes'] or sha!=record['sha256'] or not re.fullmatch('[a-f0-9]{64}',sha):raise ValueError('Prepared client bytes/hash differ')
 # Recheck the actual packaged instructions, not just a plan's claim. The native
 # HWND route must retain all three original DXGI request instructions.
 from patch_windowed_viewport import audit_bytes
 viewport=audit_bytes(raw,sha)
 if plan.get('windowedViewportPatch') is not None or plan.get('windowedViewportAudit')!=viewport:raise ValueError('Original DXGI request audit differs from actual prepared client')
 from audit_original_code import audit_bytes as audit_code
 canonical=(root/'InitializedMain.private.exe').read_bytes()
 if plan.get('originalCodeAudit')!=audit_code(canonical,raw,m['outputSha256']):raise ValueError('Actual original executable bytes differ from audit')
 header=m.get('headerInitialization',{})
 if header.get('field')!='OptionalHeader.ImageBase' or header.get('offset')!=0x200 or header.get('sourceValue')!='5368709120' or header.get('rule')!='current-loaded-module-base' or header.get('otherHeaderBytesUnchanged') is not True or header.get('capturedAddressReplayed') is not False or header.get('outsideDir64') is not True:raise ValueError('Measured original header initialization required')
 if not header.get('observedValue','').isdigit() or int(header['observedValue'])<=0x140000000:raise ValueError('Missing original ASLR header observation')
 imports=plan.get('mainOwnedImportMetadata',{})
 if not all(imports.get(k) is True for k in ('originalSectionBytesPreserved','originalDelayDescriptorsPreserved','lookupAndIatRvasRetained')) or imports.get('normalDescriptors')!=45 or imports.get('runtimeDelaySourceAliasesRequired')!=[]:raise ValueError('Original main import placement policy differs')
 regions=m.get('protections',[]);prior=0;lines=[]
 if not regions or len(regions)>4096:raise ValueError('Measured page permission inventory required')
 for r in regions:
  v,n,p=(r[x] for x in ('rva','bytes','protect'))
  if any(type(x)!=int for x in (v,n,p)) or v<prior or v%4096 or n<=0 or n%4096 or v>=0x6314000 or n>0x6314000-v or p not in (1,2,4,8,16,32,64,128):raise ValueError('Invalid measured image protection')
  lines.append('{%d,%d,%d},'%(v,n,p));prior=v+n
 text='''#pragma once
#include "ClientImageStartup.h"
// Generated from verified private normal initialization, not a lobby claim.
static constexpr bool kPreparedClientAvailable=true;
static constexpr SIZE_T kPreparedClientBytes=%d;
static constexpr SIZE_T kPreparedOriginalImageBytes=0x6314000;
static constexpr DWORD kPreparedImageBaseField=0x200;
static constexpr ULONGLONG kPreparedPreferredImageBase=0x140000000ULL;
static const BYTE kPreparedClientSha[32]={%s};
static const ClientImageProtection kPreparedClientProtections[]={
%s
};
static constexpr unsigned kPreparedClientProtectionCount=%d;
'''%(len(raw),','.join('0x'+sha[i:i+2] for i in range(0,64,2)),'\n'.join(lines),len(lines))
 text+='\nstatic const ClientCompilerImportCopy kPreparedCompilerImports[]={\n'+'\n'.join(copy_lines)+'\n};\nstatic constexpr unsigned kPreparedCompilerImportCount=819;\n'
 Path(destination).write_text(text);return {'clientSha256':sha,'bytes':len(raw),'measuredImageRegions':len(lines),'checksPatched':False,'historicalStackRepairApplied':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('prepared');p.add_argument('destination');a=p.parse_args();print(json.dumps(write(a.prepared,a.destination)))
