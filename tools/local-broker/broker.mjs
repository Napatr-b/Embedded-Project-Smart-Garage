import aedes from 'aedes';
import websocket from 'websocket-stream';
import net from 'node:net';
import http from 'node:http';
import { readFile } from 'node:fs/promises';
import { timingSafeEqual } from 'node:crypto';
const c = JSON.parse(await readFile(new URL('../../local-test/broker.json', import.meta.url)));
const same = (a,b) => { const x=Buffer.from(a||''),y=Buffer.from(b||''); return x.length===y.length && timingSafeEqual(x,y); };
const broker = aedes({ heartbeatInterval: 3000 });
broker.authenticate = (client,user,password,cb) => {
  const role = ['board','web'].find(r=>same(user,c[r].username)&&same(password,c[r].password));
  client.garageRole = role;
  cb(null, !!role);
};
const readTopics = ['telemetry','status','ack','event'].map(t=>`${c.prefix}/${t}`);
const command = `${c.prefix}/command`;
broker.authorizeSubscribe = (client,sub,cb) => cb(null,
  (client.garageRole==='board' ? sub.topic===command : readTopics.includes(sub.topic)) ? sub : null);
broker.authorizePublish = (client,p,cb) => {
  const ok = client?.garageRole==='board' ? readTopics.includes(p.topic) :
    client?.garageRole==='web' && p.topic===command && !p.retain && p.qos===0;
  cb(ok ? null : new Error('Topic ACL denied'));
};
broker.on('clientError',()=>console.log('Client error (details redacted)'));
broker.on('connectionError',()=>console.log('Connection rejected (details redacted)'));
broker.on('clientReady',client=>console.log(`Authenticated ${client.garageRole} client`));
const ipv4 = ip => ip.split('.').reduce((n,v)=>(n<<8)+Number(v),0)>>>0;
const [network,bits] = c.cidr.split('/');
const mask = (0xffffffff << (32-Number(bits)))>>>0;
const tcp = net.createServer(socket=> {
  const ip=socket.remoteAddress?.replace(/^::ffff:/,'');
  if (!net.isIPv4(ip||'') || (ipv4(ip)&mask)!==(ipv4(network)&mask)) { socket.destroy(); return; }
  broker.handle(socket);
});
const ws = http.createServer((req,res)=>{res.writeHead(404);res.end();});
websocket.createServer({server:ws, path:'/mqtt', perMessageDeflate:false, maxPayload:16384},broker.handle);
for (const server of [tcp,ws]) server.on('error',err=>{console.error(`Listener failed: ${err.code}`);process.exit(1);});
tcp.listen(c.mqttPort,c.host,()=>console.log(`LOCAL TEST: unencrypted MQTT bound to LAN ${c.host}:${c.mqttPort}; subnet restricted; auth + ACL required`));
ws.listen(c.wsPort,'127.0.0.1',()=>console.log(`LOCAL TEST: WS bound to 127.0.0.1:${c.wsPort}; no router/firewall changes`));
