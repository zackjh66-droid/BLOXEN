'use strict';
const path = require('path');
const root = path.join(__dirname, '..', '..');
module.exports = {
  root,
  host: process.env.BLOXEN_HOST || '127.0.0.1',            // local-only by default
  webPort: +(process.env.BLOXEN_WEB_PORT || 8080),
  compatPort: +(process.env.BLOXEN_COMPAT_PORT || 8081),
  dbPath: process.env.BLOXEN_DB || path.join(root, 'data', 'bloxen.sqlite'),
  storeDir: process.env.BLOXEN_STORE || path.join(root, 'preservation', 'store'),
  assetDir: process.env.BLOXEN_ASSETS || path.join(root, 'data', 'assets'),
  manifests: path.join(root, 'preservation', 'manifests'),
  catalogDir: path.join(root, 'preservation', 'catalog'),
  startingRobux: 100000, // BLOXEN local play allowance. NOT historical behaviour; documented in STATUS.md.
  clientVersion: 'version-0d46087630eb46cd', clientBuild: '0.205.0.61876',
  clientExeSha256: '384a4cb38de6977899e09e59c2136619fef521dc7b4adeb34d404945120c8a44',
  launchTicketTtlMs: 60_000, joinTokenTtlMs: 60_000, sessionTtlMs: 14 * 24 * 3600 * 1000,
};
