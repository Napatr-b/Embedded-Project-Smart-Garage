import { CONTRACT } from './contract.generated.js';
import mqtt from 'mqtt';
import { parsePayload } from './protocol.js';
export function readConfig(env, pageProtocol = globalThis.location?.protocol || 'http:') {
  const names = ['telemetry', 'status', 'command', 'ack', 'event'];
  const topics = Object.fromEntries(
    names.map((k) => [k, env[`VITE_MQTT_${k.toUpperCase()}_TOPIC`]?.trim()]),
  );
  const deviceId = env.VITE_DEVICE_ID?.trim();
  const url = env.VITE_MQTT_URL?.trim();
  let valid = false;
  try {
    const u = new URL(url);
    const localWs = env.DEV === true && env.VITE_LOCAL_TEST === 'true' &&
      pageProtocol === 'http:' && u.protocol === 'ws:' &&
      ['127.0.0.1', 'localhost'].includes(u.hostname);
    valid = (u.protocol === 'wss:' || localWs) && !u.username && !u.password;
  } catch {
    /* missing */
  }
  if (
    !valid ||
    !/^[a-zA-Z0-9_-]{1,48}$/.test(deviceId || '') ||
    names.some((k) => !topics[k] || /[+#\u0000]/.test(topics[k])) ||
    new Set(Object.values(topics)).size !== 5
  )
    return {
      error:
        'ยังตั้งค่า MQTT ไม่ครบ: ต้องมี deviceId, URL wss:// และ topic แยกกัน 5 ค่า (ไม่ใช้ wildcard)',
    };
  return { url, topics, deviceId,
    username: env.VITE_LOCAL_TEST === 'true' ? env.VITE_MQTT_USERNAME : undefined,
    password: env.VITE_LOCAL_TEST === 'true' ? env.VITE_MQTT_PASSWORD : undefined,
    localTest: env.DEV === true && env.VITE_LOCAL_TEST === 'true',
  };
}
export function createMqtt(emit, config, connect = mqtt.connect) {
  if (config.error) {
    emit({ kind: 'broker', value: 'unconfigured', message: config.error });
    return {
      close() {},
      command() {
        throw new Error(config.error);
      },
    };
  }
  let closed = false;
  const client = connect(config.url, {
    clientId: `garage-web-${crypto.randomUUID()}`,
    clean: true,
    reconnectPeriod: 3000,
    connectTimeout: 10000,
    resubscribe: false,
    queueQoSZero: false,
    username: config.username,
    password: config.password,
  });
  const report = (message) => {
    if (!closed) emit({ kind: 'error', message });
  };
  client.on('connect', () => {
    if (closed) return;
    client.subscribe(
      ['telemetry', 'status', 'ack', 'event'].map((k) => config.topics[k]),
      { qos: 0 },
      (err, grants) => {
        if (closed) return;
        if (err || grants?.some((g) => g.qos === 128)) {
          report('สมัครรับ topic ไม่สำเร็จ');
          emit({ kind: 'broker', value: 'error' });
        } else emit({ kind: 'broker', value: 'connected' });
      },
    );
  });
  client.on(
    'reconnect',
    () => !closed && emit({ kind: 'broker', value: 'reconnecting' }),
  );
  client.on(
    'offline',
    () => !closed && emit({ kind: 'broker', value: 'disconnected' }),
  );
  client.on(
    'close',
    () => !closed && emit({ kind: 'broker', value: 'disconnected' }),
  );
  client.on('error', () => report('MQTT ผิดพลาด ตรวจ URL, TLS และสิทธิ์กับทีม'));
  client.on('message', (topic, bytes, packet) => {
    if (closed) return;
    const kind = ['telemetry', 'status', 'ack', 'event'].find(
      (k) => config.topics[k] === topic,
    );
    if (!kind) return;
    if (kind === 'ack' && packet.retain) return;
    if (bytes.length > 16384) {
      report('ปฏิเสธ payload เกิน 16 KB');
      return;
    }
    try {
      emit({
        kind,
        payload: parsePayload(kind, bytes.toString(), config.deviceId),
      });
    } catch (error) {
      report(`ละเว้น ${kind}: ${error.message}`);
    }
  });
  return {
    command(c) {
      if (!client.connected) throw new Error('Broker ไม่ได้เชื่อมต่อ');
      client.publish(
        config.topics.command,
        JSON.stringify({
          ...c,
          schemaVersion: CONTRACT.schemaVersion,
          deviceId: config.deviceId,
          timeSynced: true,
        }),
        { qos: 0, retain: false },
        (err, grants) => {
          if (closed) return;
          if (err || grants?.some((g) => g.qos === 128))
            report('publish ไม่สำเร็จ • ไม่ส่งซ้ำอัตโนมัติ');
        },
      );
    },
    close() {
      closed = true;
      client.removeAllListeners();
      client.on('error', () => {});
      client.end(true);
    },
  };
}
