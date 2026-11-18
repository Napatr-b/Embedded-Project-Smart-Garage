import assert from 'node:assert/strict';
import { createServer } from 'node:http';
import { test } from 'node:test';
import { parkingHandler } from '../server/parking.js';

async function listen(t, handler) {
  const server = createServer(handler);
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  t.after(() => server.close());
  return `http://127.0.0.1:${server.address().port}`;
}

test('parking validates events, deduplicates retries and keeps the latest event by occurrence time', async (t) => {
  const events = new Map();
  const supabase = await listen(t, async (req, res) => {
    const { pathname, searchParams } = new URL(req.url, 'http://localhost');
    assert.equal(pathname, '/rest/v1/garage_events');
    assert.equal(req.headers.apikey, 'server-test-key');
    assert.equal(req.headers[req.method === 'POST' ? 'content-profile' : 'accept-profile'], 'public');
    let rows;
    if (req.method === 'POST') {
      assert.equal(searchParams.get('on_conflict'), 'event_id');
      assert.equal(req.headers.prefer, 'resolution=ignore-duplicates,return=representation');
      const chunks = [];
      for await (const chunk of req) chunks.push(chunk);
      const event = JSON.parse(Buffer.concat(chunks));
      assert.equal(event.source, 'ULTRASONIC');
      assert.equal(event.event_type, 'OCCUPANCY_CHANGED');
      rows = [];
      if (!events.has(event.event_id)) {
        events.set(event.event_id, { ...event, ingested_at: events.size });
        rows.push({ event_id: event.event_id });
      }
    } else if (searchParams.has('event_id')) {
      const event = events.get(searchParams.get('event_id').slice(3));
      rows = event ? [{ ...event, occurred_at: event.occurred_at.replace('Z', '+00:00') }] : [];
    } else {
      assert.equal(searchParams.get('gateway_id'), 'eq.garage-01');
      assert.equal(searchParams.get('event_type'), 'eq.OCCUPANCY_CHANGED');
      assert.equal(searchParams.get('order'), 'occurred_at.desc,ingested_at.desc,event_id.desc');
      assert.equal(searchParams.get('limit'), '1');
      rows = [...events.values()].filter((event) => event.gateway_id === 'garage-01' &&
        event.event_type === 'OCCUPANCY_CHANGED').sort((a, b) =>
        b.occurred_at.localeCompare(a.occurred_at) || b.ingested_at - a.ingested_at).slice(0, 1);
    }
    res.setHeader('Content-Type', 'application/json');
    res.end(JSON.stringify(rows));
  });
  const env = {
    SUPABASE_URL: supabase, SUPABASE_SECRET_KEY: 'server-test-key',
    PARKING_GATEWAY_ID: 'garage-01', PARKING_DEVICE_TOKEN: 'test-token',
  };
  const first = await listen(t, parkingHandler(env));
  const second = await listen(t, parkingHandler(env));
  const url = first + '/api/parking';
  const read = async () => (await fetch(url)).json();
  const post = async (body, token = 'test-token', base = first) => {
    const response = await fetch(base + '/api/parking', {
      method: 'POST',
      headers: { Authorization: 'Bearer ' + token, 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    return [response.status, await response.json()];
  };

  assert.deepEqual(await read(), { state: 'UNKNOWN', occurred_at: null });
  const entry = { event_id: 'in', gateway_id: 'garage-01', occupied: true,
    occurred_at: '2026-11-18T12:00:00+07:00' };
  assert.equal((await post(entry, 'wrong'))[0], 401);
  assert.equal((await post({ ...entry, occupied: 'true' }))[0], 400);
  assert.equal((await post({ ...entry, occurred_at: '2026-02-30T00:00:00Z' }))[0], 400);
  const writes = await Promise.all([post(entry), post(entry, 'test-token', second)]);
  assert.deepEqual(writes.map(([status]) => status).sort(), [200, 201]);
  assert.deepEqual(await read(), { state: 'OCCUPIED', occurred_at: '2026-11-18T05:00:00.000Z' });
  assert.equal((await post({ ...entry, occupied: false }))[0], 409);
  assert.equal((await post({ ...entry, gateway_id: 'other-gateway' }))[0], 400);
  assert.equal(events.size, 1);

  const exit = { ...entry, event_id: 'out', occupied: false, occurred_at: '2026-11-18T06:00:00Z' };
  assert.deepEqual(await post(exit), [201, { ok: true, duplicate: false }]);
  assert.deepEqual(await post(exit), [200, { ok: true, duplicate: true }]);
  assert.equal((await post({ ...entry, event_id: 'backfill', occurred_at: '2026-11-18T04:00:00Z' }))[0], 201);
  assert.deepEqual(await read(), { state: 'VACANT', occurred_at: '2026-11-18T06:00:00.000Z' });
  assert.equal(events.size, 3);
  assert.equal(events.get('out').state_to, 'VACANT');

  events.set('other', { ...events.get('out'), gateway_id: 'garage-02',
    occurred_at: '2026-11-18T07:00:00.000Z', state_to: 'OCCUPIED' });
  events.set('door', { ...events.get('out'), event_type: 'DOOR_CHANGED',
    occurred_at: '2026-11-18T07:00:00.000Z', state_to: 'OCCUPIED' });
  assert.equal((await read()).state, 'VACANT');
  assert.equal((await post({ ...exit, occurred_at: '2026-11-18T13:00:00+07:00' }))[0], 200);
  assert.equal((await post({ ...exit, occurred_at: '2026-11-18T06:00:00.000001Z' }))[0], 409);
});

test('parking logs Supabase diagnostics without exposing upstream content', async (t) => {
  const secret = 'server-test-key';
  let content = JSON.stringify({
    code: 'PGRST205',
    message: `Unexpected upstream text with ${secret}`,
    details: 'Authorization: Bearer example; face payload',
  });
  const supabase = await listen(t, (_req, res) => {
    res.statusCode = 404;
    res.end(content);
  });
  const parking = await listen(t, parkingHandler({
    SUPABASE_URL: supabase, SUPABASE_SECRET_KEY: secret,
  }));
  const log = t.mock.method(console, 'error', () => {});
  const url = parking + '/api/parking';

  const response = await fetch(url);
  assert.equal(response.status, 503);
  assert.deepEqual(await response.json(), { error: 'storage_unavailable' });
  assert.deepEqual(log.mock.calls[0].arguments, [
    'Parking storage request failed:',
    "Supabase Data API returned HTTP 404 PGRST205: Could not find the table 'public.garage_events' in the schema cache",
  ]);

  content = '<html>Not found</html>';
  assert.equal((await fetch(url)).status, 503);
  assert.deepEqual(log.mock.calls[1].arguments, [
    'Parking storage request failed:',
    'Supabase Data API returned HTTP 404: Request to /rest/v1/garage_events (schema public) failed',
  ]);
});
