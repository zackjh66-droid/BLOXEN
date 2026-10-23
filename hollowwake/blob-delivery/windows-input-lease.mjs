// Operator-authorized, read-only preparation for one private Windows test.
// Never exports a Blob write credential or plaintext signed URL.
import {head,get,issueSignedToken,presignUrl} from '@vercel/blob';
import {EnvHttpProxyAgent,setGlobalDispatcher} from 'undici';
import {createInterface} from 'node:readline';import {readFile,writeFile,open} from 'node:fs/promises';
import {randomBytes,publicEncrypt,constants,createCipheriv} from 'node:crypto';
import {ARCHIVE,MARKER,SHA,BYTES,BUILD} from './policy.mjs';
setGlobalDispatcher(new EnvHttpProxyAgent());
const input=createInterface({input:process.stdin,terminal:false});console.log('Ready for hidden read-only private input configuration');
const cfg=await new Promise(resolve=>input.once('line',line=>{input.close();resolve(JSON.parse(line));}));
try{
 const options={storeId:cfg.storeId,token:cfg.token};const info=await head(ARCHIVE,options);
 if(info.size!==BYTES||info.pathname!==ARCHIVE)throw Error('Private archive metadata mismatch');
 const marker=await get(MARKER,{...options,access:'private',useCache:false});if(!marker||marker.statusCode!==200)throw Error('Verified private release marker unavailable');
 const verified=JSON.parse(await new Response(marker.stream).text());if(verified.sha256!==SHA||verified.bytes!==BYTES||verified.private!==true)throw Error('Private verified marker differs');
 const expiresAt=Date.now()+45*60000;const signed=await issueSignedToken({...options,pathname:ARCHIVE,operations:['get'],validUntil:expiresAt});const url=(await presignUrl(signed,{operation:'get',pathname:ARCHIVE,access:'private',validUntil:expiresAt})).presignedUrl;
 const parsed=new URL(url);if(parsed.hostname!=='ricswvi50gagbcuh.private.blob.vercel-storage.com'||parsed.pathname!=='/'+ARCHIVE)throw Error('Private host/path mismatch');
 const local=await open('/workspace/fortnite-alternative-analysis/6.21.zip','r');try{for(const at of [0,5000000000]){const expected=Buffer.alloc(32);await local.read(expected,0,32,at);const r=await fetch(url,{redirect:'manual',headers:{Range:`bytes=${at}-${at+31}`}});if(r.status!==206||r.headers.get('content-range')!==`bytes ${at}-${at+31}/${BYTES}`||!Buffer.from(await r.arrayBuffer()).equals(expected))throw Error('Private input exact range mismatch');}}finally{await local.close();}
 if(cfg.publicKeyPath){
  const recipient=JSON.parse(await readFile(cfg.publicKeyPath,'utf8'));if(recipient.runId!==String(cfg.runId)||recipient.algorithm!=='RSA-OAEP-SHA256+A256GCM')throw Error('One-run recipient identity mismatch');
  const key=randomBytes(32),nonce=randomBytes(12);const cipher=createCipheriv('aes-256-gcm',key,nonce);cipher.setAAD(Buffer.from(String(cfg.runId)));
  const lease={runId:String(cfg.runId),build:BUILD,archiveUrl:url,archiveBytes:BYTES,archiveSha256:SHA,private:true,expiresAt};
  if(cfg.includeLicensedProviders){
   const providerSha='233ed20ffcf8d87230c126453e6a99d36060dddb5f32ac994ec9ba6619f200dd',providerBytes=39172799;
   const providerPath=`licensed-providers/${providerSha}/providers.zip`,provider=await head(providerPath,options);
   if(provider.size!==providerBytes||provider.pathname!==providerPath)throw Error('Preserved licensed provider metadata differs');
   const providerToken=await issueSignedToken({...options,pathname:providerPath,operations:['get'],validUntil:expiresAt});
   const providerUrl=(await presignUrl(providerToken,{operation:'get',pathname:providerPath,access:'private',validUntil:expiresAt})).presignedUrl;
   const origin=new URL(providerUrl);if(origin.hostname!==parsed.hostname||origin.pathname!=='/'+providerPath)throw Error('Licensed provider origin differs');
   lease.licensedProviders={url:providerUrl,bytes:providerBytes,sha256:providerSha};
  }
  const ciphertext=Buffer.concat([cipher.update(JSON.stringify(lease)),cipher.final()]);
  const wrapped=publicEncrypt({key:Buffer.from(recipient.publicKey,'base64'),format:'der',type:'spki',padding:constants.RSA_PKCS1_OAEP_PADDING,oaepHash:'sha256'},key);
  await writeFile(cfg.outputPath,JSON.stringify({runId:String(cfg.runId),key:wrapped.toString('base64'),nonce:nonce.toString('base64'),ciphertext:ciphertext.toString('base64'),tag:cipher.getAuthTag().toString('base64')})+'\n',{mode:0o600});key.fill(0);
  console.log('PASS read-only private lease encrypted to one-run recipient; no account session/write credential exported');
 }else console.log('PASS existing verified private archive and exact ranges; no full transfer or game execution claimed');
}catch(e){console.error('Private input operation failed: '+String(e.message||e.name).replace(/https?:\/\/[^\s]+/g,'[URL]').split(cfg.token).join('[credential]'));process.exitCode=1;}
