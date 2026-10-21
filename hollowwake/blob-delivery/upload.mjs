// Hosted operator tool. Credentials arrive over hidden stdin and remain in RAM.
import {createReadStream} from 'node:fs';import {stat,open,writeFile} from 'node:fs/promises';import {createHash} from 'node:crypto';import {createInterface} from 'node:readline';
import {EnvHttpProxyAgent,setGlobalDispatcher} from 'undici';import {createMultipartUploader} from '@vercel/blob';import {ARCHIVE,SHA,BYTES,digest} from './policy.mjs';
setGlobalDispatcher(new EnvHttpProxyAgent());
const input=createInterface({input:process.stdin,terminal:false});console.log('Ready for hidden operator configuration');
const cfg=await new Promise(resolve=>input.once('line',line=>{input.close();resolve(JSON.parse(line));}));
const source='/workspace/fortnite-alternative-analysis/6.21.zip';let phase='configuration';
function check(value,message){if(!value){const e=new Error(message);e.safe=message;throw e;}}
function privateUrl(text){const u=new URL(text);check(u.protocol==='https:'&&u.hostname.endsWith('.private.blob.vercel-storage.com'),'Unexpected private storage origin');return u.href;}
async function control(body){const r=await fetch(cfg.origin+'/api/control',{method:'POST',headers:{'Content-Type':'application/json','x-vercel-protection-bypass':cfg.bypass,'x-hollowwake-upload':cfg.upload},body:JSON.stringify(body)});check(r.status===200,'Control HTTP '+r.status);const b=await r.json();check(!b.error,'Control rejected '+String(b.error));return b;}
const report={build:'6.21-CL-4526925',archiveBytes:BYTES,archiveSha256:SHA,gameExecuted:false,proprietaryBytesPublic:false};
try{
 phase='private fixture';const probe=await control({op:'probe'});
 const denied=await fetch(privateUrl(probe.url),{redirect:'manual'});check([401,403].includes(denied.status),'Unsigned private fixture was accessible');await denied.body?.cancel();
 const proof=await fetch(privateUrl(probe.getUrl));const data=Buffer.from(await proof.arrayBuffer());check(proof.status===200&&data.length===probe.bytes&&digest(data)===probe.sha256,'Private fixture read/hash differs');report.privateFixtureVerified=true;report.anonymousFixtureDenied=denied.status;console.log('PASS actual private fixture write/read/hash and anonymous denial');
 phase='preserved input verification';check((await stat(source)).size===BYTES,'Preserved archive length differs');const before=createHash('sha256');for await(const b of createReadStream(source))before.update(b);check(before.digest('hex')===SHA,'Preserved original archive SHA-256 differs');console.log('PASS preserved complete archive SHA-256; beginning private multipart upload');
 phase='multipart upload';
 const signed=await control({type:'blob.generate-presigned-url',payload:{pathname:ARCHIVE,multipart:true}});
 // SDK 2.8.2 accepts a scoped presignedUrlPayload in its control-plane auth.
 // Manual parts avoid the automatic stream uploader retaining a large queue.
 const uploader=await createMultipartUploader(ARCHIVE,{access:'private',contentType:'application/zip',presignedUrlPayload:signed.presignedUrlPayload});
 const handle=await open(source,'r');const parts=[];let sent=0,last=-1;
 try{while(sent<BYTES){const size=Math.min(32*1024*1024,BYTES-sent);const bytes=Buffer.alloc(size);const r=await handle.read(bytes,0,size,sent);check(r.bytesRead===size,'Local archive ended during upload');parts.push(await uploader.uploadPart(parts.length+1,bytes));sent+=size;const step=Math.floor(sent/1000000000);if(step!==last){last=step;console.log('Uploaded completed bytes: '+sent+' / '+BYTES);}}}finally{await handle.close();}
 const result=await uploader.complete(parts);
 check(result.pathname===ARCHIVE,'Uploaded pathname differs');privateUrl(result.url);console.log('PASS private multipart completion; verifying the stored archive');
 phase='mirror metadata';const remote=await control({op:'inspect'});check(remote.bytes===BYTES&&remote.sha256===SHA,'Mirror metadata differs');
 const anonymous=await fetch(privateUrl(remote.url),{redirect:'manual'});check([401,403].includes(anonymous.status),'Unsigned private archive was accessible');await anonymous.body?.cancel();report.anonymousArchiveDenied=anonymous.status;
 phase='full stored archive verification';const response=await fetch(privateUrl(remote.getUrl));check(response.status===200,'Private full read HTTP '+response.status);const after=createHash('sha256');let received=0,shown=-1;for await(const block of response.body){received+=block.length;check(received<=BYTES,'Mirror body exceeds its pin');after.update(block);const step=Math.floor(received/2000000000);if(step!==shown){shown=step;console.log('Verified stream bytes: '+received+' / '+BYTES);}}check(received===BYTES&&after.digest('hex')===SHA,'Complete stored archive hash differs');report.completeMirrorReadVerified=true;
 phase='real range checks';const rangeHandle=await open(source,'r');report.ranges=[];try{for(const at of [0,65536,5000000000,BYTES-32]){const size=32,expected=Buffer.alloc(size);await rangeHandle.read(expected,0,size,at);const r=await fetch(privateUrl(remote.getUrl),{headers:{Range:'bytes='+at+'-'+(at+size-1)}});check(r.status===206&&r.headers.get('content-range')==='bytes '+at+'-'+(at+size-1)+'/'+BYTES,'Private exact range HTTP differs');const bytes=Buffer.from(await r.arrayBuffer());check(bytes.equals(expected),'Private ranged bytes differ');report.ranges.push({start:at,bytes:size,sha256:digest(bytes)});}}finally{await rangeHandle.close();}
 phase='seal';await control({op:'seal',bytes:BYTES,sha256:SHA,fullDownloadVerified:true});await control({op:'remove-probe'});report.sealed=true;report.pathname=ARCHIVE;report.blobHost=new URL(remote.url).hostname;report.verifiedAt=new Date().toISOString();await writeFile('mirror-verification.json',JSON.stringify(report,null,2)+'\n');console.log('PASS full private mirror SHA-256, four exact ranges, anonymous denial and sealed readiness; no game execution');
}catch(error){console.error('FAILED phase='+phase+'; '+String(error.safe||error.message||error.name||'Error').replace(/https?:\/\/[^\s]+/g,'[URL]').split(cfg.bypass).join('[credential]').split(cfg.upload).join('[credential]'));process.exitCode=1;}
