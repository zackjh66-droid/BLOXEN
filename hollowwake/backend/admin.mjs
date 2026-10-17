// Server-only RPC. A separate owner-private Site signs requests; public users have no admin role.
import {createHmac,timingSafeEqual,randomUUID} from 'node:crypto';
const headers={'Cache-Control':'no-store','Content-Type':'application/json','X-Content-Type-Options':'nosniff'};
const json=(v,s=200)=>new Response(JSON.stringify(v),{status:s,headers});
const fail=(message,status=400)=>Object.assign(Error(message),{status});
const kinds=new Set(['cosmetic','shop','announcement','feature','release','policy','news','playlist','maintenance']);
const cleanId=v=>typeof v==='string'&&/^[A-Za-z0-9_:.\-]{1,160}$/.test(v);
export async function adminRPC(request,env){
 try{
  if(request.method!=='POST'||!env.ADMIN_RPC_SECRET||env.ADMIN_RPC_SECRET.length<32)throw fail('Unauthorized',401);
  const timestamp=request.headers.get('X-HW-Time')||'',nonce=request.headers.get('X-HW-Nonce')||'',actor=request.headers.get('X-HW-Actor')||'',signature=request.headers.get('X-HW-Signature')||'';
  if(!/^\d{13}$/.test(timestamp)||Math.abs(Date.now()-Number(timestamp))>60000||!cleanId(nonce)||!actor||actor.length>200||!/^[a-f0-9]{64}$/.test(signature))throw fail('Unauthorized',401);
  if(Number(request.headers.get('content-length')||0)>32768)throw fail('Request too large',413);
  const raw=await request.text();if(Buffer.byteLength(raw)>32768)throw fail('Request too large',413);
  const expected=createHmac('sha256',env.ADMIN_RPC_SECRET).update(`POST\n/api/control/rpc\n${timestamp}\n${nonce}\n${actor}\n${raw}`).digest();
  if(!timingSafeEqual(expected,Buffer.from(signature,'hex')))throw fail('Unauthorized',401);
  const db=env.DB;
  await db.prepare('DELETE FROM admin_nonce WHERE expires_at<?').bind(Date.now()).run();
  try{await db.prepare('INSERT INTO admin_nonce(nonce,expires_at) VALUES(?,?)').bind(nonce,Date.now()+120000).run();}catch{throw fail('Request already used',409);}
  const d=JSON.parse(raw),now=Date.now(),action=d.action;
  const query=(sql,...args)=>db.prepare(sql).bind(...args);
  const all=async(sql,...args)=>(await query(sql,...args).all()).results;
  const audit=(target,detail)=>query('INSERT INTO admin_audit(id,actor,action,target,detail,created_at) VALUES(?,?,?,?,?,?)',randomUUID(),actor,action,target,JSON.stringify(detail),now);
  async function change(statements,target,detail){await db.batch([...statements,audit(target,detail)]);return json({ok:true});}
  if(action==='overview')return json({users:(await query('SELECT COUNT(*) n FROM user').first()).n,devices:(await query('SELECT COUNT(*) n FROM project_device d JOIN session s ON s.id=d.session_id WHERE s.expiresAt>?',now).first()).n,documents:(await query('SELECT COUNT(*) n FROM admin_document').first()).n,fortniteClientAvailable:false,gameKickAvailable:false,matchmakingAvailable:false,emailDelivery:env.MAIL_API_KEY?'configured':'unavailable'});
  if(action==='users.list')return json({users:await all('SELECT id,username,name,createdAt,emailVerified FROM user WHERE username LIKE ? OR id=? ORDER BY createdAt DESC LIMIT 100','%'+String(d.search||'').slice(0,80).replace(/[%_]/g,'')+'%',String(d.search||''))});
  if(action==='users.detail'){
   if(!cleanId(d.userId))throw fail('Invalid account');
   const user=await query('SELECT id,username,name,createdAt,emailVerified FROM user WHERE id=?',d.userId).first();if(!user)throw fail('Account not found',404);
   return json({user,moderation:await query('SELECT banned_until,reason FROM project_moderation WHERE user_id=?',d.userId).first(),inventory:await all('SELECT cosmetic_id,granted_at FROM project_inventory WHERE user_id=? ORDER BY cosmetic_id',d.userId),devices:await all('SELECT d.session_id id,d.label,d.created_at,d.last_seen_at FROM project_device d JOIN session s ON s.id=d.session_id WHERE d.user_id=? AND s.expiresAt>?',d.userId,now)});
  }
  if(action==='documents.list')return json({documents:(await all('SELECT * FROM admin_document ORDER BY kind,id LIMIT 1000')).map(r=>({...r,data:JSON.parse(r.data)}))});
  if(action==='documents.save'||action==='documents.delete'){
   if(!kinds.has(d.kind)||!cleanId(d.id)||!Number.isSafeInteger(d.revision)||d.revision<0)throw fail('Invalid document');
   const old=await query('SELECT revision,kind FROM admin_document WHERE id=?',d.id).first();
   if((old?.revision||0)!==d.revision||old&&old.kind!==d.kind)throw fail('Document changed; refresh before saving',409);
   // Compare-and-swap is enforced in SQL too. The assertion makes a race roll back the whole batch.
   let statement;
   if(action==='documents.delete'){
    if(!old)throw fail('Document not found',404);
    statement=query('DELETE FROM admin_document WHERE id=? AND revision=?',d.id,d.revision);
   }else{
    if(!d.data||typeof d.data!=='object'||Array.isArray(d.data)||typeof d.data.title!=='string'||d.data.title.length<1||d.data.title.length>100||typeof d.data.enabled!=='boolean')throw fail('Title and enabled state required');
    if(d.kind==='cosmetic'&&(!cleanId(d.data.templateId)||!['character','backpack','pickaxe','glider','emote','wrap','contrail','loading'].includes(d.data.category)))throw fail('Cosmetic needs a template ID and category');
    if(d.kind==='shop'){
     if(!Number.isSafeInteger(d.data.startsAt)||!Number.isSafeInteger(d.data.endsAt)||d.data.endsAt<=d.data.startsAt||!Array.isArray(d.data.items)||d.data.items.length>100)throw fail('Invalid shop schedule/items');
     for(const item of d.data.items){if(!cleanId(item.cosmeticId)||!Number.isSafeInteger(item.price)||item.price<0)throw fail('Invalid shop item');const c=await query("SELECT id FROM admin_document WHERE kind='cosmetic' AND id=?",item.cosmeticId).first();if(!c)throw fail('Unknown cosmetic');}
    }
    if(d.kind==='feature'&&['fortnitePortAvailable','matchmakingAvailable','gameKickAvailable'].includes(d.id)&&d.data.enabled)throw fail('Unavailable capabilities cannot be enabled');
    statement=old?query('UPDATE admin_document SET data=?,revision=revision+1,updated_at=? WHERE id=? AND revision=?',JSON.stringify(d.data),now,d.id,d.revision):query('INSERT INTO admin_document(kind,id,data,revision,updated_at) VALUES(?,?,?,1,?)',d.kind,d.id,JSON.stringify(d.data),now);
   }
   // A NOT NULL violation asserts changes()=1 inside D1's atomic batch without transaction SQL.
   const assertion=query('INSERT INTO admin_nonce(nonce,expires_at) VALUES(CASE WHEN changes()=1 THEN ? ELSE NULL END,?)','cas:'+randomUUID(),now+120000);
   return await change([statement,assertion],d.id,{kind:d.kind,previousRevision:d.revision});
  }
  if(action==='audit.list')return json({events:await all('SELECT * FROM admin_audit WHERE created_at<? ORDER BY created_at DESC LIMIT 100',Number(d.before)||now+1)});
  if(['users.ban','users.unban','users.revoke','devices.revoke','inventory.grant','inventory.revoke','inventory.full','inventory.clear'].includes(action)){
   if(!cleanId(d.userId)||!(await query('SELECT id FROM user WHERE id=?',d.userId).first()))throw fail('Account not found',404);
   if(typeof d.reason!=='string'||d.reason.trim().length<3||d.reason.length>300)throw fail('An audit reason (3–300 characters) is required');
   const reason=d.reason.trim(),statements=[];
   if(action==='users.ban'){
    if(!Number.isSafeInteger(d.until)||d.until<=now||d.until>now+366*86400000)throw fail('Suspension must end within 366 days');
    statements.push(query('INSERT INTO project_moderation(user_id,banned_until,reason,updated_at) VALUES(?,?,?,?) ON CONFLICT(user_id) DO UPDATE SET banned_until=excluded.banned_until,reason=excluded.reason,updated_at=excluded.updated_at',d.userId,d.until,reason,now),query('DELETE FROM session WHERE userId=?',d.userId),query('DELETE FROM deviceCode WHERE userId=?',d.userId));
   }
   if(action==='users.unban')statements.push(query('DELETE FROM project_moderation WHERE user_id=?',d.userId));
   if(action==='users.revoke')statements.push(query('DELETE FROM session WHERE userId=?',d.userId));
   if(action==='devices.revoke'){if(!cleanId(d.deviceId))throw fail('Invalid device');const owned=await query('SELECT session_id FROM project_device WHERE session_id=? AND user_id=?',d.deviceId,d.userId).first();if(!owned)throw fail('Device not found',404);statements.push(query('DELETE FROM session WHERE id=? AND userId=?',d.deviceId,d.userId));}
   if(action==='inventory.full')statements.push(query("INSERT OR IGNORE INTO project_inventory(id,user_id,cosmetic_id,granted_at) SELECT ?||id,?,id,? FROM admin_document WHERE kind='cosmetic' AND json_extract(data,'$.enabled')=1",randomUUID()+':',d.userId,now));
   if(action==='inventory.clear')statements.push(query('DELETE FROM project_inventory WHERE user_id=?',d.userId));
   if(action==='inventory.grant'||action==='inventory.revoke'){
    if(!cleanId(d.cosmeticId)||!(await query("SELECT id FROM admin_document WHERE id=? AND kind='cosmetic'",d.cosmeticId).first()))throw fail('Cosmetic not found',404);
    statements.push(action==='inventory.grant'?query('INSERT OR IGNORE INTO project_inventory(id,user_id,cosmetic_id,granted_at) VALUES(?,?,?,?)',randomUUID(),d.userId,d.cosmeticId,now):query('DELETE FROM project_inventory WHERE user_id=? AND cosmetic_id=?',d.userId,d.cosmeticId));
   }
   return await change(statements,d.userId,{reason,cosmeticId:d.cosmeticId||null,deviceId:d.deviceId||null,until:d.until||null});
  }
  if(action==='game.kick'||action==='matchmaking.enable')throw fail('Requires a working Fortnite runtime and game service; unavailable',409);
  throw fail('Unknown action',400);
 }catch(e){return json({error:e.status?e.message:'Request failed; refresh and retry.'},e.status||500);}
}
