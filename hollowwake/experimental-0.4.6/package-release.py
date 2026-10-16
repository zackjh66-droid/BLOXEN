from pathlib import Path
import zipfile,hashlib,json,shutil,io,sys
root=Path('/workspace/hollowwake');exp=root/'experimental-0.4.6';out=Path('/workspace/hollowwake-experiment-release-0.4.6');out.mkdir(exist_ok=True)
artifact=Path(sys.argv[1]);run=int(sys.argv[2]);commit=sys.argv[3]
bundle=out/'Hollowwake-Client-Experiment-0.4.6-x64';bundle.mkdir(exist_ok=True);logs=bundle/'WindowsVerification';logs.mkdir(exist_ok=True)
with zipfile.ZipFile(artifact) as z:
 test=json.loads(z.read('windows-test.json').decode('utf-8-sig'));assert test['result'].startswith('PASS') and not test['xboxTested'] and not test['fortniteExecuted'] and test['clockContract'] and test['allocatorContract'] and test['systemInfoContract']
 forwarders=json.loads(z.read('forwarder-checks/forwarder-test.json').decode('utf-8-sig'));assert forwarders['productionResolverUsed'] and forwarders['fixedBaseCollisionEstablished'] and forwarders['realMappedTargetReturn']==621 and forwarders['fixedExecutableRejected'] and not forwarders['downloadedCodeExecuted'] and forwarders['mappedClockProcessAttachExecuted'] and forwarders['clockAdvancesUsingRealOsUptime'] and forwarders['clockWrapSemanticsVerified'] and forwarders['mappedLegacyMallocProcessAttach'] and forwarders['allocatorFamilyChecksPassed'] and forwarders['mappedInitializerTablesExecuted'] and forwarders['mappedGetSystemInfoMatchedHost'] and forwarders['mappedGetVersionMatchedHost'] and forwarders['combinedStartupChainPassed']
 fit=json.loads(z.read('menu-fit.json').decode('utf-8-sig'));assert fit['result']=='PASS' and fit['allSixButtonsVisibleWithoutScrolling']
 for n in z.namelist():
  if n.endswith('/Hollowwake_0.4.6.0_x64.appx'):dest=bundle/'Hollowwake_0.4.6.0_x64.appx'
  elif '/Dependencies/x64/' in n and n.endswith('.appx'):dest=bundle/'Dependencies/x64'/Path(n).name
  elif n=='HollowwakeExperiment.cer':dest=bundle/n
  elif n.endswith(('.log','.json','.txt','.png')) and len(n.split('/'))<=2:dest=logs/n
  else:continue
  assert '..' not in Path(n).parts;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(n))
 for name in ['AppxSignature.p7x','AppxBlockMap.xml','AppxManifest.xml']:
  with zipfile.ZipFile(bundle/'Hollowwake_0.4.6.0_x64.appx') as a:assert name in a.namelist()
for name in ['startup-coverage.json','dbghelp-full-startup-static.json','first-build.log','second-build.log','third-build.log','account-adapter-tests.log','final-build.log','first-build-failure.log','xbox-systeminfo-failure.json','systeminfo-callsite.txt','source-identity.json','sites-deployment.json','access-preservation.json','delivery-secret-scan.json','public-download-verification.json','initializer-callsite-audit.json','branding-alpha-check.json','layout-checks.json','sites-tests.log','sites-build.log','xbox-malloc-failure.json','tick-callsite-audit.json','xbox-tick-failure.json','crt-forwarder-audit.json','xbox-forwarder-failure.json','source-identity.json','boot-range-verified.json','native-dependency-check.json','website-readonly-check.json','site-mobile-home.png','site-mobile-link.png','site-desktop-home.png','site-desktop-project.png','site-desktop-downloads.png','deployed-browser-results.json','live-browser-results.json','website-mobile.png','website-desktop.png']:
 f=exp/'evidence'/name
 if f.exists():shutil.copy2(f,logs/f.name)
for name in ['credential-probes.json','credential-remediation.json']:
 shutil.copy2(root/'research-0.3.1/evidence'/name,logs/name)
