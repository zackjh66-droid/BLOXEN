import { DatabaseSync } from 'node:sqlite';
import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname } from 'node:path';
import { pathToFileURL } from 'node:url';
import { getMigrations } from 'better-auth/db/migration';
import { readConfig } from './config.mjs';
import { createAuth } from './auth.mjs';
export async function migrate(database, auth, { capture = false } = {}) {
  const migrations = await getMigrations(auth.options);
  if (capture) writeFileSync(new URL('./migrations/001-better-auth.sql', import.meta.url), '-- Generated from pinned Better Auth 1.7.10; do not hand-edit.\n' + await migrations.compileMigrations() + '\n');
  await migrations.runMigrations();
  database.exec(readFileSync(new URL('./migrations/002-project.sql', import.meta.url), 'utf8'));
  database.exec(readFileSync(new URL('./migrations/003-consumed-codes.sql', import.meta.url), 'utf8'));
  database.exec(readFileSync(new URL('./migrations/004-admin.sql', import.meta.url), 'utf8'));
  database.exec(readFileSync(new URL('./migrations/005-client621.sql', import.meta.url), 'utf8'));
}
if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) {
  const config = readConfig(); mkdirSync(dirname(config.dbPath), { recursive: true, mode: 0o700 });
  const db = new DatabaseSync(config.dbPath); const auth = createAuth(config, db);
  await migrate(db, auth, { capture: process.argv.includes('--capture') });
  db.close(); console.log('Database migrations complete.');
}
