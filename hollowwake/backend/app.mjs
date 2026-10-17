import {createFortnite621Service} from './fortnite621.mjs';
import { DatabaseSync } from 'node:sqlite';
import { createHmac, randomUUID } from 'node:crypto';
import { mkdirSync, readFileSync, writeFileSync, chmodSync } from 'node:fs';
import { dirname } from 'node:path';
import { createCore } from './core.mjs';
import { createAuth, CLIENT_ID } from './auth.mjs';
import { adminRPC } from './admin.mjs';
import { migrate } from './migrate.mjs';

export async function createApplication(config, { database, sendMail, captureMigration = false } = {}) {
  mkdirSync(dirname(config.dbPath), { recursive: true, mode: 0o700 });
  const db = database || new DatabaseSync(config.dbPath);
  db.exec('PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA busy_timeout=5000;');
  if (!database) chmodSync(config.dbPath, 0o600);
  const localSend = config.local && !config.smtp ? async message => {
    const outbox = `${dirname(config.dbPath)}/outbox`; mkdirSync(outbox, { recursive: true, mode: 0o700 });
    writeFileSync(`${outbox}/${randomUUID()}.json`, JSON.stringify(message), { mode: 0o600 });
  } : undefined;
  const auth = createAuth(config, db, sendMail || localSend);
  await migrate(db, auth, { capture: captureMigration });
  const statement=(sql,args=[])=>({bind(...values){return statement(sql,values);},first:async()=>db.prepare(sql).get(...args),all:async()=>({results:db.prepare(sql).all(...args)}),run:async()=>db.prepare(sql).run(...args),_sql:sql,_args:args});
  const adapter={prepare:sql=>statement(sql),async batch(statements){db.exec('BEGIN IMMEDIATE');try{const results=statements.map(s=>db.prepare(s._sql).run(...s._args));db.exec('COMMIT');return results;}catch(e){db.exec('ROLLBACK');throw e;}}};
  const app=createCore(config, { db, auth, readAsset: name => readFileSync(new URL(`../web/${name}`, import.meta.url)), close: () => db.close(), clientAdapter:createFortnite621Service(adapter,{origin:config.origin,enabled:config.fortnite621AdapterEnabled===true}) });
  const fetch=app.fetch;app.fetch=(request,...args)=>new URL(request.url).pathname==='/api/control/rpc'?adminRPC(request,{DB:adapter,ADMIN_RPC_SECRET:config.adminRpcSecret,MAIL_API_KEY:config.smtp?'configured':undefined}):fetch(request,...args);
  return app;
}
