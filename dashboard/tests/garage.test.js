import test from 'node:test';
import assert from 'node:assert/strict';
import {
  CommandTracker,
  acceptTelemetry,
  parsePayload,
  fresh,
  canCommand,
  COMMAND_TIMEOUT_MS,
} from '../src/services/protocol.js';
import { readConfig } from '../src/services/mqtt.js';
import { createDemo } from '../src/services/demo.js';
const packet = (p) =>
  JSON.stringify({
    schemaVersion: 2,
    deviceId: 'garage-01',
    timeSynced: true,
    simulatedHardware: false,
    ...p,
  });
const timestamp = new Date().toISOString();
test('JSON เสีย, null, timestamp หาย, ชนิดผิด ต้องถูกปฏิเสธ', () => {
  for (const raw of [
    '{bad',
    'null',
    '[]',
    '{}',
    packet({ timestamp, humidity: '70' }),
    packet({ timestamp, door: 'Flying' }),
    packet({ timestamp, occupied: 1 }),
    packet({ timestamp, temperature: 999 }),
  ])
    assert.throws(() => parsePayload('telemetry', raw));
});
test('field ขาดต้องไม่มีข้อมูล ไม่สร้างค่าจากอากาศ', () => {
  const p = parsePayload('telemetry', packet({ timestamp, temperature: 30 }));
  assert.equal(p.temperature, 30);
  assert.equal(p.door, null);
  assert.equal(p.eco2Ppm, null);
});
test('timestamp ต้องมีเขตเวลา, ack ต้องมี commandId ที่ถูกต้อง', () => {
  assert.throws(() =>
    parsePayload(
      'status',
      packet({ timestamp: '2026-11-16T10:00:00', online: true }),
    ),
  );
  assert.throws(() =>
    parsePayload('ack', packet({ timestamp, status: 'completed' })),
  );
});
test('ข้อมูลเก่าหรือเวลาจากอนาคตมากเกินไปไม่อนุญาตคำสั่ง', () => {
  const now = 100000,
    s = { broker: 'connected', online: true, statusAt: now, telemetryAt: now };
  assert.equal(canCommand(s, now), true);
  for (const patch of [
    { broker: 'disconnected' },
    { online: false },
    { statusAt: now - 15001 },
    { telemetryAt: now - 15001 },
    { telemetryAt: null },
    { telemetryAt: now + 5001 },
  ])
    assert.equal(canCommand({ ...s, ...patch }, now), false);
  assert.equal(fresh(now - 15000, now), true);
});
test('publish หรือ ack accepted ไม่ใช่ completed; timeout ไม่ retry', () => {
  const c = new CommandTracker();
  c.start('open', 0, 'one');
  assert.equal(c.pending.action, 'open');
  assert.equal(c.ack({ commandId: 'one', status: 'accepted' }), null);
  assert.ok(c.pending);
  assert.equal(c.ack({ commandId: 'other', status: 'completed' }), null);
  assert.ok(c.pending);
  assert.equal(c.expire(COMMAND_TIMEOUT_MS - 1), false);
  assert.equal(c.expire(COMMAND_TIMEOUT_MS), true);
  assert.equal(c.pending, null);
  assert.equal(c.expire(COMMAND_TIMEOUT_MS + 1), false);
});
test('เฉพาะ final state พร้อม commandId ที่ตรงกันยืนยันผลได้', () => {
  const c = new CommandTracker();
  c.start('open', 0, 'one');
  assert.equal(c.state({ door: 'Open' }), null);
  assert.equal(c.state({ door: 'Opening', commandId: 'one' }), null);
  assert.equal(
    c.state({ door: 'Open', positionConfirmed: true, commandId: 'one' }),
    null,
  );
  assert.equal(c.ack({ commandId: 'one', status: 'accepted' }), 'completed');
  assert.equal(c.pending, null);
});
test('Stop แทนคำสั่ง pending ได้ และ ack เก่าต้องไม่ล้าง Stop', () => {
  const c = new CommandTracker();
  c.start('open', 0, 'one');
  c.start('close', 1, 'two');
  c.start('stop', 2, 'stop');
  c.ack({ commandId: 'one', status: 'completed' });
  assert.equal(c.pending.commandId, 'stop');
  assert.equal(c.ack({ commandId: 'stop', status: 'completed' }), null);
  assert.ok(c.pending);
  assert.equal(c.state({ door: 'Stopped', commandId: 'stop' }), 'completed');
});
test('MQTT config ต้อง WSS และ topic แยก ไม่ใส่ credential ใน URL', () => {
  assert.ok(readConfig({}).error);
  const env = {
    VITE_MQTT_URL: 'wss://example.invalid/mqtt',
    VITE_DEVICE_ID: 'garage-01',
  };
  for (const k of ['TELEMETRY', 'STATUS', 'COMMAND', 'ACK', 'EVENT'])
    env[`VITE_MQTT_${k}_TOPIC`] = `draft/${k}`;
  assert.equal(readConfig(env).error, undefined);
  assert.ok(
    readConfig({ ...env, VITE_MQTT_URL: 'mqtt://example.invalid' }).error,
  );
  assert.ok(
    readConfig({ ...env, VITE_MQTT_URL: 'wss://user:secret@example.invalid' })
      .error,
  );
  assert.ok(
    readConfig({ ...env, VITE_MQTT_COMMAND_TOPIC: 'draft/STATUS' }).error,
  );
});
test('local WS ต้องเปิด test flag ใน dev และห้ามใช้จาก HTTPS หรือเครื่องอื่น', () => {
  const env = { DEV: true, VITE_LOCAL_TEST: 'true', VITE_DEVICE_ID: 'garage-01', VITE_MQTT_URL: 'ws://127.0.0.1:8083/mqtt' };
  for (const k of ['TELEMETRY', 'STATUS', 'COMMAND', 'ACK', 'EVENT']) env[`VITE_MQTT_${k}_TOPIC`] = `draft/${k}`;
  assert.equal(readConfig(env, 'http:').error, undefined);
  assert.ok(readConfig(env, 'https:').error);
  assert.ok(readConfig({ ...env, DEV: false }, 'http:').error);
  assert.ok(readConfig({ ...env, VITE_LOCAL_TEST: 'false' }, 'http:').error);
  assert.ok(readConfig({ ...env, VITE_MQTT_URL: 'ws://example.invalid/mqtt' }, 'http:').error);
});

