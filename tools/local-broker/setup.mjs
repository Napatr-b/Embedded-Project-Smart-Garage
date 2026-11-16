import { mkdir, readFile, writeFile, access } from 'node:fs/promises';
import { randomBytes } from 'node:crypto';
import { networkInterfaces } from 'node:os';
const root = new URL('../../', import.meta.url);
const dir = new URL('local-test/', root);
await mkdir(dir, { recursive: true });
const file = new URL('broker.json', dir);
let c;
try { c = JSON.parse(await readFile(file)); }
catch {
  const ips = Object.values(networkInterfaces()).flat().filter(i => i.family === 'IPv4' && !i.internal);
  if (ips.length !== 1) throw new Error('Need one explicitly selected LAN IPv4 address');
  c = { host: ips[0].address, cidr: ips[0].cidr, mqttPort: 1883, wsPort: 8083,
    prefix: 'draft/smart-garage/garage-01',
    board: { username: 'garage-board-test', password: randomBytes(24).toString('hex') },
    web: { username: 'garage-web-test', password: randomBytes(24).toString('hex') } };
  await writeFile(file, JSON.stringify(c, null, 2));
}
const localHeader = new URL('gateway/include/config.local.h', root);
try { await access(localHeader); console.log('Existing config.local.h preserved'); }
catch {
  await writeFile(localHeader, `#pragma once
// LOCAL TEST ONLY: unencrypted MQTT on private LAN; no actuator output.
// Fill these two fields yourself, save, then notify the assistant.
#define GARAGE_WIFI_SSID ""
#define GARAGE_WIFI_PASSWORD ""
#define GARAGE_MQTT_URI "mqtt://${c.host}:${c.mqttPort}"
#define GARAGE_MQTT_USERNAME ${JSON.stringify(c.board.username)}
#define GARAGE_MQTT_PASSWORD ${JSON.stringify(c.board.password)}
#define GARAGE_DEVICE_ID "garage-01"
#define GARAGE_MQTT_CA_PEM ""
#define GARAGE_SNTP_SERVER "pool.ntp.org"
`);
}
const topics = ['telemetry', 'status', 'command', 'ack', 'event'];
const env = ['# LOCAL TEST ONLY; VITE values are visible to browser. Never use production credentials.',
  'VITE_LOCAL_TEST=true', 'VITE_DEVICE_ID=garage-01', `VITE_MQTT_URL=ws://127.0.0.1:${c.wsPort}/mqtt`,
  `VITE_MQTT_USERNAME=${c.web.username}`, `VITE_MQTT_PASSWORD=${c.web.password}`,
  ...topics.map(t=>`VITE_MQTT_${t.toUpperCase()}_TOPIC=${c.prefix}/${t}`)].join('\n')+'\n';
try { await access(new URL('dashboard/.env.local', root)); console.log('Existing dashboard/.env.local preserved'); }
catch { await writeFile(new URL('dashboard/.env.local', root), env); }
console.log(`Prepared local test files. MQTT LAN ${c.host}:${c.mqttPort}; WS loopback ${c.wsPort}. Credentials not logged.`);
