"""Compile-time client pins from an exact verified private preparation only."""
import argparse,hashlib,json,re
from pathlib import Path
PIN='9d18fb5c5c43ddc21251d5ce9e026ca01f90d42921dd741973bd5536e8949478'
def write(prepared,destination):
 root=Path(prepared);plan=json.loads((root/'plan.json').read_text());m=plan.get('initializedClient')
 if plan.get('originalClientSha256')!=PIN or not isinstance(m,dict):raise ValueError('Verified normal initialization preparation required')
 if not m.get('originalInitializationExecuted') or not m.get('entryCallRetained') or m.get('checksPatched') is not False or m.get('historicalStackRepairApplied') is not False or plan.get('stackAddressRepair') is not None:raise ValueError('Original initialization/entry/check provenance differs')
 if (m.get('dir64Restored'),m.get('normalIatBindingsRestored'),m.get('delayIatBindingsRestored'),m.get('originalImageBytes'))!=(1018761,821,260,0x6314000):raise ValueError('Exact original loader inventory differs')
 main=[x for x in plan['files'] if x['mainImage']]
 if len(main)!=1 or main[0]['originalSha256']!=PIN:raise ValueError('One exact original main image required')
 record=main[0];rel=record['packageRelativePath']
 if rel!='6.21/FortniteGame/Binaries/Win64/Fortnite621.imports.private.dll':raise ValueError('Unexpected main image path')
 raw=(root/'GamePayload'/rel).read_bytes();sha=hashlib.sha256(raw).hexdigest()
 if len(raw)!=record['bytes'] or sha!=record['sha256'] or not re.fullmatch('[a-f0-9]{64}',sha):raise ValueError('Prepared client bytes/hash differ')
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
static const BYTE kPreparedClientSha[32]={%s};
static const ClientImageProtection kPreparedClientProtections[]={
%s
};
static constexpr unsigned kPreparedClientProtectionCount=%d;
'''%(len(raw),','.join('0x'+sha[i:i+2] for i in range(0,64,2)),'\n'.join(lines),len(lines))
 Path(destination).write_text(text);return {'clientSha256':sha,'bytes':len(raw),'measuredImageRegions':len(lines),'checksPatched':False,'historicalStackRepairApplied':False}
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('prepared');p.add_argument('destination');a=p.parse_args();print(json.dumps(write(a.prepared,a.destination)))