test('demo เปิดแล้วเปลี่ยนสถานะ, หยุดยกเลิกการเคลื่อนและไม่กลับไป Open', async () => {
  const events = [],
    d = createDemo((e) => events.push(e));
  try {
    d.command({ action: 'open', commandId: '1' });
    assert.equal(
      events.filter((e) => e.kind === 'telemetry').at(-1).payload.door,
      'Opening',
    );
    await new Promise((r) => setTimeout(r, 2900));
    assert.equal(
      events.filter((e) => e.kind === 'telemetry').at(-1).payload.door,
      'Open',
    );
    d.command({ action: 'close', commandId: '2' });
    d.command({ action: 'stop', commandId: '3' });
    await new Promise((r) => setTimeout(r, 2900));
    assert.equal(
      events.filter((e) => e.kind === 'telemetry').at(-1).payload.door,
      'Stopped',
    );
    d.simulate('fault');
    assert.equal(
      events.filter((e) => e.kind === 'telemetry').at(-1).payload.door,
      'Fault',
    );
    d.simulate('offline');
    assert.equal(
      events.filter((e) => e.kind === 'status').at(-1).payload.online,
      false,
    );
  } finally {
    d.close();
  }
});
import { EventEmitter } from 'node:events';
import { createMqtt } from '../src/services/mqtt.js';
test('MQTT publish ไม่ retain, reconnect subscribe ใหม่ และ cleanup connection', () => {
  const c = new EventEmitter();
  c.connected = true;
  const subscriptions = [],
    published = [],
    states = [];
  let ended = false,
    options;
  c.subscribe = (topics, opts, callback) => {
    subscriptions.push(topics);
    callback(
      null,
      topics.map((topic) => ({ topic, qos: 0 })),
    );
  };
  c.publish = (...args) => published.push(args);
  c.end = (force) => {
    ended = force;
  };
  const topics = {
    telemetry: 't',
    status: 's',
    command: 'c',
    ack: 'a',
    event: 'e',
  };
  const service = createMqtt(
    (e) => states.push(e),
    { url: 'wss://example.invalid', topics },
    (url, opts) => {
      options = opts;
      return c;
    },
  );
  c.emit('connect');
  c.emit('reconnect');
  c.emit('connect');
  assert.equal(subscriptions.length, 2);
  assert.deepEqual(subscriptions[0], ['t', 's', 'a', 'e']);
  assert.equal(options.queueQoSZero, false);
  assert.equal(options.clean, true);
  service.command({ action: 'open', commandId: 'x', timestamp });
  assert.equal(published.length, 1);
  assert.deepEqual(published[0][2], { qos: 0, retain: false });
  c.emit('message', 't', Buffer.from('{bad'), { retain: false });
  assert.equal(states.at(-1).kind, 'error');
  const count = states.length;
  c.emit(
    'message',
    'a',
    Buffer.from(packet({ timestamp, commandId: 'x', status: 'completed' })),
    { retain: true },
  );
  assert.equal(states.length, count);
  c.connected = false;
  assert.throws(() => service.command({ action: 'close' }));
  assert.equal(published.length, 1);
  service.close();
  assert.equal(ended, true);
  assert.deepEqual(c.eventNames(), ['error']);
});
test('MQTT ปฏิเสธ subscription ต้องไม่แสดง connected', () => {
  const c = new EventEmitter();
  c.subscribe = (topics, opts, cb) => cb(null, [{ qos: 128 }]);
  c.end = () => {};
  const states = [];
  const service = createMqtt(
    (e) => states.push(e),
    {
      url: 'wss://example.invalid',
      topics: {
        telemetry: 't',
        status: 's',
        ack: 'a',
        event: 'e',
        command: 'c',
      },
    },
    () => c,
  );
  c.emit('connect');
  assert.equal(
    states.some((e) => e.value === 'connected'),
    false,
  );
  assert.equal(states.at(-1).value, 'error');
  service.close();
});
import fs from 'node:fs';
const f = JSON.parse(
  fs.readFileSync(new URL('./contract-fixtures.json', import.meta.url), 'utf8'),
);
test('ร่าง v2: fixture, device identity, LWT และ received ack', () => {
  for (const kind of ['telemetry', 'status', 'command', 'ack', 'event'])
    assert.doesNotThrow(() =>
      parsePayload(kind, JSON.stringify(f[kind]), 'garage-01'),
    );
  assert.equal(
    parsePayload('status', JSON.stringify(f.lastWill), 'garage-01').online,
    false,
  );
  assert.throws(() =>
    parsePayload('telemetry', JSON.stringify(f.telemetry), 'other-device'),
  );
  assert.throws(() =>
    parsePayload(
      'telemetry',
      JSON.stringify({ ...f.telemetry, schemaVersion: 1 }),
    ),
  );
  assert.throws(() =>
    parsePayload(
      'telemetry',
      JSON.stringify({ ...f.telemetry, timeSynced: false }),
    ),
  );
  const c = new CommandTracker();
  c.start('open', Date.now(), f.ack.commandId);
  assert.equal(c.ack(f.ack), null);
  assert.ok(c.pending);
  assert.equal(c.pending.received, true);
});

