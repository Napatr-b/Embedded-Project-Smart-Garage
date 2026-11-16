import {readFileSync as read} from 'node:fs';
import assert from 'node:assert/strict';
const c=JSON.parse(read('local-test/broker.json','utf8'));
const h=read('gateway/include/config.local.h','utf8');
const e=Object.fromEntries(read('dashboard/.env.local','utf8').split(/\r?\n/).filter(x=>x.includes('=')&&!x.startsWith('#')).map(x=>{const i=x.indexOf('=');return [x.slice(0,i),x.slice(i+1)];}));
const m=k=>h.match(new RegExp('#define '+k+' "([^"]*)"'))?.[1];
const checks={
 deviceId:m('GARAGE_DEVICE_ID')===e.VITE_DEVICE_ID,
 boardLanUri:m('GARAGE_MQTT_URI')===`mqtt://${c.host}:${c.mqttPort}`,
 webWebSocket:e.VITE_MQTT_URL===`ws://127.0.0.1:${c.wsPort}/mqtt`,
 topics:['telemetry','status','command','ack','event'].every(t=>e[`VITE_MQTT_${t.toUpperCase()}_TOPIC`]===`${c.prefix}/${t}`),
 boardAuth:m('GARAGE_MQTT_USERNAME')===c.board.username&&m('GARAGE_MQTT_PASSWORD')===c.board.password,
 webAuth:e.VITE_MQTT_USERNAME===c.web.username&&e.VITE_MQTT_PASSWORD===c.web.password,
};
for(const [name,ok] of Object.entries(checks)){console.log(`${ok?'PASS':'FAIL'} ${name}`);assert(ok,name+' mismatch');}
