from pathlib import Path
import zipfile,json,hashlib,os,re
root=Path('/workspace/hollowwake');current=root/'client-runtime';out=Path('/workspace/hollowwake-runtime-release-0.3.2');out.mkdir(exist_ok=True)
source=out/'Hollowwake-Source-0.3.2.zip'
with zipfile.ZipFile('/workspace/hollowwake-research-release-0.3.1/Hollowwake-Research-Source.zip') as prior,zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for i in prior.infolist():z.writestr(i.filename,prior.read(i))
 for f in sorted(current.rglob('*')):
  if f.is_file() and not {'evidence','__pycache__'}&set(f.relative_to(current).parts):z.write(f,'hollowwake/client-runtime/'+f.relative_to(current).as_posix())
 z.writestr('hollowwake/CLIENT-RUNTIME-NOTES.md','New client-runtime/ contains original tested TLS donor and compiled CoreWindow factory/import bridge. Full Fortnite-compatible runtime, client integration and lobby remain unfinished. No new APPX exists. Read client-runtime/docs/STATUS.md and BUILD-RESULTS.md. Existing website/companion source is retained from0.3.1.\n')
with zipfile.ZipFile(out/'Hollowwake-Runtime-Changes-0.3.2.zip','w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
 for f in sorted((current/'evidence/hosted-delivered').iterdir()):
  if f.is_file():z.write(f,'windows/'+f.name)
 for f in sorted((current/'evidence').glob('*')):
  if f.is_file() and f.suffix in {'.log','.json'}:z.write(f,'evidence/'+f.name)
 for f in sorted((current/'docs').glob('*.md')):z.write(f,'docs/'+f.name)
 z.writestr('README.txt','Implementation and Windows test artifacts only. NOT an Xbox-installable APPX, NOT Fortnite. DLLs are original compiled helper/fixture code. Do not try to install these DLLs using Device Portal. No new certificate/private signing key exists. The matching source and hosted workflow are supplied separately.\n')
for f in (current/'docs').glob('*.md'):(out/f.name).write_bytes(f.read_bytes())
manifest={f.name:{'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()} for f in out.iterdir() if f.is_file() and f.name!='ARTIFACTS.json'};(out/'ARTIFACTS.json').write_text(json.dumps(manifest,indent=2)+'\n')
values=[os.environ.get('GH_TOKEN','')]
for f in [Path('/workspace/sites/hollowwake/.dev.vars'),Path('/workspace/sites/hollowwake-control/.dev.vars'),root/'.env']:
 if f.exists():
  for line in f.read_text().splitlines():
   if '=' in line and re.search('SECRET|TOKEN|PASSWORD|PRIVATE',line.split('=',1)[0],re.I):values.append(line.split('=',1)[1].strip().strip('\"\''))
values=[v.encode() for v in values if len(v)>12];count=0
for f in out.glob('*.zip'):
 with zipfile.ZipFile(f) as z:
  for i in z.infolist():
   data=z.read(i);assert not any(v in data for v in values),'Known secret detected';assert not i.filename.lower().endswith(('.pfx','.key','.sqlite','.pak','.utoc','.ucas'))
   assert not re.search(rb'-----BEGIN (?:RSA |EC )?PRIVATE KEY-----|gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,}',data),'Credential pattern';count+=1
print(json.dumps({'artifacts':manifest,'filesScanned':count,'knownLocalSecretsChecked':len(values),'secretMatches':0},indent=2))