for f in (exp/'docs').glob('*.md'):shutil.copy2(f,bundle/f.name)
summary={'version':'0.4.6.0','status':'Connected legacy startup implementation and compact failure diagnostics; unfinished Fortnite port','sourceCommit':commit,'workflowRun':f'https://github.com/zackjh66-droid/BLOXEN/actions/runs/{run}','hostedWindows':test,'productionForwarderFixture':forwarders,'xbox':'0.4.6 update unverified; user photographed0.4.5 stopping at legacy scalar operator new in dbghelp, return RVA1221957; original main entry/lobby not observed','fortniteLobby':'Not achieved or tested','productionServices':'Unchanged; existing accounts/admin/companion preserved','gameInputs':'No archive code or private game content redistributed; official public Season6 promotional background included under conditional fan-content terms; private console import required'}
(logs/'verification-summary.json').write_text(json.dumps(summary,indent=2))
notes='''# Hollowwake0.4.6 — connected dependency-startup experiment

Development-signed x64 UWP real-client loading experiment, NOT a completed Fortnite port. Original lobby/character/locker and native game authentication are unfinished.0.4.6 has not been tested on physical Xbox. It uses the existing privately imported inspected6.21 files; no game executable or PAK content is included.

The user-reported0.4.5 failure is msvcrt!??2@YAPEAX_K@Z (legacy scalar operator new), dbghelp return RVA1221957. A static trace now covers the exact dependency entry point, its C initializer and all16 non-null C++ initializer roots. The newly supported direct constructor import set includes scalar/array new/delete, recursive CRT locking, callback-table growth/registration and critical-section initialization. Version/private-heap/TLS/wait/stack-probe and structured-exception support is tested in an original combined mapped-DLL fixture. Detailed coverage and remaining unimplemented entry/error/cleanup calls are recorded; this is static evidence, not proof that original dbghelp finished on Xbox.

Legacy allocations share the same maintained allocator/free family. Private DLL exit tables keep their original ownership. Process exit callbacks are drained LIFO on a normal main return. Old C++ exception/new-handler behavior on allocation failure is not implemented: it explicitly fails fast with a named log. No old FILE/RTTI/C++ exception layouts are aliased to modern incompatible structures. Stable x64 SEH handling is tested separately from C++ exceptions.

The combined fixture exercises16 scalar constructions/destructions, raw array allocation/deletion,64 callback registrations with reverse-order execution, recursive locking and8000 concurrent updates, real private-heap allocation/growth, TLS, sleeping,24KB stack probing and a caught structured exception through registered unwind information. Earlier initializer/clock/system-information/allocator/forwarder/TLS checks are preserved. Hosted testing never executes downloaded game code.

Diagnostics now open with the latest named error and last stage, without the oversized checklist that hid the failure. Short Full log, Enable checks and Close buttons remain accessible. A real persisted own-fixture fail-fast error is used for the UI test and screenshot. This Windows diagnostic screenshot is NOT a new Xbox or Fortnite boot result. All six menu buttons remain visible without scrolling; Xbox cursor emulation stays disabled for directional focus.

Hosted signed-package installation/activation, signature verification, existing native contracts, fail-fast logging/startup recovery, real website approval/polling, protected automatic sign-in after restart, logout and revocation are checked. Source/verification evidence is included. Physical Xbox execution and controller behavior require the user test.

Install this as an UPDATE to Hollowwake.ClientExperiment without uninstalling. Keep your imported boot files and protected session. Use the same Hollowwake account; it is intended to own the game profile and username. Native game transport/login and the disabled client621 prototype are still unfinished. Website sign-in is not Fortnite login. No shared hardcoded game identity or Epic credentials are used. Matches are unavailable.

Website/backend/accounts/private owner admin/original archive/previous releases are unchanged. Website download page may still list the earlier release; use this delivered0.4.6 link. Public website: https://hollowwake.jihnjohn10.chatgpt.site . Recovery email remains unavailable.

Public signing certificate and all three x64 framework dependencies are included. The private key was non-exportable on the ephemeral hosted runner and is not distributed. Original public Epic Season6 promotional background and licensed Anton lettering are preserved under the recorded usage terms. No original archive binaries are uploaded to the public repository.

Remaining requirements include legacy C++ EH/CRT compatibility, complete Windows module/thread/window/graphics behavior, and native client-compatible account/backend integration. Full archive import cannot implement missing APIs. No lobby success or next-boot guarantee is made.
'''
(bundle/'README.md').write_text(notes);(out/'RELEASE-NOTES.md').write_text(notes)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
manifest={str(f.relative_to(bundle)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(bundle.rglob('*')) if f.is_file()};(bundle/'SHA256SUMS.json').write_text(json.dumps(manifest,indent=2))
def makezip(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,f in files:z.write(f,name)
makezip(out/(bundle.name+'.zip'),[(f.relative_to(bundle).as_posix(),f) for f in sorted(bundle.rglob('*')) if f.is_file()])
makezip(out/'Hollowwake-Dependencies-x64.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted((bundle/'Dependencies').rglob('*.appx'))])
makezip(out/'Hollowwake-Verification-0.4.6.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted(logs.rglob('*')) if f.is_file()]+[(f.name,f) for f in sorted((exp/'docs').glob('*.md'))])
shutil.copy2(bundle/'Hollowwake_0.4.6.0_x64.appx',out/'Hollowwake_0.4.6.0_x64.appx');shutil.copy2(bundle/'HollowwakeExperiment.cer',out/'HollowwakeExperiment.cer')
# Preserve previously reviewed complete website/backend/admin/companion source;
# replace the runtime directories with this build's original implementation.
seed=Path('/workspace/hollowwake-experiment-release-0.4.5/Hollowwake-Source-0.4.5.zip');files={}
with zipfile.ZipFile(seed) as z:
 for n in z.namelist():
  if n.startswith(('hollowwake/client-runtime/','hollowwake/experimental-0.4.6/')):continue
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
files['hollowwake/EXPERIMENT-0.4.6-README.md']=notes.encode()
files['hollowwake/README.md']=b'# Latest experimental runtime: 0.4.6\n\nRead EXPERIMENT-0.4.6-README.md and experimental-0.4.6/docs before the preserved companion documentation below. The actual Fortnite lobby is unfinished.\n\n'+files['hollowwake/README.md']
with zipfile.ZipFile(out/'Hollowwake-Source-0.4.6.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,data in sorted(files.items()):
  assert '..' not in Path(n).parts and not n.startswith('/')
  assert Path(n).suffix.lower() not in {'.pfx','.key','.sqlite','.pak','.exe','.dll'}
  z.writestr(n,data)
art={f.name:{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(out.iterdir()) if f.is_file() and f.name not in {'ARTIFACTS.json','download-links.json'}};(out/'ARTIFACTS.json').write_text(json.dumps(art,indent=2));print(json.dumps(art,indent=2))
