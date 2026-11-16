import test from 'node:test';
import assert from 'node:assert/strict';
import { createDemo } from '../src/services/demo.js';
for (const [initial, next] of [
  ['open', 'open'],
  ['open', 'close'],
  ['close', 'close'],
  ['close', 'open'],
]) {
  test(initial + ' halfway ? stop ? ' + next, (t) => {
    t.mock.timers.enable({ apis: ['setInterval'] });
    let time = 0,
      data,
      id = 0;
    const d = createDemo(
      (e) => {
        if (e.kind === 'telemetry') data = e.payload;
      },
      { now: () => time },
    );
    const send = (action) => d.command({ action, commandId: String(++id) });
    const tick = (ms) => {
      time += ms;
      t.mock.timers.tick(ms);
    };
    try {
      if (initial === 'close') {
        send('open');
        tick(2816);
        assert.equal(data.door, 'Open');
      }
      send(initial);
      tick(1400);
      assert.ok(Math.abs(data.doorPosition - 50) < 0.01);
      send('stop');
      const held = data.doorPosition;
      assert.equal(data.door, 'Stopped');
      tick(3000);
      assert.equal(data.doorPosition, held);
      send(next);
      assert.equal(data.doorPosition, held);
      tick(700);
      assert.ok(
        next === 'open' ? data.doorPosition > held : data.doorPosition < held,
      );
      tick(720);
      assert.equal(data.doorPosition, next === 'open' ? 100 : 0);
      assert.equal(data.door, next === 'open' ? 'Open' : 'Closed');
      tick(10000);
      assert.equal(data.doorPosition, next === 'open' ? 100 : 0);
    } finally {
      d.close();
    }
  });
}
test('rapid reversal/repeated commands preserve position and cancel previous motion', (t) => {
  t.mock.timers.enable({ apis: ['setInterval'] });
  let time = 0,
    data,
    events = [];
  const d = createDemo(
    (e) => {
      events.push(e);
      if (e.kind === 'telemetry') data = e.payload;
    },
    { now: () => time },
  );
  try {
    d.command({ action: 'open', commandId: 'first' });
    time = 1000;
    t.mock.timers.tick(1000);
    for (let i = 0; i < 100; i++)
      d.command({ action: i % 2 ? 'open' : 'close', commandId: String(i) });
    assert.ok(Math.abs(data.doorPosition - 1000 / 28) < 0.01);
    time = 4000;
    t.mock.timers.tick(3000);
    assert.equal(data.door, 'Open');
    assert.deepEqual(
      events
        .filter((e) => e.kind === 'ack' && e.payload.status === 'completed')
        .map((e) => e.payload.commandId),
      ['99'],
    );
    d.close();
    const count = events.length;
    time = 20000;
    t.mock.timers.tick(16000);
    assert.equal(events.length, count);
  } finally {
    d.close();
  }
});
