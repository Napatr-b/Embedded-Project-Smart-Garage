import mqtt from '../../dashboard/node_modules/mqtt/build/index.js';
import {readFile,writeFile} from 'node:fs/promises';
import assert from 'node:assert/strict';
import {randomUUID} from 'node:crypto';
const c=JSON.parse(await readFile(new URL('../../local-test/broker.json',import.meta.url)));
const client=await mqtt.connectAsync(`ws://127.0.0.1:${c.wsPort}/mqtt`,{...c.web,clean:true,reconnectPeriod:0});
const records=[];
client.on('message',(topic,b)=>{try{const p=JSON.parse(b); records.push({topic,...p});console.log(JSON.stringify({kind:topic.split('/').at(-1),...p}));}catch{}});
await client.subscribeAsync(['telemetry','status','ack','event'].map(t=>`${c.prefix}/${t}`));
const pause=ms=>new Promise(r=>setTimeout(r,ms));
async function wait(predicate,ms=10000){const end=Date.now()+ms;while(Date.now()<end){const p=records.find(predicate);if(p)return p;await pause(50);}throw Error('board response timeout');}
async function send(action){const commandId=randomUUID();await client.publishAsync(`${c.prefix}/command`,JSON.stringify({schemaVersion:2,deviceId:'garage-01',timeSynced:true,timestamp:new Date().toISOString(),commandId,action}),{qos:0,retain:false});await wait(p=>p.commandId===commandId&&p.status==='accepted');return commandId;}
async function complete(action,door){const id=await send(action);await wait(p=>p.commandId===id&&p.status==='completed');return wait(p=>p.commandId===id&&p.door===door);}
try{
 const t=await wait(p=>p.topic.endsWith('/telemetry')&&p.timeSynced&&p.actuatorEnabled===false,30000);
 assert.equal(t.simulatedHardware,true);
 console.log('PASS real gateway telemetry, synced clock, simulated hardware, actuator disabled');
 await complete('open','Open');await pause(5000);
 assert.equal(records.filter(p=>p.topic.endsWith('/telemetry')).at(-1).door,'Open');
 console.log('PASS open remains open');
 await send('close');await pause(1000);const stopped=await complete('stop','Stopped');
 assert(stopped.doorPosition>0&&stopped.doorPosition<100);
 console.log('PASS close then stop at intermediate position');
 await complete('open','Open');console.log('PASS resume open from stopped position');
 await send('close');await pause(900);await complete('stop','Stopped');
 await complete('close','Closed');console.log('PASS resume close from stopped position');
}finally{await writeFile(new URL('../../local-test/board-test.json',import.meta.url),JSON.stringify(records,null,2));await client.endAsync();}
