// Original protocol research implementation. No Lawin source/default profiles copied.
// This is NOT certified against a running 6.21 client. No Epic services or project refresh tokens.
import {createHash,randomBytes,randomUUID} from 'node:crypto';
const BUILD='6.21-CL-4526925',CLIENT='hollowwake-fortnite-621',PREFIX='/client621';
const hasProjectCookie=r=>(r.headers.get('cookie')||'').split(';').some(v=>/^\s*(?:__Secure-|__Host-)?hollowwake[._-]/i.test(v));
const sha=v=>createHash('sha256').update(v).digest('hex');
const headers={'Cache-Control':'no-store','Content-Type':'application/json','Referrer-Policy':'no-referrer','X-Content-Type-Options':'nosniff'};
const json=(v,status=200)=>new Response(JSON.stringify(v),{status,headers});
const failure=(status,code,message=code)=>Object.assign(Error(message),{status,code});
const slots={Character:['AthenaCharacter','favorite_character'],Backpack:['AthenaBackpack','favorite_backpack'],Pickaxe:['AthenaPickaxe','favorite_pickaxe'],Glider:['AthenaGlider','favorite_glider'],SkyDiveContrail:['AthenaSkyDiveContrail','favorite_skydivecontrail'],LoadingScreen:['AthenaLoadingScreen','favorite_loadingscreen'],Dance:['AthenaDance','favorite_dance']};
export function createFortnite621Service(db,{origin,enabled=false}={}){
 const query=(sql,...args)=>db.prepare(sql).bind(...args),all=async(sql,...args)=>(await query(sql,...args).all()).results;
 async function parent(userId,parentId){
  const current=await query(`SELECT s.id,s.expiresAt,u.id user_id,u.username,u.name FROM session s JOIN project_device d ON s.id=d.session_id JOIN user u ON u.id=s.userId WHERE s.id=? AND s.userId=? AND s.expiresAt>? AND d.created_at>?`,parentId,userId,Date.now(),Date.now()-30*86400000).first();
  if(!current)throw failure(401,'authorization_failed');
  const banned=await query('SELECT banned_until FROM project_moderation WHERE user_id=?',userId).first();if(banned?.banned_until>Date.now())throw failure(403,'account_not_active');return current;
 }
 async function identity(userId){await query('INSERT OR IGNORE INTO client621_account(user_id,account_id) VALUES(?,?)',userId,randomUUID().replaceAll('-','')).run();return(await query('SELECT account_id FROM client621_account WHERE user_id=?',userId).first()).account_id;}
 async function issue(user,parentSession){if(!enabled)throw failure(404,'adapter_disabled');await parent(user.id,parentSession);const exchange=randomBytes(32).toString('base64url');await query('DELETE FROM client621_exchange WHERE expires_at<?',Date.now()).run();await query('INSERT INTO client621_exchange(code_hash,user_id,parent_session,expires_at) VALUES(?,?,?,?)',sha(exchange),user.id,parentSession,Date.now()+60000).run();return {exchange_code:exchange,expires_in:60,client_id:CLIENT,audience:'hollowwake-client621-only',build:BUILD,clientValidated:false};}
 async function authorize(request){
  if(hasProjectCookie(request))throw failure(401,'adapter_bearer_required');const auth=request.headers.get('authorization')||'';if(!/^Bearer hw621_[A-Za-z0-9_-]{43}$/i.test(auth))throw failure(401,'adapter_bearer_required');
  const token=auth.slice(7),session=await query('SELECT * FROM client621_session WHERE token_hash=? AND expires_at>?',sha(token),Date.now()).first();if(!session)throw failure(401,'authorization_failed');const user=await parent(session.user_id,session.parent_session);return{...session,user,accountId:await identity(session.user_id)};
 }
 async function inventory(userId){return await all(`SELECT i.cosmetic_id,d.data,d.revision metadata_revision,a.proof_sha256 FROM project_inventory i JOIN admin_document d ON d.id=i.cosmetic_id AND d.kind='cosmetic' JOIN client621_asset a ON a.template_id=lower(json_extract(d.data,'$.templateId')) WHERE i.user_id=? AND a.build=? AND a.verified=1 AND json_extract(d.data,'$.enabled')=1 ORDER BY i.cosmetic_id`,userId,BUILD);}
 async function state(userId){
  const items=await inventory(userId),fingerprint=sha(JSON.stringify(items));await query('INSERT OR IGNORE INTO client621_profile(user_id,revision,equipment,inventory_fingerprint,updated_at) VALUES(?,1,?,?,?)',userId,'{}',fingerprint,Date.now()).run();
  await query('UPDATE client621_profile SET revision=revision+1,inventory_fingerprint=?,updated_at=? WHERE user_id=? AND inventory_fingerprint<>?',fingerprint,Date.now(),userId,fingerprint).run();const row=await query('SELECT * FROM client621_profile WHERE user_id=?',userId).first();return{...row,items};
 }
 function document(current,profileId,s){
  const items={},equipment=JSON.parse(s.equipment);for(const item of s.items){const data=JSON.parse(item.data);items['hw_'+sha(item.cosmetic_id).slice(0,24)]={templateId:data.templateId,attributes:{item_seen:true,variants:[],level:1,xp:0},quantity:1};}
  const attrs=profileId==='athena'?{season_num:6,level:1,season_level:1,xp:0,battlestars:0,book_level:1,book_xp:0,book_purchased:false,favorite_character:'',favorite_backpack:'',favorite_pickaxe:'',favorite_glider:'',favorite_skydivecontrail:'',favorite_loadingscreen:'',favorite_dance:['','','','','','']}:{current_mtx_platform:'EpicPC',mtx_affiliate:'',allowed_to_receive_gifts:false};
  if(profileId==='athena')for(const [name,value]of Object.entries(equipment)){
   // A grant revoked by the owner can never remain equipped in a returned profile.
   if(Array.isArray(value))attrs[name]=value.map(x=>items[x]?x:'');else if(items[value])attrs[name]=value;
  }
  return {created:new Date(s.updated_at).toISOString(),updated:new Date(s.updated_at).toISOString(),rvn:s.revision,wipeNumber:1,accountId:current.accountId,profileId,version:'hollowwake-621-prototype',items:profileId==='athena'?items:{},stats:{attributes:attrs},commandRevision:s.revision};
 }
 const envelope=(current,profileId,s,base=s.revision)=>({profileRevision:s.revision,profileId,profileChangesBaseRevision:base,profileChanges:[{changeType:'fullProfileUpdate',profile:document(current,profileId,s)}],profileCommandRevision:s.revision,serverTime:new Date().toISOString(),responseVersion:1});
 async function body(request,form=false){if(Number(request.headers.get('content-length')||0)>16384)throw failure(413,'request_too_large');const text=await request.text();if(Buffer.byteLength(text)>16384)throw failure(413,'request_too_large');if(form){if(!request.headers.get('content-type')?.startsWith('application/x-www-form-urlencoded'))throw failure(415,'form_required');const params=new URLSearchParams(text);for(const key of new Set(params.keys()))if(params.getAll(key).length!==1)throw failure(400,'duplicate_field');return Object.fromEntries(params);}if(!request.headers.get('content-type')?.startsWith('application/json'))throw failure(415,'json_required');try{const d=JSON.parse(text);if(!d||typeof d!=='object'||Array.isArray(d))throw Error();return d;}catch{throw failure(400,'invalid_json');}}
 async function fetch(request){
  try{
   if(!enabled)return json({error:'adapter_disabled'},404);const url=new URL(request.url),path=url.pathname.slice(PREFIX.length);
   if(url.origin!==origin||request.headers.has('origin')&&request.headers.get('origin')!==origin)throw failure(403,'invalid_origin');
   for(const key of ['access_token','refresh_token','password','exchange_code','device_code'])if(url.searchParams.has(key))throw failure(400,'secret_in_url');
   const ua=request.headers.get('user-agent')||'';if(/Fortnite/i.test(ua)&&!ua.includes('6.21')||/Fortnite/i.test(ua)&&!/CL-4526925(?:\D|$)/.test(ua))throw failure(400,'wrong_client_build');
   if(path==='/account/api/oauth/token'&&request.method==='POST'){
    if(hasProjectCookie(request)||request.headers.has('authorization'))throw failure(401,'exchange_grant_only');const data=await body(request,true);
    if(data.grant_type!=='exchange_code'||data.client_id!==CLIENT||!/^[A-Za-z0-9_-]{43}$/.test(data.exchange_code||'')||Object.keys(data).some(k=>!['grant_type','client_id','exchange_code'].includes(k)))throw failure(400,'invalid_grant');
    const record=await query('UPDATE client621_exchange SET used_at=? WHERE code_hash=? AND used_at IS NULL AND expires_at>? RETURNING user_id,parent_session',Date.now(),sha(data.exchange_code),Date.now()).first();if(!record)throw failure(400,'invalid_grant');
    const user=await parent(record.user_id,record.parent_session),accountId=await identity(record.user_id),token='hw621_'+randomBytes(32).toString('base64url'),expires=Math.min(Date.now()+300000,new Date(user.expiresAt).getTime());
    await query('INSERT INTO client621_session(token_hash,user_id,parent_session,expires_at) VALUES(?,?,?,?)',sha(token),record.user_id,record.parent_session,expires).run();
    return json({access_token:token,expires_in:Math.floor((expires-Date.now())/1000),expires_at:new Date(expires).toISOString(),token_type:'bearer',client_id:CLIENT,account_id:accountId,displayName:user.username||user.name,app:'fortnite',in_app_id:accountId,client_service:'fortnite',scope:'profile:read profile:equipment catalog:read account:read',adapter_prototype:true});
   }
   const current=await authorize(request);
   if(path==='/account/api/oauth/verify'&&request.method==='GET')return json({token_type:'bearer',account_id:current.accountId,displayName:current.user.username||current.user.name,client_id:CLIENT,expires_at:new Date(current.expires_at).toISOString()});
   if(path==='/account/api/oauth/sessions/kill'&&['DELETE','POST'].includes(request.method)){await query('DELETE FROM client621_session WHERE token_hash=?',current.token_hash).run();return new Response(null,{status:204,headers});}
   if(path==='/account/api/public/account'&&request.method==='GET'){const ids=url.searchParams.getAll('accountId');if(ids.some(id=>id!==current.accountId))throw failure(403,'account_scope_mismatch');return json([{id:current.accountId,displayName:current.user.username||current.user.name,externalAuths:{}}]);}
   if(path.startsWith('/account/api/public/account/')&&path!=='/account/api/public/account/'+current.accountId)throw failure(403,'account_scope_mismatch');
   if(path==='/account/api/public/account/'+current.accountId&&request.method==='GET')return json({id:current.accountId,displayName:current.user.username||current.user.name,externalAuths:{}});
   const match=/^\/fortnite\/api\/game\/v2\/profile\/([^/]+)\/client\/([^/]+)$/.exec(path);
   if(match&&request.method==='POST'){
    if(match[1]!==current.accountId)throw failure(403,'account_scope_mismatch');const profileId=url.searchParams.get('profileId')||'athena';if(!['athena','common_core','common_public'].includes(profileId))throw failure(403,'profile_not_supported');const rvn=Number(url.searchParams.get('rvn')??-1);if(!Number.isSafeInteger(rvn)||rvn< -1)throw failure(400,'invalid_revision');const data=await body(request);let s=await state(current.user_id);
    if(['QueryProfile','ClientQuestLogin'].includes(match[2]))return json(envelope(current,profileId,s));
    if(match[2]!=='EquipBattleRoyaleCustomization')throw failure(409,'operation_not_implemented');if(profileId!=='athena'||!Object.hasOwn(slots,data.slotName)||typeof data.itemToSlot!=='string')throw failure(400,'invalid_equipment');if(data.variantUpdates!==undefined&&(!Array.isArray(data.variantUpdates)||data.variantUpdates.length))throw failure(409,'variant_metadata_not_validated');
    if(rvn!==s.revision)return json({...envelope(current,profileId,s),errorCode:'errors.hollowwake.profiles.revision_conflict'},409);
    const item=s.items.find(i=>'hw_'+sha(i.cosmetic_id).slice(0,24)===data.itemToSlot);if(data.itemToSlot&&!item)throw failure(403,'item_not_owned_or_unvalidated');if(item&&JSON.parse(item.data).templateId.split(':')[0]!==slots[data.slotName][0])throw failure(400,'wrong_cosmetic_slot');if(!data.itemToSlot&&['Pickaxe','Glider'].includes(data.slotName))throw failure(400,'slot_cannot_be_empty');
    const equipment=JSON.parse(s.equipment),key=slots[data.slotName][1];if(data.slotName==='Dance'){if(!Number.isInteger(data.indexWithinSlot)||data.indexWithinSlot<0||data.indexWithinSlot>5)throw failure(400,'invalid_slot_index');equipment[key]||=['','','','','',''];equipment[key][data.indexWithinSlot]=data.itemToSlot;}else equipment[key]=data.itemToSlot;
    const updated=await query(`UPDATE client621_profile SET equipment=?,revision=revision+1,updated_at=? WHERE user_id=? AND revision=? AND EXISTS(SELECT 1 FROM session WHERE id=? AND expiresAt>?) AND NOT EXISTS(SELECT 1 FROM project_moderation WHERE user_id=? AND banned_until>?) AND (?='' OR EXISTS(SELECT 1 FROM project_inventory i JOIN admin_document d ON d.id=i.cosmetic_id JOIN client621_asset a ON a.template_id=lower(json_extract(d.data,'$.templateId')) AND a.verified=1 AND a.build=? WHERE i.user_id=? AND i.cosmetic_id=? AND d.revision=? AND json_extract(d.data,'$.enabled')=1)) RETURNING *`,JSON.stringify(equipment),Date.now(),current.user_id,s.revision,current.parent_session,Date.now(),current.user_id,Date.now(),data.itemToSlot,BUILD,current.user_id,item?.cosmetic_id||'',item?.metadata_revision||0).first();if(!updated)throw failure(409,'profile_changed_retry');s={...updated,items:s.items};return json(envelope(current,profileId,s,rvn));
   }
   if(path==='/fortnite/api/storefront/v2/catalog'&&request.method==='GET'){
    const definitions=await all("SELECT id,data FROM admin_document WHERE kind='shop' ORDER BY id"),storefronts=[],now=Date.now();let expiration=now+3600000;
    for(const doc of definitions){const shop=JSON.parse(doc.data);if(!shop.enabled||shop.startsAt>now||shop.endsAt<=now)continue;expiration=Math.min(expiration,shop.endsAt);const catalogEntries=[];
     for(const [index,offer]of shop.items.entries()){const cosmetic=await query("SELECT d.data FROM admin_document d JOIN client621_asset a ON a.template_id=lower(json_extract(d.data,'$.templateId')) WHERE d.id=? AND d.kind='cosmetic' AND json_extract(d.data,'$.enabled')=1 AND a.build=? AND a.verified=1",offer.cosmeticId,BUILD).first();if(!cosmetic)continue;const data=JSON.parse(cosmetic.data);catalogEntries.push({offerId:'HW621:'+doc.id+':'+index,devName:data.title,offerType:'StaticPrice',dailyLimit:-1,weeklyLimit:-1,monthlyLimit:-1,categories:[],prices:[{currencyType:'MtxCurrency',currencySubType:'',regularPrice:offer.price,finalPrice:offer.price,saleExpiration:new Date(shop.endsAt).toISOString(),basePrice:offer.price}],itemGrants:[{templateId:data.templateId,quantity:1}],requirements:[],metaInfo:[],displayAssetPath:'',giftInfo:{bIsEnabled:false},refundable:false});}
     storefronts.push({name:'BRDailyStorefront',catalogEntries});
    }return json({refreshIntervalHrs:1,dailyPurchaseHrs:24,expiration:new Date(expiration).toISOString(),storefronts,adapterPrototype:true,purchasesAvailable:false});
   }
   if(/matchmaking|\/game\/v2\/matchmaking|PurchaseCatalogEntry|GiftCatalogEntry/.test(path))return json({errorCode:'errors.hollowwake.feature_unavailable',message:'Matches are not available yet; purchases and gifting are unavailable.'},409);
   throw failure(501,'service_not_implemented');
  }catch(e){return json({errorCode:'errors.hollowwake.'+(e.code||'internal_error'),errorMessage:e.status?e.message:'Adapter request failed',numericErrorCode:e.status||500,originatingService:'hollowwake-client621',intent:'prototype'},e.status||500);}
 }
 return{issue,fetch};
}
