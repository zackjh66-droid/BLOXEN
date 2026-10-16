from pathlib import Path
import zipfile,hashlib,json,shutil,io,sys
root=Path('/workspace/hollowwake');exp=root/'experimental-0.4.7';out=Path('/workspace/hollowwake-experiment-release-0.4.7');out.mkdir(exist_ok=True)
artifact=Path(sys.argv[1]);run=int(sys.argv[2]);commit=sys.argv[3]
bundle=out/'Hollowwake-Client-Experiment-0.4.7-x64';bundle.mkdir(exist_ok=True);logs=bundle/'WindowsVerification';logs.mkdir(exist_ok=True)
with zipfile.ZipFile(artifact) as z:
 test=json.loads(z.read('windows-test.json').decode('utf-8-sig'));assert test['result'].startswith('PASS') and not test['xboxTested'] and not test['fortniteExecuted'] and test['clockContract'] and test['allocatorContract'] and test['systemInfoContract']
 forwarders=json.loads(z.read('forwarder-checks/forwarder-test.json').decode('utf-8-sig'));assert forwarders['productionResolverUsed'] and forwarders['fixedBaseCollisionEstablished'] and forwarders['realMappedTargetReturn']==621 and forwarders['fixedExecutableRejected'] and not forwarders['downloadedCodeExecuted'] and forwarders['mappedClockProcessAttachExecuted'] and forwarders['clockAdvancesUsingRealOsUptime'] and forwarders['clockWrapSemanticsVerified'] and forwarders['mappedLegacyMallocProcessAttach'] and forwarders['allocatorFamilyChecksPassed'] and forwarders['mappedInitializerTablesExecuted'] and forwarders['mappedGetSystemInfoMatchedHost'] and forwarders['mappedGetVersionMatchedHost'] and forwarders['combinedStartupChainPassed'] and forwarders['moduleLookupChainPassed']
 fit=json.loads(z.read('menu-fit.json').decode('utf-8-sig'));assert fit['result']=='PASS' and fit['allSixButtonsVisibleWithoutScrolling']
 for n in z.namelist():
  if n.endswith('/Hollowwake_0.4.7.0_x64.appx'):dest=bundle/'Hollowwake_0.4.7.0_x64.appx'
  elif '/Dependencies/x64/' in n and n.endswith('.appx'):dest=bundle/'Dependencies/x64'/Path(n).name
  elif n=='HollowwakeExperiment.cer':dest=bundle/n
  elif n.endswith(('.log','.json','.txt','.png')) and len(n.split('/'))<=2:dest=logs/n
  else:continue
  assert '..' not in Path(n).parts;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(n))
 for name in ['AppxSignature.p7x','AppxBlockMap.xml','AppxManifest.xml']:
  with zipfile.ZipFile(bundle/'Hollowwake_0.4.7.0_x64.appx') as a:assert name in a.namelist()
for name in ['xbox-046-module-failure.json','ucrt-module-callsite.json','ucrt-entry-static.json','startup-coverage.json','dbghelp-full-startup-static.json','first-build.log','second-build.log','third-build.log','fourth-build.log','account-adapter-tests.log','final-build.log','first-build-failure.log','xbox-systeminfo-failure.json','systeminfo-callsite.txt','source-identity.json','sites-deployment.json','access-preservation.json','delivery-secret-scan.json','public-download-verification.json','initializer-callsite-audit.json','branding-alpha-check.json','layout-checks.json','sites-tests.log','sites-build.log','xbox-malloc-failure.json','tick-callsite-audit.json','xbox-tick-failure.json','crt-forwarder-audit.json','xbox-forwarder-failure.json','source-identity.json','boot-range-verified.json','native-dependency-check.json','website-readonly-check.json','site-mobile-home.png','site-mobile-link.png','site-desktop-home.png','site-desktop-project.png','site-desktop-downloads.png','deployed-browser-results.json','live-browser-results.json','website-mobile.png','website-desktop.png']:
 f=exp/'evidence'/name
 if f.exists():shutil.copy2(f,logs/f.name)
for name in ['credential-probes.json','credential-remediation.json']:
 shutil.copy2(root/'research-0.3.1/evidence'/name,logs/name)
