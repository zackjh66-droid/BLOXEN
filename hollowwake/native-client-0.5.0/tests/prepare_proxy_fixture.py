"""Generate only Hollowwake's original proxies from public import metadata.
No game executable or DLL is read, copied or executed by this compilation test.
"""
from pathlib import Path
import argparse,json,sys
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'private-package'))
from generate_native_proxy import emit
p=argparse.ArgumentParser();p.add_argument('output');a=p.parse_args()
out=Path(a.output);out.mkdir(parents=True,exist_ok=False)
(out/'GamePayload').mkdir();sources=out/'ProxySources';sources.mkdir()
specs=[json.loads(f.read_text()) for f in sorted((root/'tests/client-proxy-specs').glob('*.json'))]
if len(specs)!=13:raise ValueError('Exact dependency proxy specification incomplete')
for spec in specs:emit(spec,sources/spec['proxyDll'])
(out/'plan.json').write_text(json.dumps({'proxyDlls':specs,'gameCodeIncluded':False},indent=2))
