import http from 'node:http';
import { readConfig } from './config.mjs';
import { createApplication } from './app.mjs';
const config = readConfig();
const app = await createApplication(config);
const server = http.createServer(async (incoming, outgoing) => {
  try {
    let size = 0; const chunks = [];
    for await (const chunk of incoming) { size += chunk.length; if (size > 16384) { outgoing.writeHead(413); outgoing.end(); return; } chunks.push(chunk); }
    const headers = new Headers();
    for (const [key, value] of Object.entries(incoming.headers)) if (value !== undefined) headers.set(key, Array.isArray(value) ? value.join(', ') : value);
    const request = new Request(new URL(incoming.url, config.origin), { method: incoming.method, headers,
      ...(['GET', 'HEAD'].includes(incoming.method) ? {} : { body: Buffer.concat(chunks) }) });
    const ip = config.trustProxy ? (incoming.headers['x-forwarded-for'] || incoming.socket.remoteAddress).split(',')[0].trim() : incoming.socket.remoteAddress;
    const response = await app.fetch(request, ip);
    const resultHeaders = Object.fromEntries(response.headers); delete resultHeaders['set-cookie'];
    const cookies = response.headers.getSetCookie(); if (cookies.length) resultHeaders['set-cookie'] = cookies;
    outgoing.writeHead(response.status, resultHeaders); outgoing.end(Buffer.from(await response.arrayBuffer()));
  } catch { outgoing.writeHead(500, { 'Cache-Control': 'no-store' }); outgoing.end('Request failed'); }
});
server.requestTimeout = 15000; server.headersTimeout = 10000;
server.listen(config.port, config.host, () => console.log(`Hollowwake companion listening at ${config.origin}; Fortnite port unavailable.`));
for (const signal of ['SIGINT', 'SIGTERM']) process.on(signal, () => server.close(() => { app.close(); process.exit(0); }));
