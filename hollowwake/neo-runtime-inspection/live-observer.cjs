'use strict';
const fs=require('node:fs');
const path=require('node:path');
const {spawn,spawnSync}=require('node:child_process');
const {DefaultArtifactClient}=require('@actions/artifact');
const temp=process.env.RUNNER_TEMP;
const evidence=path.join(temp,'NeoReferenceEvidence');
const childEnv={...process.env};
for(const key of Object.keys(childEnv))if(key.startsWith('ACTIONS_')||key==='GITHUB_TOKEN'||key==='GH_TOKEN')delete childEnv[key];
const artifact=new DefaultArtifactClient();
const delay=ms=>new Promise(r=>setTimeout(r,ms));
let sequence=0;
async function upload(){
 if(!fs.existsSync(evidence))return;
 const protect=spawnSync('pwsh.exe',['-NoProfile','-File',path.join(temp,'Protect-NeoEvidence.ps1'),'-OutputDirectory',evidence,'-PublicKey',path.join(temp,'recipient-public.pem')],{env:childEnv,stdio:'ignore',timeout:45000});
 if(protect.status!==0){console.log('Private evidence encryption failed; uploading current metadata without stale screenshots');}
 const out=path.join(temp,`NeoEvidenceSnapshot-${++sequence}`);fs.mkdirSync(out,{recursive:true});
 const allowed=['neo-runtime-result.json','neo-installed-before.json','neo-installed-after.json'];
 if(protect.status===0)allowed.push('neo-screens-delivery.json','Neo-Reference-Screens.hwenc');
 const files=[];
 for(const name of allowed){const src=path.join(evidence,name);if(fs.existsSync(src)){const dest=path.join(out,name);fs.copyFileSync(src,dest);files.push(dest);}}
 if(files.length){await artifact.uploadArtifact(`neo-reference-live-${String(sequence).padStart(3,'0')}`,files,out,{retentionDays:2});console.log(`Uploaded encrypted observation ${sequence}`);}
 fs.rmSync(out,{recursive:true,force:true});
}
(async()=>{
 const log=fs.openSync(path.join(temp,'neo-observer-private.stdout'),'w');
 const err=fs.openSync(path.join(temp,'neo-observer-private.stderr'),'w');
 const observer=spawn('powershell.exe',['-NoProfile','-ExecutionPolicy','RemoteSigned','-File',path.join(temp,'Run-NeoReference.ps1'),'-OutputDirectory',evidence,'-MaximumSeconds','18000'],{env:childEnv,stdio:['ignore',log,err]});
 let done=false,exit=null;observer.on('exit',c=>{done=true;exit=c;});observer.on('error',()=>{done=true;exit=-1;});
 const deadline=Date.now()+315*60*1000;
 while(!done&&Date.now()<deadline){await delay(60000);try{await upload();}catch(e){console.log('Evidence upload failed; next observation will retry: '+e.name);}}
 if(!done){observer.kill();console.log('Observer reached bounded session timeout');}
 try{await upload();}catch(e){console.log('Final evidence upload failed: '+e.name);process.exitCode=1;}
 fs.closeSync(log);fs.closeSync(err);
 if(exit!==0){console.log('Observer exit status: '+exit);process.exitCode=1;}
})().catch(e=>{console.log('Observer action failed: '+e.name);process.exitCode=1;});
