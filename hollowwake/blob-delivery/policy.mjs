import {createHash,createHmac,timingSafeEqual} from 'node:crypto';
export const BUILD='6.21-CL-4526925';
export const SHA='bae66d8b0ff7e8ebea4361e835579c180bc2a2e618d04808f72b40214ababee5';
export const BYTES=12528679454;
export const ARCHIVE=`client621/${SHA}/6.21.zip`;
export const MARKER=`client621/${SHA}/verified.json`;
export const PROBE='owned-fixtures/private-delivery-v1.txt';
export const PROBE_DATA=Buffer.from('Hollowwake original private storage fixture\n');
export const digest=data=>createHash('sha256').update(data).digest('hex');
export function uploadAuthorized(headers,env){const value=headers['x-hollowwake-upload']||'';return /^[a-f0-9]{64}$/.test(env.UPLOAD_KEY_SHA256||'')&&value.length>=32&&value.length<=128&&timingSafeEqual(Buffer.from(digest(value),'hex'),Buffer.from(env.UPLOAD_KEY_SHA256,'hex'));}
export function leaseAuthorized(headers,body,env,now=Date.now()){
 const time=headers['x-hollowwake-time']||'',nonce=headers['x-hollowwake-nonce']||'',signature=headers['x-hollowwake-signature']||'';
 if(!env.CONTENT_GATEWAY_KEY||!/^\d{13}$/.test(time)||Math.abs(now-Number(time))>60000||!/^[-a-f0-9]{36}$/.test(nonce)||!/^[a-f0-9]{64}$/.test(signature)||body.op!=='lease'||body.build!==BUILD||body.sha256!==SHA||Object.keys(body).sort().join(',')!=='build,op,sha256')return false;
 const expected=createHmac('sha256',env.CONTENT_GATEWAY_KEY).update([time,nonce,body.op,body.build,body.sha256].join('\n')).digest();return timingSafeEqual(expected,Buffer.from(signature,'hex'));
}
