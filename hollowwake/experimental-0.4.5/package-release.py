from pathlib import Path
import zipfile,hashlib,json,shutil,io,sys
root=Path('/workspace/hollowwake');exp=root/'experimental-0.4.5';out=Path('/workspace/hollowwake-experiment-release-0.4.5');out.mkdir(exist_ok=True)
artifact=Path(sys.argv[1]);run=int(sys.argv[2]);commit=sys.argv[3]
bundle=out/'Hollowwake-Client-Experiment-0.4.5-x64';bundle.mkdir(exist_ok=True);logs=bundle/'WindowsVerification';logs.mkdir(exist_ok=True)
with zipfile.ZipFile(artifact) as z:
 test=json.loads(z.read('windows-test.json').decode('utf-8-sig'));assert test['result'].startswith('PASS') and not test['xboxTested'] and not test['fortniteExecuted'] and test['clockContract'] and test['allocatorContract'] and test['systemInfoContract']
 forwarders=json.loads(z.read('forwarder-checks/forwarder-test.json').decode('utf-8-sig'));assert forwarders['productionResolverUsed'] and forwarders['fixedBaseCollisionEstablished'] and forwarders['realMappedTargetReturn']==621 and forwarders['fixedExecutableRejected'] and not forwarders['downloadedCodeExecuted'] and forwarders['mappedClockProcessAttachExecuted'] and forwarders['clockAdvancesUsingRealOsUptime'] and forwarders['clockWrapSemanticsVerified'] and forwarders['mappedLegacyMallocProcessAttach'] and forwarders['allocatorFamilyChecksPassed'] and forwarders['mappedInitializerTablesExecuted'] and forwarders['mappedGetSystemInfoMatchedHost'] and forwarders['mappedGetVersionMatchedHost']
 fit=json.loads(z.read('menu-fit.json').decode('utf-8-sig'));assert fit['result']=='PASS' and fit['allSixButtonsVisibleWithoutScrolling']
 for n in z.namelist():
  if n.endswith('/Hollowwake_0.4.5.0_x64.appx'):dest=bundle/'Hollowwake_0.4.5.0_x64.appx'
  elif '/Dependencies/x64/' in n and n.endswith('.appx'):dest=bundle/'Dependencies/x64'/Path(n).name
  elif n=='HollowwakeExperiment.cer':dest=bundle/n
  elif n.endswith(('.log','.json','.txt','.png')) and len(n.split('/'))<=2:dest=logs/n
  else:continue
  assert '..' not in Path(n).parts;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(z.read(n))
 for name in ['AppxSignature.p7x','AppxBlockMap.xml','AppxManifest.xml']:
  with zipfile.ZipFile(bundle/'Hollowwake_0.4.5.0_x64.appx') as a:assert name in a.namelist()
for name in ['xbox-systeminfo-failure.json','systeminfo-callsite.txt','source-identity.json','sites-deployment.json','access-preservation.json','delivery-secret-scan.json','public-download-verification.json','initializer-callsite-audit.json','branding-alpha-check.json','layout-checks.json','sites-tests.log','sites-build.log','xbox-malloc-failure.json','tick-callsite-audit.json','xbox-tick-failure.json','crt-forwarder-audit.json','xbox-forwarder-failure.json','source-identity.json','boot-range-verified.json','native-dependency-check.json','website-readonly-check.json','site-mobile-home.png','site-mobile-link.png','site-desktop-home.png','site-desktop-project.png','site-desktop-downloads.png','deployed-browser-results.json','live-browser-results.json','website-mobile.png','website-desktop.png']:
 f=exp/'evidence'/name
 if f.exists():shutil.copy2(f,logs/f.name)
for name in ['credential-probes.json','credential-remediation.json']:
 shutil.copy2(root/'research-0.3.1/evidence'/name,logs/name)
