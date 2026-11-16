import mqtt from '../../dashboard/node_modules/mqtt/build/index.js';
import {readFile,writeFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {parsePayload} from '../../dashboard/src/services/protocol.js';
const c=JSON.parse(await readFile(new URL('../../local-test/broker.json',import.meta.url)));
const client=await mqtt.connectAsync(`ws://127.0.0.1:${c.wsPort}/mqtt`,{...c.web,clean:true,reconnectPeriod:0});
const samples=[];
try {
 await new Promise((resolve,reject)=>{
  const timer=setTimeout(()=>reject(Error('No valid live telemetry within 30 seconds')),30000);
  client.on('message',(topic,b)=>{
   if(!topic.endsWith('/telemetry'))return;
   try{
    const p=parsePayload('telemetry',b.toString(),'garage-01');
    assert.equal(p.timeSynced,true);assert.equal(p.simulatedHardware,true);assert.equal(p.actuatorEnabled,false);
    assert(Math.abs(Date.now()-Date.parse(p.timestamp))<15000);
    samples.push(p);
    console.log(JSON.stringify({deviceId:p.deviceId,timestamp:p.timestamp,door:p.door,simulatedHardware:p.simulatedHardware,actuatorEnabled:p.actuatorEnabled}));
    if(samples.length>=2){assert(Date.parse(samples.at(-1).timestamp)>Date.parse(samples[0].timestamp));clearTimeout(timer);resolve();}
   }catch(e){clearTimeout(timer);reject(e);}
  });
  client.subscribe(`${c.prefix}/telemetry`,{qos:0},e=>{if(e){clearTimeout(timer);reject(e);}});
 });
 await writeFile(new URL('../../local-test/live-telemetry.json',import.meta.url),JSON.stringify(samples,null,2));
 console.log('PASS fresh board telemetry over browser WebSocket endpoint and web payload parser');
}finally{await client.endAsync();}