test('completed ack อย่างเดียวไม่ยืนยันตำแหน่งประตู', () => {
  const c = new CommandTracker();
  c.start('open', 0, 'ack-only');
  assert.equal(c.ack({ commandId: 'ack-only', status: 'completed' }), null);
  assert.ok(c.pending);
  assert.equal(c.state({ door: 'Opening', commandId: 'ack-only' }), null);
  assert.equal(
    c.state({ door: 'Open', positionConfirmed: true, commandId: 'ack-only' }),
    'completed',
  );
});

test('ไม่ถือว่ามุม servo หรือ door field ที่ไม่มี feedback เป็นตำแหน่งยืนยัน', () => {
  const p = parsePayload(
    'telemetry',
    packet({
      timestamp,
      door: 'Open',
      positionConfirmed: false,
      commandedServoAngle: 90,
    }),
  );
  assert.equal(p.door, null);
  assert.equal(p.commandedServoAngle, 90);
  assert.throws(() =>
    parsePayload('command', packet({ ...f.command, timeSynced: false })),
  );
});

test('packet ซ้ำไม่เพิ่มกราฟ แต่ Stop ที่เวลาเท่ากันต้องไม่ถูกทิ้ง', () => {
  const now = Date.now();
  const opening = {
    timestamp: new Date(now).toISOString(),
    door: 'Opening',
    commandId: 'open',
  };
  assert.equal(acceptTelemetry({ ...opening }, opening, now), false);
  assert.equal(
    acceptTelemetry(
      { ...opening, door: 'Stopped', commandId: 'stop' },
      opening,
      now,
    ),
    true,
  );
  assert.equal(
    acceptTelemetry(
      { ...opening, timestamp: new Date(now - 1).toISOString() },
      opening,
      now,
    ),
    false,
  );
});

test('online:false ใน telemetry ต้องไม่ใช้หลบเงื่อนไข timeSynced', () => {
  assert.throws(() =>
    parsePayload(
      'telemetry',
      packet({ timestamp, timeSynced: false, online: false }),
    ),
  );
});
