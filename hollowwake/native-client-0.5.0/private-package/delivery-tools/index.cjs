const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
async function main() {
  const { DefaultArtifactClient } = await import('@actions/artifact');
  const root = path.resolve(process.env.INPUT_DIRECTORY || '');
  const prefix = process.env.INPUT_PREFIX;
  if (!/^[a-z0-9-]{1,80}$/.test(prefix || '')) throw Error('Invalid artifact prefix');
  const metaFile = path.join(root, 'delivery.json');
  const meta = JSON.parse(fs.readFileSync(metaFile, 'utf8'));
  if (meta.format !== 'HollowwakePrivateFullArtifact2' || !Array.isArray(meta.parts) || meta.parts.length > 2048) throw Error('Invalid encrypted delivery metadata');
  const allowed = new Set(['delivery.json', 'delivery-index.json']);
  for (let i = 0; i < meta.parts.length; i++) {
    const p = meta.parts[i];
    if (p.index !== i || p.file !== `part-${String(i).padStart(5, '0')}.hwenc` || p.bytes < 1 || p.bytes > 16 * 1024 * 1024) throw Error('Invalid encrypted part name/size');
    const file = path.join(root, p.file), stat = fs.lstatSync(file);
    if (!stat.isFile() || stat.isSymbolicLink() || stat.size !== p.bytes || crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex') !== p.cipherSha256) throw Error('Encrypted part differs from metadata');
    allowed.add(p.file);
  }
  for (const n of fs.readdirSync(root)) if (!allowed.has(n)) throw Error('Unexpected/plaintext file in encrypted delivery directory');
  const client = new DefaultArtifactClient(), groups = [];
  for (let start = 0; start < meta.parts.length; start += 31) {
    const selected = meta.parts.slice(start, start + 31);
    const name = `${prefix}-p${String(groups.length).padStart(3, '0')}`;
    const result = await client.uploadArtifact(name, selected.map(p => path.join(root, p.file)), root, { compressionLevel: 0, retentionDays: 1 });
    if (!result.id || result.size >= 512 * 1024 * 1024) throw Error('Artifact missing or above delivery ceiling');
    groups.push({ name, id: result.id, bytes: result.size, firstPart: start, lastPart: start + selected.length - 1 });
    console.log(`Uploaded encrypted group ${groups.length}; artifact ${result.id}; bytes ${result.size}`);
  }
  const index = path.join(root, 'delivery-index.json');
  fs.writeFileSync(index, JSON.stringify({ schema: 1, prefix, groups, originalCodeCommit: meta.codeCommit, plaintextGameFilesPublic: false }, null, 2));
  await client.uploadArtifact(`${prefix}-metadata`, [metaFile, index], root, { compressionLevel: 0, retentionDays: 1 });
  console.log(`PASS ${groups.length} bounded encrypted groups and metadata; no plaintext game files or private keys uploaded`);
}
main().catch(e => { console.error(e.message); process.exitCode = 1; });
