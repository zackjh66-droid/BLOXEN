"""Build an own-source overlay; never include proprietary binaries or keys."""
from pathlib import Path
import zipfile,hashlib,json
r=Path(__file__).resolve().parents[1];out=r/'native-route-delivery/Hollowwake-Native-Route-Source.zip';files=[]
allowed={'.cpp','.h','.cs','.xaml','.csproj','.vcxproj','.appxmanifest','.ps1','.py','.json','.yml','.md','.txt','.pem','.def','.asm'}
for folder in ['native-client-0.5.0','client-runtime/graphics','runtime-native-route']:
 for p in (r/folder).rglob('*'):
  if not p.is_file() or p.suffix not in allowed or set(p.relative_to(r).parts)&{'evidence','research','__pycache__','bin','obj','tools','artifacts'} or '.private.' in p.name:continue
  if p.suffix=='.pem' and p.name!='delivery-recipient-public.pem':continue
  files.append(p)
files.extend(p for p in (r/'native-route-delivery').iterdir() if p.suffix in {'.md','.json','.log','.py'} and p.name!='source-hash.json')
for name in ['import-comparison.json','native-window-probe.json','preflight-results.json','native-platform-imports.json','native-thread-results.json','swapchain-contract-results.json','native-launch.log','ldap-provider-test.log','web-provider-test.log','network-test.log','packaged-content-plan.json','workflow-outcomes.json','private-package-audit.json']:
 p=r/'neo-route-comparison/evidence/final-private-run'/name
 if p.is_file():files.append(p)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
 for p in sorted(files):z.write(p,p.relative_to(r))
meta={'file':out.name,'bytes':out.stat().st_size,'sha256':hashlib.sha256(out.read_bytes()).hexdigest(),'files':len(files),'containsGameFiles':False,'containsPrivateKeys':False,'usage':'Source overlay for existing repository; preserve existing assets and backend','testedCodeCommit':'d463aaa1966cdc89e5c48ced6e8bf36e44c2c0ee','privateBuildRun':39095870275}
(r/'native-route-delivery/source-hash.json').write_text(json.dumps(meta,indent=2));print(json.dumps(meta))
