// Authenticated device-only metadata. No game bytes, project tokens or private URLs are embedded.
import {createHash} from 'node:crypto';
export const CONTENT_BUILD='6.21-CL-4526925';
const ARCHIVE='https://public-build-archives-bucket.erafn.dev/6.21.zip';
const ARCHIVE_SHA='bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5';
const allowed=/\.(pak|sig|mp4|srt|png|jpg|bmp|txt|bin|dat)$/i;
export function contentPath(path){
 return typeof path==='string'&&path.length<512&&/^(6\.21\/FortniteGame\/Content\/|6\.21\/Engine\/Binaries\/ThirdParty\/CEF3\/Win64\/)/.test(path)&&allowed.test(path)&&!/[\\:\x00-\x1f%]/.test(path)&&path.split('/').every(x=>x&&x!=='.'&&x!=='..'&&!/[. ]$/.test(x)&&! /^(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|$)/i.test(x));
}
export function validateContentManifest(value){
 if(!value||value.schema!==1||value.build!==CONTENT_BUILD||!value.archive||value.archive.url!==ARCHIVE||value.archive.bytes!==12528679454||value.archive.sha256!==ARCHIVE_SHA||!Array.isArray(value.files)||!value.files.length||value.files.length>10000)throw Error('Invalid content manifest');
 let total=0;const seen=new Set();
 const files=value.files.map(f=>{if(!contentPath(f.path)||seen.has(f.path.toLowerCase())||!Number.isSafeInteger(f.bytes)||f.bytes<0||f.bytes>12*1024**3||! /^[0-9a-f]{64}$/.test(f.sha256))throw Error('Invalid content record');seen.add(f.path.toLowerCase());total+=f.bytes;return{path:f.path,bytes:f.bytes,sha256:f.sha256};}).sort((a,b)=>a.path<b.path?-1:a.path>b.path?1:0);
 if(total>64*1024**3)throw Error('Content expansion exceeds limit');
 const normalized={schema:1,build:CONTENT_BUILD,archive:{url:ARCHIVE,bytes:12528679454,sha256:ARCHIVE_SHA},files};
 return {...normalized,expandedBytes:total,manifestId:createHash('sha256').update(JSON.stringify(normalized)).digest('hex')};
}
export function contentManifestResponse(value){
 if(!value)return new Response(JSON.stringify({error:'content_not_configured',message:'An owner-approved private content manifest has not been configured.'}),{status:503,headers:{'Content-Type':'application/json','Cache-Control':'no-store'}});
 try{return Response.json({...validateContentManifest(typeof value==='string'?JSON.parse(value):value),expiresAt:Date.now()+15*60*1000},{headers:{'Cache-Control':'no-store','Referrer-Policy':'no-referrer','X-Content-Type-Options':'nosniff'}});}
 catch{return Response.json({error:'invalid_content_configuration'},{status:503,headers:{'Cache-Control':'no-store'}});}
}
