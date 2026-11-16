import { useState } from 'react';
import { useGarage } from './useGarage.js';
import { canCommand, fresh } from './services/protocol.js';
import { Badge, Metric, thaiTime, value } from './components/Common.jsx';
import Trend from './components/Trend.jsx';
import EventLog from './components/EventLog.jsx';
const doors = {
  Closed: 'ปิดแล้ว',
  Opening: 'กำลังเปิด',
  Open: 'เปิดแล้ว',
  Closing: 'กำลังปิด',
  Stopped: 'หยุดกลางทาง',
  Fault: 'ขัดข้อง',
  Unknown: 'ยังไม่ทราบสถานะ',
};
export default function App() {
  const [mode, setMode] = useState(() =>
      new URLSearchParams(window.location.search).get('mode') === 'mqtt'
        ? 'mqtt'
        : 'demo',
    ),
    [reset, setReset] = useState(0),
    [offline, setOffline] = useState(false),
    [paused, setPaused] = useState(false);
  const { state: s, now, command, simulate } = useGarage(mode, reset);
  const d = s.data,
    door = d.door || 'Unknown',
    ready = canCommand(s, now),
    stale = !fresh(s.telemetryAt, now),
    gateway = s.online && fresh(s.statusAt, now),
    isDemo = mode === 'demo';
  function resetDemo() {
    setOffline(false);
    setPaused(false);
    setReset((n) => n + 1);
  }
  return (
    <div className="app-shell">
      <aside className="sidebar">
        <a className="brand" href="#main">
          <span className="brand-icon" aria-hidden="true">
            ⌂
          </span>
          <span>
            Smart Garage<small>EMBEDDED SYSTEM</small>
          </span>
        </a>
        <div className="nav-label">พื้นที่ทำงาน</div>
        <a className="nav-item active" href="#main">
          <span aria-hidden="true">▦</span> ภาพรวมโรงจอดรถ
        </a>
        <a className="nav-item" href="#events">
          <span aria-hidden="true">≡</span> ประวัติเหตุการณ์
        </a>
        <div className="side-project">
          <span className="eyebrow">PROJECT 2026</span>
          <p>
            เชื่อมโลกจริง
            <br />
            กับระบบอัจฉริยะ
          </p>
          <small>
            Sensor Node → UART
            <br />
            Gateway → MQTT → Web
          </small>
        </div>
        <div className="side-footer">
          <span className="dot" />
          กลุ่ม นอนน้อยนอนไม่พอ
        </div>
      </aside>
      <div className="workspace">
        <header className="topbar">
          <span>
            โรงจอดรถอัจฉริยะ <span className="divider">/</span>{' '}
            <strong>แดชบอร์ด</strong>
          </span>
          <span className="clock">{thaiTime(now)}</span>
        </header>
        <main id="main">
          <div className="page-heading">
            <div>
              <div className="eyebrow">GARAGE OVERVIEW</div>
              <h1>
                Dash Board<span className="heading-dot">.</span>
              </h1>
              <p>ติดตามสภาพแวดล้อมและควบคุมโรงจอดรถของคุณ</p>
            </div>
            <label className="mode-label">
              โหมดการทำงาน
              <select
                value={mode}
                onChange={(e) => {
                  setMode(e.target.value);
                  setOffline(false);
                  setPaused(false);
                }}
              >
                <option value="demo">Demo Mode</option>
                <option value="mqtt">MQTT Mode</option>
              </select>
            </label>
          </div>
          <div className={`mode-banner ${isDemo ? 'demo' : 'integration'}`}>
            <div>
              <strong>
                {isDemo
                  ? '◈ ข้อมูลจำลอง • Demo Mode'
                  : d.simulatedHardware
                    ? gateway && !stale
                      ? '◈ เชื่อมบอร์ดจริง / จำลองการเคลื่อนประตู'
                      : '◈ ข้อมูลจำลองจากบอร์ดครั้งล่าสุด / ยังไม่ยืนยันการเชื่อมต่อ'
                    : '◇ MQTT Mode • โครงสำหรับ integration'}
              </strong>
              <span>
                {isDemo
                  ? 'ทดลองได้ทุกฟังก์ชัน โดยไม่ต้องเชื่อมต่ออุปกรณ์หรือบัญชีภายนอก'
                  : d.simulatedHardware
                    ? 'Actuator ปิด • ค่าเซนเซอร์และตำแหน่งประตูเป็นข้อมูลจำลองจากเฟิร์มแวร์ ไม่ใช่ค่าที่วัดจากอุปกรณ์จริง'
                    : 'ต้องตกลง authentication และสิทธิ์ topic กับทีมก่อนใช้ควบคุมจริง'}
              </span>
            </div>
            {isDemo && (
              <button className="text-button" onClick={resetDemo}>
                ↻ รีเซ็ตเดโม
              </button>
            )}
          </div>
          {!isDemo && import.meta.env.VITE_LOCAL_TEST === 'true' && (
            <p className="alert">
              LOCAL TEST • MQTT/WS ไม่เข้ารหัส ใช้เฉพาะเครือข่ายทดสอบ •
              บัญชีเว็บมองเห็นได้ใน browser • actuator ต้องปิดอยู่
            </p>
          )}
          <section className="connection-strip" aria-label="สถานะการเชื่อมต่อ">
            <div>
              <small>MQTT BROKER {isDemo ? '(จำลอง)' : ''}</small>
              <Badge tone={s.broker === 'connected' ? 'good' : 'warn'}>
                {{
                  connected: 'เชื่อมต่อแล้ว',
                  connecting: 'กำลังเชื่อมต่อ',
                  reconnecting: 'กำลังเชื่อมต่อใหม่',
                  disconnected: 'ไม่ได้เชื่อมต่อ',
                  unconfigured: 'ยังไม่ตั้งค่า',
                  error: 'เชื่อมต่อผิดพลาด',
                }[s.broker] || s.broker}
              </Badge>
            </div>
            <div>
              <small>GATEWAY {isDemo ? '(จำลอง)' : d.deviceId || ''}</small>
              <Badge tone={gateway ? 'good' : 'warn'}>
                {gateway
                  ? 'ออนไลน์'
                  : s.statusAt
                    ? 'ออฟไลน์ / heartbeat เก่า'
                    : 'ยังไม่ทราบ'}
              </Badge>
            </div>
            <div className="last-seen">
              <small>รับ TELEMETRY ล่าสุด • ASIA/BANGKOK</small>
              <span>
                {thaiTime(s.receivedAt, true)}{' '}
                <Badge tone={stale ? 'warn' : 'good'}>
                  {stale ? 'ไม่มีข้อมูลใหม่ / ข้อมูลเก่า' : 'ข้อมูลล่าสุด'}
                </Badge>
              </span>
            </div>
          </section>
          {!ready && (
            <p className="alert" role="status">
              ส่งคำสั่งไม่ได้: ต้องเชื่อม Broker และได้รับ heartbeat / telemetry ใหม่ภายใน
              15 วินาที ค่าที่เห็นอาจเป็นค่าล่าสุดก่อนขาดการเชื่อมต่อ
            </p>
          )}
          <div className="primary-grid">
            <section className="panel door-panel">
              <div className="section-head">
                <div>
                  <span className="eyebrow">DOOR CONTROL</span>
                  <h2>ควบคุมประตู</h2>
                </div>
                <Badge
                  tone={
                    door === 'Fault'
                      ? 'danger'
                      : door === 'Unknown'
                        ? 'neutral'
                        : 'good'
                  }
                >
                  {doors[door]}
                </Badge>
              </div>
              <div className="garage-scene" aria-hidden="true">
                <div className="garage-roof" />
                <div className="garage-frame">
                  <div className="garage-dark">
                    <span>SMART GARAGE</span>
                  </div>
                  <div
                    className="garage-shutter"
                    style={{
                      transform: `translateY(-${s.doorPosition ?? 0}%)`,
                    }}
                  />
                </div>
                <div className="garage-ground" />
              </div>
              <div className="door-caption">
                <strong>
                  {doors[door]} · {Math.round(s.doorPosition ?? 0)}%
                </strong>
                <span>
                  {isDemo || d.simulatedHardware
                    ? 'ตำแหน่งยืนยันเฉพาะโลกจำลอง'
                    : d.positionConfirmed
                      ? 'ตำแหน่งยืนยันจาก feedback ของ Gateway'
                      : 'แอนิเมชันเป็นค่าประมาณ • ยังไม่มี feedback ตำแหน่งจริง'}
                </span>
              </div>
              <div className="controls">
                <button
                  className="primary"
                  disabled={
                    !ready ||
                    door === 'Fault' ||
                    door === 'Unknown' ||
                    door === 'Open'
                  }
                  onClick={() => command('open')}
                >
                  ↑ เปิดประตู
                </button>
                <button
                  className="secondary"
                  disabled={
                    !ready ||
                    door === 'Fault' ||
                    door === 'Unknown' ||
                    door === 'Closed'
                  }
                  onClick={() => command('close')}
                >
                  ↓ ปิดประตู
                </button>
                <button
                  className="stop"
                  disabled={!ready}
                  onClick={() => command('stop')}
                >
                  ■ หยุด
                </button>
              </div>
              <div className="command-message" role="status" aria-live="polite">
                {s.pending
                  ? `กำลังรอยืนยัน ${s.pending.action}${s.pending.accepted ? ' • Gateway รับคำสั่งแล้ว' : ''}`
                  : s.message || 'พร้อมรับคำสั่ง • ไม่มีการปิดประตูอัตโนมัติ'}
                {s.pending && <code>commandId: {s.pending.commandId}</code>}
              </div>
              <p className="safety-note">
                ปุ่มหยุดบนเว็บไม่ทดแทนปุ่มหยุดฮาร์ดแวร์
                <br />
                พื้นที่จอดว่างไม่ได้ยืนยันว่าทางเลื่อนประตูปลอดภัย
              </p>
            </section>
            <div className="right-column">
              <section className="panel parking-panel">
                <div className="section-head">
                  <div>
                    <span className="eyebrow">PARKING SPACE</span>
                    <h2>พื้นที่จอดรถ</h2>
                  </div>
                  <Badge
                    tone={
                      d.occupied == null
                        ? 'neutral'
                        : d.occupied
                          ? 'warn'
                          : 'good'
                    }
                  >
                    {d.occupied == null
                      ? 'ไม่มีข้อมูล'
                      : d.occupied
                        ? 'มีรถจอด'
                        : 'ว่าง'}
                  </Badge>
                </div>
                <div className="parking-body">
                  <div
                    className={`parking-space ${d.occupied ? 'occupied' : ''}`}
                    aria-hidden="true"
                  >
                    {d.occupied ? '▰' : 'P'}
                  </div>
                  <div>
                    <div className="distance">
                      {value(d.distanceCm, 1)}{' '}
                      <small>{d.distanceCm == null ? '' : 'ซม.'}</small>
                    </div>
                    <p>ระยะจาก Ultrasonic</p>
                    <small>ใช้ตรวจพื้นที่จอด • ไม่ใช่เซนเซอร์ยืนยันทางประตู</small>
                  </div>
                </div>
              </section>
              <section className="panel air-panel">
                <div className="section-head">
                  <div>
                    <span className="eyebrow">AIR QUALITY · ENS160</span>
                    <h2>คุณภาพอากาศ</h2>
                  </div>
                  <span className="sensor-tag">ENS160</span>
                </div>
                <div className="air-values">
                  <div>
                    <small>
                      AQI <span>(1–5)</span>
                    </small>
                    <strong>{value(d.aqi)}</strong>
                  </div>
                  <div>
                    <small>
                      TVOC <span>ppb</span>
                    </small>
                    <strong>{value(d.tvocPpb)}</strong>
                  </div>
                  <div>
                    <small>
                      eCO2 <span>ppm</span>
                    </small>
                    <strong>{value(d.eco2Ppm)}</strong>
                  </div>
                </div>
                <p className="subtle">
                  eCO2 เป็นค่าประมาณเทียบเท่า ไม่ใช่การวัด CO2 โดยตรง
                </p>
              </section>
            </div>
          </div>
          <section className="metrics" aria-label="ค่าเซนเซอร์">
            <Metric
              label="อุณหภูมิ"
              number={value(d.temperature, 1)}
              unit="°C"
              note="DHT22 • Temperature"
              icon="♨"
            />
            <Metric
              label="ความชื้นสัมพัทธ์"
              number={value(d.humidity, 1)}
              unit="%RH"
              note="DHT22 • Humidity"
              icon="◉"
            />
            <Metric
              label="ระดับแสง"
              number={value(d.ldrRaw)}
              unit="raw"
              note="LDR • ค่าดิบ ยังไม่ calibration"
              icon="☼"
            />
            <Metric
              label="ไฟส่องสว่าง"
              number={d.ledOn == null ? 'ไม่มีข้อมูล' : d.ledOn ? 'เปิด' : 'ปิด'}
              unit=""
              note="LED • ควบคุมแสงที่ Sensor Node"
              icon="✧"
            />
          </section>
          <Trend samples={s.samples} />
          {isDemo && (
            <section className="demo-tools">
              <div>
                <span className="eyebrow">DEMO LAB</span>
                <h2>ทดลองสถานการณ์</h2>
                <p>ใช้เครื่องมือเหล่านี้ระหว่างสาธิตโครงงาน</p>
              </div>
              <div className="tool-buttons">
                <button
                  aria-pressed={offline}
                  onClick={() => {
                    simulate('offline');
                    setOffline(!offline);
                  }}
                >
                  {offline ? 'เชื่อม Gateway กลับ' : 'จำลอง Gateway offline'}
                </button>
                <button
                  aria-pressed={paused}
                  onClick={() => {
                    simulate('stale');
                    setPaused(!paused);
                  }}
                >
                  {paused ? 'ส่งข้อมูลต่อ' : 'พักข้อมูล (ทดสอบข้อมูลเก่า)'}
                </button>
                <button onClick={() => simulate('fault')}>
                  {door === 'Fault' ? 'รีเซ็ต Fault' : 'จำลอง Fault'}
                </button>
                <button onClick={() => simulate('access')}>
                  จำลองรถเข้า / ออก
                </button>
              </div>
            </section>
          )}
          <EventLog events={s.events} source={s.source} />
          <footer className="page-footer">
            <span>
              Smart Garage <b>·</b> Embedded System 2026
            </span>
            <span>Face Recognition: ESP-WHO บนบอร์ด • ยังไม่มีระบบภาพกล้อง</span>
          </footer>
        </main>
      </div>
    </div>
  );
}