for f in (exp/'docs').glob('*.md'):shutil.copy2(f,bundle/f.name)
summary={'version':'0.4.7.0','status':'Guest module lookup implementation and optional export handling; unfinished Fortnite port','sourceCommit':commit,'workflowRun':f'https://github.com/zackjh66-droid/BLOXEN/actions/runs/{run}','hostedWindows':test,'productionForwarderFixture':forwarders,'xbox':'0.4.7 unverified; user photographed0.4.6 stopping at LoadLibraryExW in ucrtbase, return RVA214922; original main entry/lobby not observed','fortniteLobby':'Not achieved or tested','productionServices':'Unchanged; existing accounts/admin/companion preserved','gameInputs':'No archive code or private game content redistributed; official public Season6 promotional background included under conditional fan-content terms; private console import required'}
(logs/'verification-summary.json').write_text(json.dumps(summary,indent=2))
notes='''# Hollowwake 0.4.7 — guest module lookup experiment

Development-signed x64 UWP real-client loading experiment. This is NOT a completed Fortnite port. The original lobby, character, locker and native Fortnite login remain unfinished. Physical Xbox 0.4.7 behavior is unverified. No game EXE/DLL/PAK is redistributed.

The user-reported 0.4.6 failure is LoadLibraryExW in original ucrtbase.dll, return RVA214922. Static inspection confirms an optional API resolver using SEARCH_SYSTEM32 and a table containing the fibers/synch contracts and kernel32. This update implements a connected module namespace: LoadLibrary A/W/Ex, GetProcAddress, GetModuleHandle A/W/Ex, real guest filenames, from-address identity and acquired-reference release. Public-API facade handles only expose real supported functions or existing bridges. Missing optional exports return null, never fake success pointers. Static required imports still fail fast when called without implementation.

Already mapped hash-pinned guest modules support real exports, ordinals and verified forwarding. Arbitrary paths, unsupported flags, unknown contracts and invalid handles are rejected. OS LoadLibrary/GetProcAddress never receive archive module paths or guest handles. Guest images remain internally pinned; new dynamic mapping and complete DLL detach/unload/reentrant initialization semantics remain unfinished. Facades are restricted compatibility interfaces, not native OS module identities or complete Windows DLLs. See MODULE-LOADING.md.

Focused owned-code tests exercise actual FLS values and destruction callbacks, dynamically called clock and critical-section APIs, mapped callable exports/ordinals/forwarders, A/W interfaces, filenames/truncation, references and failure cases. Existing combined constructor/allocator/TLS/stack-probe/SEH, signing, package-installation, no-scroll-menu, error-dialog and live project-account linking/restore/revocation checks are retained. All results are scoped in TEST-REPORT.md; downloaded game code is never executed in hosted CI.

Install as an UPDATE to Hollowwake.ClientExperiment without uninstalling. Keep existing boot inputs and protected sign-in. The bundle includes the APPX, three x64 framework dependencies, public certificate, instructions, source map and verification. The private signing key is not exported. Existing orange HW branding, original Epic Season6 promotional background and controller-focus layout are preserved. All six menu buttons remain visible; compact diagnostics keep the latest failure visible.

Keep using the same Hollowwake account. Native Fortnite authentication/HTTP transport and the disabled client621 prototype remain unfinished; project linking alone is not Fortnite login. No shared hardcoded game identity or Epic credentials are used. Matches remain unavailable. Full game content will be required for eventual rendering, but importing it cannot implement missing APIs.

Website/backend/accounts/private owner admin/original archive/rollback releases are unchanged. Public website: https://hollowwake.jihnjohn10.chatgpt.site . Its downloads page may still list an older release; use this delivered package. Recovery email remains unavailable. No next-boot/lobby guarantee or Xbox success is claimed.
'''
(bundle/'README.md').write_text(notes);(out/'RELEASE-NOTES.md').write_text(notes)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
manifest={str(f.relative_to(bundle)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(bundle.rglob('*')) if f.is_file()};(bundle/'SHA256SUMS.json').write_text(json.dumps(manifest,indent=2))
def makezip(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,f in files:z.write(f,name)
makezip(out/(bundle.name+'.zip'),[(f.relative_to(bundle).as_posix(),f) for f in sorted(bundle.rglob('*')) if f.is_file()])
makezip(out/'Hollowwake-Dependencies-x64.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted((bundle/'Dependencies').rglob('*.appx'))])
makezip(out/'Hollowwake-Verification-0.4.7.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted(logs.rglob('*')) if f.is_file()]+[(f.name,f) for f in sorted((exp/'docs').glob('*.md'))])
shutil.copy2(bundle/'Hollowwake_0.4.7.0_x64.appx',out/'Hollowwake_0.4.7.0_x64.appx');shutil.copy2(bundle/'HollowwakeExperiment.cer',out/'HollowwakeExperiment.cer')
# Preserve previously reviewed complete website/backend/admin/companion source;
# replace the runtime directories with this build's original implementation.
seed=Path('/workspace/hollowwake-experiment-release-0.4.5/Hollowwake-Source-0.4.5.zip');files={}
with zipfile.ZipFile(seed) as z:
 for n in z.namelist():
  if n.startswith(('hollowwake/client-runtime/','hollowwake/experimental-0.4.7/')):continue
  files[n]=z.read(n)
for directory in [root/'client-runtime',exp]:
 for f in sorted(directory.rglob('*')):
  rel=f.relative_to(directory)
  if not f.is_file() or {'evidence','bin','obj','artifacts','AppPackages','__pycache__'} & set(rel.parts):continue
  if f.suffix.lower() in {'.pfx','.key','.sqlite','.dll','.exe'}:continue
  files['hollowwake/'+f.relative_to(root).as_posix()]=f.read_bytes()
# Preserve exact current live Sites v10 website and authentication in the source delivery.
import subprocess
site=Path('/workspace/sites/hollowwake')
tracked=subprocess.run(['git','ls-files'],cwd=site,text=True,capture_output=True,check=True).stdout.splitlines()
for name in tracked:
 f=site/name
 if f.is_file():files['hollowwake/sites/'+name]=f.read_bytes()
for name in ['backend/core.mjs','backend/auth-options.mjs','web/app.js']:
 files['hollowwake/'+name]=(root/name).read_bytes()
files['hollowwake/EXPERIMENT-0.4.7-README.md']=notes.encode()
files['hollowwake/README.md']=b'# Latest experimental runtime: 0.4.7\n\nRead EXPERIMENT-0.4.7-README.md and experimental-0.4.7/docs before the preserved companion documentation below. The actual Fortnite lobby is unfinished.\n\n'+files['hollowwake/README.md']
with zipfile.ZipFile(out/'Hollowwake-Source-0.4.7.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,data in sorted(files.items()):
  assert '..' not in Path(n).parts and not n.startswith('/')
  assert Path(n).suffix.lower() not in {'.pfx','.key','.sqlite','.pak','.exe','.dll'}
  z.writestr(n,data)
art={f.name:{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(out.iterdir()) if f.is_file() and f.name not in {'ARTIFACTS.json','download-links.json'}};(out/'ARTIFACTS.json').write_text(json.dumps(art,indent=2));print(json.dumps(art,indent=2))
