from pathlib import Path
import zipfile,hashlib,json,shutil,io,sys
root=Path('/workspace/hollowwake');exp=root/'experimental-0.4';out=Path('/workspace/hollowwake-experiment-release-0.4.0');out.mkdir(exist_ok=True)
artifact=Path(sys.argv[1]);run=int(sys.argv[2]);commit=sys.argv[3]
bundle=out/'Hollowwake-Client-Experiment-0.4.0-x64';bundle.mkdir(exist_ok=True);logs=bundle/'WindowsVerification';logs.mkdir(exist_ok=True)
with zipfile.ZipFile(artifact) as z:
 test=json.loads(z.read('windows-test.json').decode('utf-8-sig'));assert test['result'].startswith('PASS') and not test['xboxTested'] and not test['fortniteExecuted']
 for n in z.namelist():
  if n.endswith('/Hollowwake_0.4.0.0_x64.appx'):dest=bundle/'Hollowwake_0.4.0.0_x64.appx'
  elif '/Dependencies/x64/' in n and n.endswith('.appx'):dest=bundle/'Dependencies/x64'/Path(n).name
  elif n=='HollowwakeExperiment.cer':dest=bundle/n
  elif n.endswith(('.log','.json','.txt')) and len(n.split('/'))<=2:dest=logs/n
  else:continue
  assert '..' not in Path(n).parts;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(n))
 for name in ['AppxSignature.p7x','AppxBlockMap.xml','AppxManifest.xml']:
  with zipfile.ZipFile(bundle/'Hollowwake_0.4.0.0_x64.appx') as a:assert name in a.namelist()
for name in ['source-identity.json','boot-range-verified.json','native-dependency-check.json','website-readonly-check.json','website-mobile.png','website-desktop.png']:
 f=exp/'evidence'/name
 if f.exists():shutil.copy2(f,logs/f.name)
for name in ['credential-probes.json','credential-remediation.json']:
 shutil.copy2(root/'research-0.3.1/evidence'/name,logs/name)
