import { timingSafeEqual } from 'node:crypto';
import { eventStore } from './supabase.js';

const id = /^[a-zA-Z0-9_-]{1,128}$/;
const iso = /^(\d{4}-\d{2}-\d{2})(T(?:[01]\d|2[0-3]):[0-5]\d:[0-5]\d)(?:\.(\d{1,6}))?(Z|[+-](?:[01]\d|2[0-3]):[0-5]\d)$/;
const eventType = 'OCCUPANCY_CHANGED';

function normalizeTime(value) {
  const match = typeof value === 'string' && iso.exec(value);
  if (!match) return null;
  const [, date, time, fraction = '', zone] = match;
  const midnight = new Date(date);
  const instant = new Date(date + time + zone);
  if (date.startsWith('0000') || !Number.isFinite(+midnight) ||
      !Number.isFinite(+instant) || midnight.toISOString().slice(0, 10) !== date) return null;
  const utc = instant.toISOString();
  if (utc.startsWith('0000') || utc.length !== 24) return null;
  const decimal = fraction.replace(/0+$/, '').padEnd(3, '0');
  return utc.slice(0, 20) + decimal + 'Z';
}

function parseEvent(body, gatewayId) {
  const { event_id, gateway_id, occupied, occurred_at } = body ?? {};
  if (typeof event_id !== 'string' || !id.test(event_id) ||
      gateway_id !== gatewayId || typeof occupied !== 'boolean') return null;
  const timestamp = normalizeTime(occurred_at);
  if (!timestamp) return null;
  return {
    event_id, gateway_id, occurred_at: timestamp,
    event_type: eventType, source: 'ULTRASONIC', state_to: occupied ? 'OCCUPIED' : 'VACANT',
  };
}

function mockStore() {
  const rows = new Map();
  return {
    async insert(event) {
      if (rows.has(event.event_id)) return false;
      rows.set(event.event_id, { ...event, ingested_at: new Date().toISOString() });
      return true;
    },
    async find(eventId) {
      return rows.get(eventId);
    },
    async latest(gatewayId) {
      return [...rows.values()]
        .filter((row) => row.gateway_id === gatewayId && row.event_type === eventType)
        .sort((a, b) => b.occurred_at.slice(0, -1).padEnd(26, '0')
          .localeCompare(a.occurred_at.slice(0, -1).padEnd(26, '0')) ||
          b.ingested_at.localeCompare(a.ingested_at) ||
          (b.event_id > a.event_id ? 1 : -1))[0];
    },
  };
}

async function readBody(req) {
  if (req.body !== undefined) {
    return typeof req.body === 'string' || Buffer.isBuffer(req.body)
      ? JSON.parse(req.body.toString()) : req.body;
  }
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size <= 4096) chunks.push(chunk);
  }
  if (size > 4096) throw new Error('Request too large');
  return JSON.parse(Buffer.concat(chunks).toString());
}

export function parkingHandler(env, { local = false } = {}) {
  const gatewayId = env.PARKING_GATEWAY_ID || 'garage-01';
  const store = local && env.PARKING_MOCK === '1' ? mockStore() : eventStore(env);

  return async (req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('Content-Type', 'application/json; charset=utf-8');
    const reply = (status, body) => {
      res.statusCode = status;
      res.end(JSON.stringify(body));
    };
    try {
      if (req.method === 'GET') {
        const row = await store.latest(gatewayId);
        return reply(200, {
          state: row?.state_to ?? 'UNKNOWN', occurred_at: normalizeTime(row?.occurred_at),
        });
      }
      if (req.method !== 'POST') {
        res.setHeader('Allow', 'GET, POST');
        return reply(405, { error: 'method_not_allowed' });
      }
      if (!env.PARKING_DEVICE_TOKEN) return reply(503, { error: 'device_token_not_configured' });
      const expected = Buffer.from('Bearer ' + env.PARKING_DEVICE_TOKEN);
      const supplied = Buffer.from(req.headers.authorization ?? '');
      if (expected.length !== supplied.length || !timingSafeEqual(expected, supplied)) {
        return reply(401, { error: 'unauthorized' });
      }
      if (req.headers['content-type']?.split(';')[0].trim().toLowerCase() !== 'application/json') {
        return reply(415, { error: 'expected_json' });
      }
      let event;
      try { event = parseEvent(await readBody(req), gatewayId); } catch { /* invalid JSON */ }
      if (!event) return reply(400, { error: 'invalid_event' });

      if (await store.insert(event)) return reply(201, { ok: true, duplicate: false });
      const existing = await store.find(event.event_id);
      if (!existing) throw new Error('Conflicting event could not be read');
      const same = existing.gateway_id === event.gateway_id &&
        existing.event_type === event.event_type && existing.state_to === event.state_to &&
        normalizeTime(existing.occurred_at) === event.occurred_at;
      return same ? reply(200, { ok: true, duplicate: true }) :
        reply(409, { error: 'event_id_conflict' });
    } catch (error) {
      console.error('Parking storage request failed:', error.message);
      return reply(503, { error: 'storage_unavailable' });
    }
  };
}