for f in (exp/'docs').glob('*.md'):shutil.copy2(f,bundle/f.name)
summary={'version':'0.4.5.0','status':'Real GetSystemInfo/GetVersion import support and no-scroll directional launcher; unfinished Fortnite port','sourceCommit':commit,'workflowRun':f'https://github.com/zackjh66-droid/BLOXEN/actions/runs/{run}','hostedWindows':test,'productionForwarderFixture':forwarders,'xbox':'0.4.5 update unverified; user photographed 0.4.4 dbghelp process-attach stopping at KERNEL32!GetSystemInfo RVA1221867; original main entry/lobby not observed','fortniteLobby':'Not achieved or tested','productionServices':'Unchanged; existing accounts/admin/companion preserved','gameInputs':'No archive code or private game content redistributed; official public Season6 promotional background included under conditional fan-content terms; private console import required'}
(logs/'verification-summary.json').write_text(json.dumps(summary,indent=2))
notes='''# Hollowwake 0.4.5 — system-information startup fix and no-scroll launcher

This is a development-signed x64 UWP real-client loading experiment. It imports the actual inspected 6.21 Windows client privately onto the console and attempts original dependency initialization. It is NOT a completed Fortnite port. Original loading/lobby, character rendering, locker and native Fortnite authentication remain unfinished. This update has not been tested on Xbox.

User-reported Xbox0.4.4 stopped at KERNEL32.dll!GetSystemInfo in original dbghelp.dll, return RVA1221867. Static disassembly confirms it reads the returned dwPageSize. The immediately following original initializer calls GetVersion. New production bindings call those actual SDK APIs, including GetNativeSystemInfo, without invented hardware or OS values. GetVersion preserves Windows manifest-dependent semantics. Focused original mapped-DLL initialization checks compare results with the real host and guard SYSTEM_INFO structure bounds. No archive code is run in hosted CI.

All six menu buttons now fit in a scaled1280x720 TV-safe layout without a menu ScrollViewer. The actual guest SwapChainPanel is outside the scaling. Xbox default pointer emulation is disabled with RequiresPointerMode=WhenRequested so directional focus is used. Orange focus, HW branding and the verified Epic Season6 background remain. The long diagnostic log can still scroll; menu navigation does not need scrolling.

Hosted Windows tests verify every menu button is simultaneously visible inside the window, focus each without scrolling, and retain screenshots. Physical Xbox layout/controller behavior is unverified. Existing native memory/TLS/window/graphics/clock/allocator contracts, initializer/forwarder fixtures, named fail-fast logs and startup recovery remain tested. Real native account linking, protected automatic sign-in after restart, logout and server revocation are tested against the existing live service. See WindowsVerification and TEST-REPORT.md.

Use the SAME Hollowwake account you created on the website. Explicitly approve the device once; later launches restore its protected session automatically while it remains valid. This account is the intended authority for the game profile, username, locker and shop. The existing version-specific backend prototype maps a stable client account ID to that same project user and shares authoritative admin data. It is disabled in production, and native Fortnite authentication/transport is not implemented. No project refresh token is passed into guest code and there is no shared hardcoded game login. Website linking is not proof of Fortnite login.

The existing website/backend, accounts, owner-only admin and previous releases are unchanged. Public website: https://hollowwake.jihnjohn10.chatgpt.site . Website download page may still list the previous0.4.4 rollback; use this delivered0.4.5 bundle for the new test. Recovery email remains unavailable. The private owner site is deliberately omitted from public release metadata.

Install this as an UPDATE to Hollowwake.ClientExperiment without uninstalling to retain its private inputs and protected session. Existing boot files are enough for this startup test; do NOT import the full archive merely to fix missing APIs. After Attempt client boot, reopen Show launch log if the app closes and send the newest CALLED_UNSUPPORTED/CRASH/STOP line. Other runtime blockers remain; no lobby success is promised.

Bundle contains the signed x64 APPX, all three x64 framework dependencies, matching public CER, instructions, source/verification links and logs. The signing key was non-exportable and is not distributed. No original archive executables, DLLs or PAK files are redistributed. The official public promotional background is included under conditional noncommercial fan-content terms recorded in VISUALS.md. No matches or Epic-production connection is provided.
'''
(bundle/'README.md').write_text(notes);(out/'RELEASE-NOTES.md').write_text(notes)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
manifest={str(f.relative_to(bundle)):{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(bundle.rglob('*')) if f.is_file()};(bundle/'SHA256SUMS.json').write_text(json.dumps(manifest,indent=2))
def makezip(path,files):
 with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
  for name,f in files:z.write(f,name)
makezip(out/(bundle.name+'.zip'),[(f.relative_to(bundle).as_posix(),f) for f in sorted(bundle.rglob('*')) if f.is_file()])
makezip(out/'Hollowwake-Dependencies-x64.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted((bundle/'Dependencies').rglob('*.appx'))])
makezip(out/'Hollowwake-Verification-0.4.5.zip',[(f.relative_to(bundle).as_posix(),f) for f in sorted(logs.rglob('*')) if f.is_file()]+[(f.name,f) for f in sorted((exp/'docs').glob('*.md'))])
shutil.copy2(bundle/'Hollowwake_0.4.5.0_x64.appx',out/'Hollowwake_0.4.5.0_x64.appx');shutil.copy2(bundle/'HollowwakeExperiment.cer',out/'HollowwakeExperiment.cer')
# Preserve previously reviewed complete website/backend/admin/companion source;
# replace the runtime directories with this build's original implementation.
seed=Path('/workspace/hollowwake-experiment-release-0.4.4/Hollowwake-Source-0.4.4.zip');files={}
with zipfile.ZipFile(seed) as z:
 for n in z.namelist():
  if n.startswith(('hollowwake/client-runtime/','hollowwake/experimental-0.4.5/')):continue
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
files['hollowwake/EXPERIMENT-0.4.5-README.md']=notes.encode()
files['hollowwake/README.md']=b'# Latest experimental runtime: 0.4.5\n\nRead EXPERIMENT-0.4.5-README.md and experimental-0.4.5/docs before the preserved companion documentation below. The actual Fortnite lobby is unfinished.\n\n'+files['hollowwake/README.md']
with zipfile.ZipFile(out/'Hollowwake-Source-0.4.5.zip','w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
 for n,data in sorted(files.items()):
  assert '..' not in Path(n).parts and not n.startswith('/')
  assert Path(n).suffix.lower() not in {'.pfx','.key','.sqlite','.pak','.exe','.dll'}
  z.writestr(n,data)
art={f.name:{'bytes':f.stat().st_size,'sha256':sha(f)} for f in sorted(out.iterdir()) if f.is_file() and f.name not in {'ARTIFACTS.json','download-links.json'}};(out/'ARTIFACTS.json').write_text(json.dumps(art,indent=2));print(json.dumps(art,indent=2))
