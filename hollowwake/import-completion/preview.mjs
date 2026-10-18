import {Miniflare,Log,LogLevel,convertV4MiniflareOptions} from '/workspace/sites/hollowwake/node_modules/miniflare/dist/src/index.js';
import {readFile,mkdtemp} from 'node:fs/promises';
import {tmpdir} from 'node:os';import {join} from 'node:path';import {randomBytes} from 'node:crypto';
const directory='/workspace/hollowwake/import-completion/sites-candidate';
const mf=new Miniflare(convertV4MiniflareOptions({scriptPath:directory+'/dist/server/index.js',modules:true,compatibilityDate:'2026-05-15',compatibilityFlags:['nodejs_compat'],cf:false,log:new Log(LogLevel.NONE),d1Databases:{DB:'hollowwake-preview'},resourcePersistencePath:await mkdtemp(join(tmpdir(),'hw-home-preview-')),bindings:{PUBLIC_ORIGIN:'http://127.0.0.1:8793',LOCAL_TEST:'1',BETTER_AUTH_SECRET:randomBytes(48).toString('hex')},port:8793,host:'127.0.0.1'}));
await mf.ready;const db=await mf.getD1Database('DB');const sql=(await readFile(directory+'/drizzle/0000_mean_black_cat.sql','utf8'))+'--> statement-breakpoint'+(await readFile(directory+'/drizzle/0001_clever_kylun.sql','utf8'));await db.batch(sql.split('--> statement-breakpoint').map(x=>x.trim()).filter(Boolean).map(x=>db.prepare(x)));
console.log('Offline candidate preview http://127.0.0.1:8793 — disposable test DB only');
process.on('SIGINT',async()=>{await mf.dispose();process.exit(0)});
