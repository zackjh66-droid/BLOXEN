// Preserve the exact already-tested licensed dependencies and their source.
// No game files are included. Operator credential is accepted on hidden stdin.
import {put,head,get,BlobNotFoundError} from '@vercel/blob';
import {EnvHttpProxyAgent,setGlobalDispatcher} from 'undici';
import {readFile} from 'node:fs/promises';import {createHash} from 'node:crypto';import {createInterface} from 'node:readline';
setGlobalDispatcher(new EnvHttpProxyAgent());
const sha='233ed20ffcf8d87230c126453e6a99d36060dddb5f32ac994ec9ba6619f200dd';
const path=`licensed-providers/${sha}/providers.zip`,bytes=39172799;
const input=createInterface({input:process.stdin,terminal:false});console.log('Ready for hidden provider-cache configuration');
const cfg=await new Promise(resolve=>input.once('line',line=>{input.close();resolve(JSON.parse(line));}));
try{
 const raw=await readFile('/workspace/hollowwake/startup-xbox-42/providers28.zip');
 if(raw.length!==bytes||createHash('sha256').update(raw).digest('hex')!==sha)throw Error('Preserved provider archive hash differs');
 const options={storeId:cfg.storeId,token:cfg.token};let existing;
 try{existing=await head(path,options);}catch(e){if(!(e instanceof BlobNotFoundError))throw e;}
 if(!existing)await put(path,raw,{...options,access:'private',addRandomSuffix:false,allowOverwrite:false,contentType:'application/zip'});
 const stored=await head(path,options);if(stored.size!==bytes||stored.pathname!==path)throw Error('Stored provider metadata differs');
 const fetched=await get(path,{...options,access:'private',useCache:false});if(!fetched||fetched.statusCode!==200)throw Error('Stored provider read failed');
 const hash=createHash('sha256');let received=0;for await(const chunk of fetched.stream){hash.update(chunk);received+=chunk.length;}
 if(received!==bytes||hash.digest('hex')!==sha)throw Error('Full stored provider SHA-256 differs');
 const anonymous=await fetch(stored.url,{redirect:'manual'});await anonymous.body?.cancel();if(![401,403].includes(anonymous.status))throw Error('Unsigned provider read was not denied');
 console.log('PASS exact preserved licensed archive, complete stored SHA-256, source retained, anonymous access denied');
}catch(e){console.error('Provider cache failed: '+String(e.message||e.name).replace(/https?:\/\/[^\s]+/g,'[URL]').split(cfg.token).join('[credential]'));process.exitCode=1;}
