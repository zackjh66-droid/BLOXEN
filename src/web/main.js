'use strict';
// Starts the website + compat backend (both loopback-only by default) and the on-demand game servers.
const http = require('http'); const cfg = require('../lib/config'); const { open, seed } = require('../lib/db'); const { createWebApp } = require('./app'); const { GameServerManager } = require('../lib/gameservers'); const { createCompatApp } = require('../compat/server');
async function start({ dbPath = cfg.dbPath, webPort = cfg.webPort, compatPort = cfg.compatPort, host = cfg.host, storeDir = cfg.storeDir, quiet = false } = {}) {
  if (host !== '127.0.0.1' && host !== '::1' && process.env.BLOXEN_ALLOW_REMOTE !== '1') throw new Error('Refusing to listen on a non-loopback address. Set BLOXEN_ALLOW_REMOTE=1 only if you understand the exposure.');
  const log = m => { if (!quiet) console.log(new Date().toISOString(), m); };
  const db = open(dbPath); const s = seed(db); log(`seeded ${s.items} catalog items, ${s.places} place records`);
  const gameservers = new GameServerManager(db, { logger: log, storeDir });
  const web = http.createServer(createWebApp({ db, gameservers, log })); const compat = http.createServer(createCompatApp({ db, gameservers, log, compatBase: () => `http://${host}:${compat.address().port}` }));
  await new Promise(r => web.listen(webPort, host, r)); await new Promise(r => compat.listen(compatPort, host, r));
  log(`website http://${host}:${web.address().port}  compat http://${host}:${compat.address().port}`);
  return { db, web, compat, gameservers, webUrl: `http://${host}:${web.address().port}`, compatUrl: `http://${host}:${compat.address().port}`, close: () => { web.close(); compat.close(); gameservers.closeAll(); db.close(); } };
}
module.exports = { start };
if (require.main === module) start().catch(e => { console.error(e.message); process.exit(1); });
