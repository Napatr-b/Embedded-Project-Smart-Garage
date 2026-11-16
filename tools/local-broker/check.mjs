import mqtt from '../../dashboard/node_modules/mqtt/build/index.js';
import { readFile } from 'node:fs/promises';
import assert from 'node:assert/strict';
const c=JSON.parse(await readFile(new URL('../../local-test/broker.json',import.meta.url)));
const clients=[];
async function connect(url, role) {
  const client=await mqtt.connectAsync(url,{...c[role],reconnectPeriod:0,connectTimeout:3000,clean:true});
  clients.push(client); return client;
}
try {
  await assert.rejects(mqtt.connectAsync(`ws://127.0.0.1:${c.wsPort}/mqtt`,{reconnectPeriod:0,connectTimeout:3000}));
  console.log('PASS anonymous connection rejected');
  const board=await connect(`mqtt://${c.host}:${c.mqttPort}`,'board');
  const web=await connect(`ws://127.0.0.1:${c.wsPort}/mqtt`,'web');
  await board.subscribeAsync(`${c.prefix}/command`);
  await assert.rejects(web.subscribeAsync(`${c.prefix}/command`));
  console.log('PASS board TCP + web WS authentication and subscription ACL');
  const received=new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('transport probe timed out')),3000);
    board.once('message',(topic,payload,packet)=>{clearTimeout(timer);resolve({topic,payload:payload.toString(),retain:packet.retain});});
  });
  await web.publishAsync(`${c.prefix}/command`,'{"transportProbe":true}',{qos:0,retain:false});
  const packet=await received;
  assert.equal(packet.payload,'{"transportProbe":true}');assert.equal(packet.retain,false);
  console.log('PASS authenticated WS -> TCP non-retained transport (host clients only; not ESP32)');
} finally { await Promise.all(clients.map(c=>c.endAsync())); }
