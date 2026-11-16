export function createDemo(emit, { now = () => performance.now() } = {}) {
  let alive = true,
    online = true,
    fault = false,
    frozen = false,
    movement = null,
    tick = 0;
  let lastMove = 0,
    activeCommand = null;
  function advance() {
    const time = now();
    const direction =
      data.door === 'Opening' ? 1 : data.door === 'Closing' ? -1 : 0;
    data.doorPosition = Math.max(
      0,
      Math.min(100, data.doorPosition + (direction * (time - lastMove)) / 28),
    );
    lastMove = time;
  }
  let data = {
    door: 'Closed',
    doorPosition: 0,
    positionConfirmed: true,
    simulatedHardware: true,
    occupied: false,
    distanceCm: 180,
    temperature: 29.4,
    humidity: 63,
    ldrRaw: 1820,
    ledOn: false,
    aqi: 2,
    tvocPpb: 125,
    eco2Ppm: 540,
  };
  function push() {
    if (alive && online && !frozen)
      emit({
        kind: 'telemetry',
        payload: { ...data, timestamp: new Date().toISOString() },
      });
  }
  function status() {
    emit({
      kind: 'status',
      payload: { online, timestamp: new Date().toISOString() },
    });
  }
  function event(type, message) {
    emit({
      kind: 'event',
      payload: { type, message, timestamp: new Date().toISOString() },
    });
  }
  emit({ kind: 'broker', value: 'connected' });
  status();
  push();
  const timer = setInterval(() => {
    tick++;
    if (online && !frozen) {
      data = {
        ...data,
        temperature: 29.4 + Math.sin(tick / 9) * 0.7,
        humidity: 63 + Math.sin(tick / 11) * 2,
        ldrRaw: Math.round(1820 + Math.sin(tick / 8) * 280),
      };
      data.ledOn = data.ldrRaw < 1700;
      status();
      push();
    }
  }, 3000);
  return {
    command(c) {
      advance();
      clearInterval(movement);
      movement = null;
      data.commandId = c.commandId;
      emit({ kind: 'ack', payload: { ...c, status: 'accepted' } });
      if (c.action === 'stop') {
        data.door = fault
          ? 'Fault'
          : data.doorPosition === 0
            ? 'Closed'
            : data.doorPosition === 100
              ? 'Open'
              : 'Stopped';
        push();
        event('STOP', 'หยุดประตูจำลองที่ตำแหน่งปัจจุบัน');
        emit({ kind: 'ack', payload: { ...c, status: 'completed' } });
        return;
      }
      if (fault) {
        emit({ kind: 'ack', payload: { ...c, status: 'rejected' } });
        return;
      }
      activeCommand = c;
      lastMove = now();
      data.door = c.action === 'open' ? 'Opening' : 'Closing';
      push();
      event(
        'COMMAND',
        c.action === 'open' ? 'กำลังเปิดประตูจำลอง' : 'กำลังปิดประตูจำลอง',
      );
      movement = setInterval(() => {
        if (!online) return;
        advance();
        const done =
          data.doorPosition === (activeCommand.action === 'open' ? 100 : 0);
        if (done) {
          clearInterval(movement);
          movement = null;
          data.door = activeCommand.action === 'open' ? 'Open' : 'Closed';
        }
        push();
        if (done)
          emit({
            kind: 'ack',
            payload: { ...activeCommand, status: 'completed' },
          });
      }, 16);
    },
    simulate(action) {
      if (action === 'offline') {
        online = !online;
        if (!online) {
          clearInterval(movement);
          if (['Opening', 'Closing'].includes(data.door)) {
            fault = true;
            data.door = 'Fault';
          }
        }
        status();
        push();
        event(
          'CONNECTION',
          online ? 'Gateway จำลองออนไลน์' : 'Gateway จำลองออฟไลน์',
        );
      }
      if (action === 'stale') {
        frozen = !frozen;
        event(
          'CONNECTION',
          frozen ? 'พักการส่งข้อมูลและ heartbeat • รอ 15 วินาที' : 'ส่งข้อมูลต่อ',
        );
        if (!frozen) {
          status();
          push();
        }
      }
      if (action === 'fault') {
        clearInterval(movement);
        fault = !fault;
        data.door = fault
          ? 'Fault'
          : data.doorPosition === 0
            ? 'Closed'
            : data.doorPosition === 100
              ? 'Open'
              : 'Stopped';
        push();
        event('FAULT', fault ? 'จำลองข้อผิดพลาดมอเตอร์' : 'รีเซ็ต fault จำลองแล้ว');
      }
      if (action === 'access') {
        data.occupied = !data.occupied;
        data.distanceCm = data.occupied ? 46 : 180;
        push();
        event(
          'ACCESS',
          data.occupied
            ? 'จำลองผู้มีสิทธิ์เข้าใช้งานและรถเข้าจอด (ไม่ได้เปิดกล้องจริง)'
            : 'จำลองรถออกจากพื้นที่จอด',
        );
      }
    },
    close() {
      alive = false;
      clearInterval(timer);
      clearInterval(movement);
    },
  };
}