for f in (exp/'docs').glob('*.md'):shutil.copy2(f,bundle/f.name)
summary={'version':'0.4.0.0','status':'Experimental real-client initialization attempt; unfinished Fortnite port','sourceCommit':commit,'workflowRun':f'https://github.com/zackjh66-droid/BLOXEN/actions/runs/{run}','hostedWindows':test,'xbox':'Unverified; no physical Xbox result for this build','fortniteLobby':'Not achieved or tested','productionServices':'Unchanged; existing accounts/admin/companion preserved','gameInputs':'No Fortnite code/assets redistributed; private console import required'}
(logs/'verification-summary.json').write_text(json.dumps(summary,indent=2))
notes='''# Hollowwake 0.4 — client initialization experiment

This download is a development-signed x64 UWP runtime that can import the actual inspected 6.21 client privately onto the console and attempt its original initialization. It is not a completed Fortnite port. No original lobby, character, locker or shop success is claimed. Xbox execution is unverified.

The old working companion, account service, owner-only administration and rollback packages are preserved. This package has a separate identity and uses your existing Hollowwake account with a new device approval.

Changes: original x64 image/dependency mapping, TLS donors, unwind registration, supported window/message/controller hooks, real DXGI/D3D11 composition bridge, explicit original entry attempt, fail-fast missing-API logs, post-crash log access, pinned minimum import, optional full import, and orange HW/focus branding. There are no placeholder successful implementations of missing native APIs.

Hosted Windows checks passed on the exact signed package: installation/activation, executable-memory fixture, distinct packaged TLS slots, window/message and graphics contracts, actual missing-API fail-fast log/process exit, startup recovery, live own-account linking, protected restart sign-in, logout and revocation. Original mapped-TLS fixtures also passed, including modified-file rejection. No downloaded game code ran in hosted CI. See WindowsVerification for scope and logs.

131 import offsets/sizes/hashes were statically checked against the unchanged source ZIP; a source HTTPS byte range was checked independently. Native CLR/C++ imports were checked against provided x64 VCLibs. These do not establish console execution. No physical controller or in-game behavior has been verified.

Install using INSTALL.md. Start with Show launch log, then the small Import boot files test, then Attempt client boot. Return the last EXECUTE/STOP/CRASH/CALLED_UNSUPPORTED record. The full archive is not necessary just to reproduce an unsupported call.

Remaining requirements are in LIMITATIONS.md: extensive Windows ABI/lifecycle coverage, private native HTTP/authentication handoff, complete version-compatible backend/presence and validated assets. The production client-protocol adapter remains disabled. No matches are provided.

Signing: public certificate only; private key was non-exportable on the ephemeral build runner. Self-signed development certificate, not Microsoft/Epic publisher endorsement. Game content is not included.

Live website: https://hollowwake.jihnjohn10.chatgpt.site
Admin remains owner-authenticated at the existing private address; it is deliberately omitted from public release metadata.
'''
(bundle/'README.md').write_text(notes);(out/'RELEASE-NOTES.md').write_text(notes)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
manifest={str(f.relative_to(bundle)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(bundle.rglob('*')) if f.is_file()};(bundle/'SHA256SUMS.json').write_text(json.dumps(manifest,indent=2))
def makezip(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,f in files:z.write(f,name)
makezip(out/(bundle.name+'.zip'),[(f.relative_to(bundle).as_posix(),f) for f in sorted(bundle.rglob('*')) if f.is_file()])
makezip(out/'Hollowwake-Dependencies-x64.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted((bundle/'Dependencies').rglob('*.appx'))])
makezip(out/'Hollowwake-Verification-0.4.0.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted(logs.rglob('*')) if f.is_file()]+[(f.name,f) for f in sorted((exp/'docs').glob('*.md'))])
shutil.copy2(bundle/'Hollowwake_0.4.0.0_x64.appx',out/'Hollowwake_0.4.0.0_x64.appx');shutil.copy2(bundle/'HollowwakeExperiment.cer',out/'HollowwakeExperiment.cer')
# Preserve previously reviewed complete website/backend/admin/companion source;
# replace the runtime directories with this build's original implementation.
seed=Path('/workspace/hollowwake-runtime-release-0.3.2/Hollowwake-Source-0.3.2.zip');files={}
with zipfile.ZipFile(seed) as z:
 for n in z.namelist():
  if n.startswith(('hollowwake/client-runtime/','hollowwake/experimental-0.4/')):continue
  files[n]=z.read(n)
for directory in [root/'client-runtime',exp]:
 for f in sorted(directory.rglob('*')):
  rel=f.relative_to(directory)
  if not f.is_file() or {'evidence','bin','obj','artifacts','AppPackages','__pycache__'} & set(rel.parts):continue
  if f.suffix.lower() in {'.pfx','.key','.sqlite','.dll','.exe'}:continue
  files['hollowwake/'+f.relative_to(root).as_posix()]=f.read_bytes()
files['hollowwake/EXPERIMENT-0.4-README.md']=notes.encode()
files['hollowwake/README.md']=b'# Latest experimental runtime: 0.4\n\nRead EXPERIMENT-0.4-README.md and experimental-0.4/docs before the preserved companion documentation below. The actual Fortnite lobby is unfinished.\n\n'+files['hollowwake/README.md']
with zipfile.ZipFile(out/'Hollowwake-Source-0.4.0.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,data in sorted(files.items()):
  assert '..' not in Path(n).parts and not n.startswith('/')
  assert Path(n).suffix.lower() not in {'.pfx','.key','.sqlite','.pak','.exe','.dll'}
  z.writestr(n,data)
art={f.name:{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(out.iterdir()) if f.is_file() and f.name!='ARTIFACTS.json'};(out/'ARTIFACTS.json').write_text(json.dumps(art,indent=2));print(json.dumps(art,indent=2))
