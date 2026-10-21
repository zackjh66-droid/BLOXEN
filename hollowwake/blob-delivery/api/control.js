import {put,get,head,del,issueSignedToken,presignUrl} from '@vercel/blob';
import {BUILD,SHA,BYTES,ARCHIVE,MARKER,PROBE,PROBE_DATA,digest,uploadAuthorized,leaseAuthorized} from '../policy.mjs';
export const config={maxDuration:60};
const storeId=process.env.BLOB_STORE_ID;
const send=(res,status,body)=>{res.setHeader('Cache-Control','no-store');res.setHeader('X-Content-Type-Options','nosniff');res.setHeader('Referrer-Policy','no-referrer');return res.status(status).json(body);};
async function signed(pathname,operation,validUntil){const token=await issueSignedToken({storeId,pathname,operations:[operation],validUntil});return (await presignUrl(token,{operation,pathname,validUntil,access:'private'})).presignedUrl;}
async function ready(){try{const result=await get(MARKER,{access:'private',storeId,useCache:false});if(!result||result.statusCode!==200)return false;const text=await new Response(result.stream).text();if(text.length>2048)return false;const mark=JSON.parse(text);return mark.sha256===SHA&&mark.bytes===BYTES&&mark.private===true;}catch{return false;}}
export default async function handler(req,res){
 if(req.method!=='POST')return send(res,404,{error:'not_found'});
 let body;try{body=typeof req.body==='string'?JSON.parse(req.body):req.body;if(!body||typeof body!=='object'||Array.isArray(body)||JSON.stringify(body).length>4096)return send(res,400,{error:'invalid_request'});}catch{return send(res,400,{error:'invalid_request'});}
 const owner=uploadAuthorized(req.headers,process.env);
 if(!owner&&!leaseAuthorized(req.headers,body,process.env))return send(res,404,{error:'not_found'});
 if(!storeId)return send(res,503,{error:'store_not_configured'});
 try{
  if(body.type==='blob.generate-presigned-url'){
   if(!owner||body.payload?.pathname!==ARCHIVE||body.payload?.multipart!==true)return send(res,403,{error:'upload_scope_denied'});
   if(await ready())return send(res,409,{error:'release_sealed'});
   const validUntil=Date.now()+2*60*60*1000;
   const token=await issueSignedToken({storeId,pathname:ARCHIVE,operations:['put'],validUntil,maximumSizeInBytes:BYTES,allowedContentTypes:['application/zip']});
   // No webhook is registered. Produce the SDK 2.8.2 client wire payload from
   // its official presignUrl result, preserving every signed query field.
   const signed=await presignUrl(token,{operation:'put',pathname:ARCHIVE,access:'private',validUntil,maximumSizeInBytes:BYTES,allowedContentTypes:['application/zip'],addRandomSuffix:false,allowOverwrite:false,cacheControlMaxAge:60});
   const query=new URL(signed.presignedUrl).searchParams;
   const delegationToken=query.get('vercel-blob-delegation'),signature=query.get('vercel-blob-signature');
   query.delete('pathname');query.delete('vercel-blob-delegation');query.delete('vercel-blob-signature');
   if(!delegationToken||!signature)throw new Error('Invalid SDK signed upload payload');
   return send(res,200,{type:body.type,presignedUrlPayload:{delegationToken,signature,params:Object.fromEntries(query)}});
  }
  if(owner&&body.op==='probe'){
   try{await head(PROBE,{storeId});}catch{await put(PROBE,PROBE_DATA,{storeId,access:'private',addRandomSuffix:false,allowOverwrite:false,contentType:'text/plain'});}
   const info=await head(PROBE,{storeId});const validUntil=Date.now()+5*60*1000;
   return send(res,200,{url:info.url,getUrl:await signed(PROBE,'get',validUntil),sha256:digest(PROBE_DATA),bytes:PROBE_DATA.length});
  }
  if(owner&&body.op==='remove-probe'){await del(PROBE,{storeId});return send(res,200,{ok:true});}
  if(owner&&body.op==='inspect'){
   const info=await head(ARCHIVE,{storeId});if(info.size!==BYTES||info.pathname!==ARCHIVE)return send(res,409,{error:'archive_size_mismatch'});
   return send(res,200,{url:info.url,getUrl:await signed(ARCHIVE,'get',Date.now()+2*60*60*1000),bytes:info.size,sha256:SHA,sealed:await ready()});
  }
  if(owner&&body.op==='seal'){
   if(body.bytes!==BYTES||body.sha256!==SHA||body.fullDownloadVerified!==true)return send(res,400,{error:'verification_required'});
   const info=await head(ARCHIVE,{storeId});if(info.size!==BYTES||info.pathname!==ARCHIVE)return send(res,409,{error:'archive_size_mismatch'});
   if(!await ready())await put(MARKER,JSON.stringify({build:BUILD,sha256:SHA,bytes:BYTES,private:true,verifiedAt:new Date().toISOString()}),{storeId,access:'private',addRandomSuffix:false,allowOverwrite:false,contentType:'application/json'});
   return send(res,200,{ok:true,sha256:SHA,bytes:BYTES});
  }
  if(body.op==='lease'){
   if(!await ready())return send(res,503,{error:'content_not_ready'});const expiresAt=Date.now()+15*60*1000;
   return send(res,200,{build:BUILD,archiveUrl:await signed(ARCHIVE,'get',expiresAt),archiveBytes:BYTES,archiveSha256:SHA,expiresAt,private:true});
  }
  return send(res,404,{error:'not_found'});
 }catch(error){return send(res,502,{error:'private_storage_operation_failed',kind:typeof error?.name==='string'?error.name:'StorageError'});}
}
