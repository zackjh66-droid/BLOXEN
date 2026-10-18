"""Compare exact unchanged inventories; a resolver count is not API correctness."""
import argparse,json,re,hashlib,collections
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('output');a=p.parse_args();out=Path(a.output);root=Path(__file__).resolve().parents[1]
raw=(root/'tests/import-baseline.json').read_bytes();baseline=json.loads(raw)
known={e['name']:e for e in baseline['missingImports']}
header=(root/'Native/ClientDependencyImports.generated.h').read_text()
inventory={d+'!'+n for d,n in re.findall(r'\{"([^"]+)","([^"]+)"\}',header)}
assert len(inventory)==843,'Fixed redirected denominator changed'
after=set(x.strip() for x in (out/'native-imports-unresolved.txt').read_text(encoding='utf-8-sig').splitlines() if x.strip())
assert after<=inventory,'Report includes names outside unchanged inventory'
assert set(known)<=inventory and len(known)==208,'Original missing baseline changed'
assert after<=set(known),'New regression in a previously resolved binding'
main=lambda name:any(p.endswith('FortniteClient-Win64-Shipping.exe') for p in known[name]['importers'])
platform=json.loads((out/'native-platform-imports.json').read_text(encoding='utf-8-sig'))
expected=json.loads((root/'tests/platform-import-baseline.json').read_text())
assert platform['total']==838 and {r['name'] for r in platform['pairs']}=={r['name'] for r in expected['imports']},'Unredirected fixed inventory differs'
result={'scope':'Hosted Windows AppContainer resolution; behavior tested only by named owned fixtures; Xbox not tested','redirectedInventory':843,'baselineSha256':hashlib.sha256(raw).hexdigest(),'before':208,'after':len(after),'mainBefore':sum(map(main,known)),'mainAfter':sum(map(main,after)),'beforeByModule':dict(collections.Counter(n.split('!')[0] for n in known)),'afterByModule':dict(collections.Counter(n.split('!')[0] for n in after)),'newBindings':sorted(set(known)-after),'stillMissing':[known[n] for n in sorted(after)],'unredirected':{'total':838,'missing':platform['missing'],'normalMissing':platform['normalMissing'],'mainMissing':platform['mainMissing'],'exportsPresentAreNotBehaviorProof':True},'gameExecuted':False,'xboxTested':False}
checkpoint=json.loads((root/'tests/startup-checkpoint.json').read_text())
previous={r['name'] for r in checkpoint['missingImports']}
assert checkpoint['redirectedInventory']==843 and len(previous)==193 and previous<=set(known) and after<=previous
result['startupCheckpoint']={'sourceCommit':checkpoint['sourceCommit'],'before':len(previous),'mainBefore':sum(map(main,previous)),'after':len(after),'mainAfter':sum(map(main,after)),'newBindings':sorted(previous-after)}
result['baselineCanonicalSha256']=hashlib.sha256(json.dumps(baseline,sort_keys=True,separators=(',',':')).encode()).hexdigest()
(out/'import-comparison.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:result[k] for k in ['redirectedInventory','before','after','mainBefore','mainAfter','unredirected']}))
