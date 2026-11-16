import { CONTRACT } from './contract.generated.js';
export const STALE_MS = CONTRACT.staleMs;
export const COMMAND_TIMEOUT_MS = CONTRACT.commandTtlMs;
export const DOORS = [
  'Closed',
  'Opening',
  'Open',
  'Closing',
  'Stopped',
  'Fault',
];
export function fresh(timestamp, now = Date.now()) {
  return (
    Number.isFinite(timestamp) &&
    now - timestamp <= STALE_MS &&
    timestamp <= now + 5000
  );
}
export function canCommand(s, now = Date.now()) {
  return (
    s.broker === 'connected' &&
    s.online &&
    fresh(s.statusAt, now) &&
    fresh(s.telemetryAt, now)
  );
}
const timestamp = (value) =>
  typeof value === 'string' &&
  /(?:Z|[+-]\d\d:\d\d)$/.test(value) &&
  Number.isFinite(Date.parse(value));
const number = (v, min, max) =>
  v === undefined ||
  v === null ||
  (typeof v === 'number' && Number.isFinite(v) && v >= min && v <= max);
export function parsePayload(kind, raw, expectedDeviceId) {
  let p;
  try {
    p = JSON.parse(raw);
  } catch {
    throw new Error('JSON ไม่ถูกต้อง');
  }
  if (
    !p ||
    Array.isArray(p) ||
    typeof p !== 'object' ||
    (!timestamp(p.timestamp) &&
      !(kind === 'status' && p.online === false && p.timestamp === null))
  )
    throw new Error('ต้องมี timestamp ISO 8601 พร้อมเขตเวลา');
  if (
    p.schemaVersion !== CONTRACT.schemaVersion ||
    typeof p.deviceId !== 'string' ||
    !/^[a-zA-Z0-9_-]{1,48}$/.test(p.deviceId) ||
    (expectedDeviceId && p.deviceId !== expectedDeviceId)
  )
    throw new Error('schemaVersion หรือ deviceId ไม่ตรงกับร่าง');
  if (
    kind !== 'command' &&
    (typeof p.timeSynced !== 'boolean' ||
      (!(kind === 'status' && p.online === false) && p.timeSynced !== true))
  )
    throw new Error('เวลาของ Gateway ยังไม่ sync');
  if (
    kind !== 'command' &&
    p.simulatedHardware != null &&
    typeof p.simulatedHardware !== 'boolean'
  )
    throw new Error('simulatedHardware ต้องเป็น boolean');
  if (
    p.commandId != null &&
    (typeof p.commandId !== 'string' ||
      !p.commandId.length ||
      p.commandId.length > 128)
  )
    throw new Error('commandId ไม่ถูกต้อง');
  if (
    kind === 'command' &&
    (!['open', 'close', 'stop'].includes(p.action) ||
      !p.commandId ||
      !/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/.test(p.timestamp))
  )
    throw new Error('command ไม่ถูกต้อง');
  if (kind === 'command' && p.timeSynced !== true)
    throw new Error('command ต้องมีเวลาที่ sync แล้ว');
  if (kind === 'telemetry' && typeof p.simulatedHardware !== 'boolean')
    throw new Error('ต้องระบุ simulatedHardware');
  if (kind === 'telemetry') {
    if (['Closed', 'Open'].includes(p.door) && p.positionConfirmed !== true)
      p.door = null;
    if (p.door !== undefined && p.door !== null && !DOORS.includes(p.door))
      throw new Error('สถานะประตูไม่ถูกต้อง');
    for (const [k, min, max] of [
      ['doorPosition', 0, 100],
      ['temperature', -40, 80],
      ['humidity', 0, 100],
      ['ldrRaw', 0, 65535],
      ['distanceCm', 0, 1000],
      ['aqi', 1, 5],
      ['tvocPpb', 0, 1e7],
      ['eco2Ppm', 0, 1e7],
    ])
      if (!number(p[k], min, max)) throw new Error(`ค่า ${k} ไม่ถูกต้อง`);
    for (const k of [
      'occupied',
      'ledOn',
      'positionConfirmed',
      'actuatorEnabled',
    ])
      if (p[k] != null && typeof p[k] !== 'boolean')
        throw new Error(`ค่า ${k} ต้องเป็น boolean`);
    return Object.fromEntries(
      [
        'timestamp',
        'deviceId',
        'schemaVersion',
        'timeSynced',
        'simulatedHardware',
        'positionConfirmed',
        'actuatorEnabled',
        'commandedServoAngle',
        'door',
        'doorPosition',
        'temperature',
        'humidity',
        'ldrRaw',
        'distanceCm',
        'aqi',
        'tvocPpb',
        'eco2Ppm',
        'occupied',
        'ledOn',
        'commandId',
      ].map((k) => [k, p[k] ?? null]),
    );
  }
  if (kind === 'status' && typeof p.online !== 'boolean')
    throw new Error('status ต้องมี online');
  if (
    kind === 'ack' &&
    (typeof p.commandId !== 'string' ||
      !['received', 'accepted', 'completed', 'rejected'].includes(p.status))
  )
    throw new Error('ack ไม่ถูกต้อง');
  if (
    kind === 'event' &&
    (typeof p.type !== 'string' || typeof p.message !== 'string')
  )
    throw new Error('event ไม่ถูกต้อง');
  return p;
}
export function acceptTelemetry(p, previous, now = Date.now()) {
  const t = Date.parse(p.timestamp),
    old = previous?.timestamp ? Date.parse(previous.timestamp) : -Infinity;
  return (
    fresh(t, now) &&
    t >= old &&
    !(t === old && JSON.stringify(p) === JSON.stringify(previous))
  );
}

export class CommandTracker {
  pending = null;
  start(action, now = Date.now(), id = globalThis.crypto.randomUUID()) {
    return (this.pending = {
      commandId: id,
      action,
      startedAt: now,
      accepted: false,
    });
  }
  finish() {
    const p = this.pending;
    if (
      p &&
      p.stateConfirmed &&
      (p.accepted || p.completedAck) &&
      (p.action !== 'stop' || p.completedAck)
    ) {
      this.pending = null;
      return 'completed';
    }
    return null;
  }
  ack(p) {
    if (!this.pending || p.commandId !== this.pending.commandId) return null;
    if (p.status === 'rejected') {
      this.pending = null;
      return 'rejected';
    }
    if (p.status === 'received') this.pending.received = true;
    if (p.status === 'accepted') this.pending.accepted = true;
    if (p.status === 'completed') this.pending.completedAck = true;
    return this.finish();
  }
  state(p) {
    const c = this.pending;
    if (
      c &&
      p.commandId === c.commandId &&
      ((c.action === 'open' &&
        p.door === 'Open' &&
        p.positionConfirmed === true) ||
        (c.action === 'close' &&
          p.door === 'Closed' &&
          p.positionConfirmed === true) ||
        (c.action === 'stop' && ['Stopped', 'Closed', 'Open'].includes(p.door)))
    )
      this.pending.stateConfirmed = true;
    return this.finish();
  }
  expire(now = Date.now()) {
    if (this.pending && now - this.pending.startedAt >= COMMAND_TIMEOUT_MS) {
      this.pending = null;
      return true;
    }
    return false;
  }
}
