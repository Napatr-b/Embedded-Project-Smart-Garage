import { useEffect, useRef, useState } from 'react';
import { createDemo } from './services/demo.js';

import {
  mockHistoryAdapter,
  unconfiguredHistoryAdapter,
} from './services/history.js';
import {
  canCommand,
  fresh,
  CommandTracker,
  acceptTelemetry,
} from './services/protocol.js';
const initial = () => ({
  broker: 'connecting',
  online: false,
  statusAt: null,
  telemetryAt: null,
  receivedAt: null,
  data: {},
  doorPosition: null,
  events: [],
  samples: [],
  source: 'กำลังโหลด',
  pending: null,
  message: '',
});
export function useGarage(mode, reset) {
  const [state, setState] = useState(initial),
    [now, setNow] = useState(Date.now());
  const current = useRef(initial()),
    service = useRef(null),
    tracker = useRef(null);
  useEffect(() => {
    let active = true;
    current.current = initial();
    tracker.current = new CommandTracker();
    setState(current.current);
    const update = (fn) => {
      if (!active) return;
      current.current = fn(current.current);
      setState({ ...current.current });
    };
    const addEvent = (s, p) => ({
      ...s,
      events: [{ ...p, id: crypto.randomUUID() }, ...s.events].slice(0, 100),
    });
    const emit = (e) =>
      update((s) => {
        const p = e.payload;
        if (e.kind === 'broker')
          return {
            ...s,
            broker: e.value,
            online: e.value === 'connected' ? s.online : false,
            statusAt: e.value === 'connected' ? s.statusAt : null,
            message: e.message || s.message,
          };
        if (e.kind === 'error')
          return addEvent(
            { ...s, message: e.message },
            {
              timestamp: new Date().toISOString(),
              type: 'ERROR',
              message: e.message,
            },
          );
        if (e.kind === 'status') {
          if (p.online === false)
            return { ...s, online: false, statusAt: Date.now() };
          const t = Date.parse(p.timestamp);
          if (!fresh(t) || t < (s.statusAt ?? 0)) return s;
          return { ...s, online: p.online, statusAt: t };
        }
        if (e.kind === 'telemetry') {
          const t = Date.parse(p.timestamp);
          if (
            mode === 'mqtt'
              ? !acceptTelemetry(p, s.data)
              : !fresh(t) || t < (s.telemetryAt ?? 0)
          )
            return s;
          const done = tracker.current.state(p);
          return {
            ...s,
            data: p,
            doorPosition: Number.isFinite(p.doorPosition)
              ? p.doorPosition
              : p.door === 'Open'
                ? 100
                : p.door === 'Closed'
                  ? 0
                  : s.doorPosition,
            telemetryAt: t,
            receivedAt: Date.now(),
            samples: !s.samples.length || t - Date.parse(s.samples.at(-1).timestamp) >= 3000 ? [...s.samples, p].slice(-40) : s.samples,
            pending: tracker.current.pending
              ? { ...tracker.current.pending }
              : null,
            message: done ? 'Gateway ยืนยันสถานะตามคำสั่งแล้ว' : s.message,
          };
        }
        if (e.kind === 'ack') {
          if (mode === 'mqtt' && !fresh(Date.parse(p.timestamp))) return s;
          const result = tracker.current.ack(p);
          return {
            ...s,
            pending: tracker.current.pending
              ? { ...tracker.current.pending }
              : null,
            message:
              result === 'completed'
                ? 'Gateway ยืนยันว่าดำเนินคำสั่งแล้ว'
                : result === 'rejected'
                  ? `Gateway ปฏิเสธคำสั่ง: ${p.reason || 'ไม่ระบุเหตุผล'}`
                  : s.message,
          };
        }
        if (e.kind === 'event') return addEvent(s, p);
        return s;
      });
    if (mode === 'demo') service.current = createDemo(emit);
    else
      import('./services/mqtt.js')
        .then(({ createMqtt, readConfig }) => {
          if (active)
            service.current = createMqtt(emit, readConfig(import.meta.env));
        })
        .catch(() => {
          if (active)
            emit({
              kind: 'broker',
              value: 'error',
              message: 'โหลด MQTT service ไม่สำเร็จ',
            });
        });
    const adapter =
      mode === 'demo' ? mockHistoryAdapter : unconfiguredHistoryAdapter;
    adapter
      .load()
      .then((h) =>
        update((s) => ({
          ...s,
          source: h.source,
          events: [...s.events, ...h.events]
            .sort((a, b) => Date.parse(b.timestamp) - Date.parse(a.timestamp))
            .slice(0, 100),
          samples: [...h.samples, ...s.samples].slice(-40),
        })),
      )
      .catch(() => update((s) => ({ ...s, message: 'โหลดประวัติไม่สำเร็จ' })));
    const timer = setInterval(() => {
      setNow(Date.now());
      // Estimate only from Gateway motion telemetry; never infer completion.
      if (mode === 'mqtt')
        update((s) => {
          if (!canCommand(s)) return s;
          const direction =
            s.data.door === 'Opening' ? 1 : s.data.door === 'Closing' ? -1 : 0;
          return direction
            ? {
                ...s,
                doorPosition: Math.max(
                  0,
                  Math.min(
                    100,
                    (s.doorPosition ?? 0) + ((direction * 50) / 2800) * 100,
                  ),
                ),
              }
            : s;
        });
      if (tracker.current.expire())
        update((s) =>
          addEvent(
            {
              ...s,
              pending: null,
              message: 'หมดเวลารอ Gateway • ไม่ทราบผลคำสั่ง ตรวจสถานะก่อนสั่งใหม่',
            },
            {
              timestamp: new Date().toISOString(),
              type: 'TIMEOUT',
              message: 'ไม่ได้รับการยืนยันภายใน 8 วินาที • ไม่มีการส่งซ้ำ',
            },
          ),
        );
    }, 50);
    return () => {
      active = false;
      clearInterval(timer);
      service.current?.close();
      service.current = null;
    };
  }, [mode, reset]);
  function command(action) {
    if (!canCommand(current.current)) {
      setState((s) => ({ ...s, message: 'ส่งไม่ได้: Gateway ออฟไลน์หรือข้อมูลเก่า' }));
      return;
    }
    if (
      action !== 'stop' &&
      (!current.current.data.door || current.current.data.door === 'Fault')
    )
      return;
    try {
      const c = tracker.current.start(action);
      current.current = {
        ...current.current,
        pending: { ...c },
        message: 'ส่งคำสั่งแล้ว • รอ Gateway ยืนยัน',
      };
      setState(current.current);
      service.current.command({
        commandId: c.commandId,
        action,
        timestamp: new Date().toISOString(),
      });
    } catch (error) {
      tracker.current.pending = null;
      current.current = {
        ...current.current,
        pending: null,
        message: error.message,
      };
      setState(current.current);
    }
  }
  return {
    state,
    now,
    command,
    simulate: (action) => service.current?.simulate?.(action),
  };
}
