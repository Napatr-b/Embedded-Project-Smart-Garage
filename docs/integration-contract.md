# ร่างเสนอทีม: Smart Garage Integration Contract v1

เอกสารนี้เป็น **ร่างเสนอทีม** ไม่ใช่ contract ที่กลุ่มอนุมัติแล้ว ใช้ร่างเดียวกันใน Web และ Gateway รอบนี้ยังไม่มี credentials / wiring / feedback ที่ยืนยันสำหรับอุปกรณ์จริง

แทนร่าง v0.1 เดิม (ดูใน git history) การเพิ่ม envelope v1 เป็น breaking change: packet เก่าที่ไม่มี deviceId/schemaVersion จะถูกปฏิเสธ เพื่อไม่แสดงข้อมูลอุปกรณ์ผิดตัว

## ข้อกำหนดที่ยืนยันและเรื่องที่ยังไม่ยืนยัน

ยืนยันจากคำขอและ proposal: Web ↔ Broker ↔ ESP32-S3; Sensor Node อ่าน LDR/DHT22/ENS160 ส่ง UART; กล้อง/IR/Ultrasonic/Servo ฝั่ง Gateway; ESP-WHO บนบอร์ด; บันทึกสถานะจอดรถลง Supabase ผ่าน HTTPS; LINE Messaging API; ไม่มี automatic closing และยังไม่กำหนดวิธีแสดงภาพกล้อง

ร่างเสนอทีม: ทุก topic/field/timeout ในหน้านี้, UART framing/CRC, ACL, authentication, threshold รถ, servo pulse/มุม, pin map, feedback, obstruction detection และการ reset Fault ต้องตกลงก่อน hardware integration

## ค่ากลางและ JSON Schema

- `docs/contract.json` คือค่ากลางของ schemaVersion, topic prefix และเวลา
- `docs/mqtt-schema.json` เป็น JSON Schema Draft 2020-12 เลือก `$defs.telemetry`, `$defs.status`, `$defs.command`, `$defs.ack`, `$defs.event` ตาม topic; ไฟล์นี้เป็นชุด definitions ไม่ใช่ schema ที่เดาชนิด packet เอง
- `dashboard/tests/contract-fixtures.json` มีตัวอย่าง packet ที่เว็บอ่านทดสอบได้
- `node tools/sync-contract.mjs` สร้าง `contract.generated.js` และ `contract.generated.h` เมื่อทีมแก้ค่ากลาง ให้ตรวจ `node tools/sync-contract.mjs --check` ก่อนส่งงาน ไม่แก้ไฟล์ generated โดยตรง

ทั้งสองฝั่งใช้ deviceId เดียวกัน เช่น `garage-01` (เพียงรหัสตัวอย่าง) อนุญาต A-Z/a-z/0-9/_/- ยาว 1–48 ตัว ใช้ schemaVersion เป็นเลข **2**

## Broker และ topic

Broker เดียวต้องมี endpoint สองแบบที่สื่อสาร topic namespace เดียวกัน:

- ESP32: `mqtts://<host>:<TLS-port>` ใช้ MQTT/TLS โดยตรง ตรวจ CA + hostname
- Browser: `wss://<host>:<WSS-port>/<path>` ใช้ MQTT over Secure WebSocket

**ไม่สมมติว่าทั้งสอง URL/port/path เหมือนกัน** ขอค่าจริงจากผู้ดูแล broker ก่อน ตั้งค่า ESP32 ใน `include/config.local.h` และเว็บใน `dashboard/.env` ค่า URL ในตัวอย่างเว้นว่าง ไม่ติดต่อ broker ใดอัตโนมัติ

| ชนิด | Topic ร่าง | ผู้ส่ง | QoS / retain |
|---|---|---|---|
| Telemetry | `draft/smart-garage/<deviceId>/telemetry` | Gateway | 0 / false |
| Status | `draft/smart-garage/<deviceId>/status` | Gateway | 0 / true |
| Command | `draft/smart-garage/<deviceId>/command` | Web | **0 / false** |
| Ack | `draft/smart-garage/<deviceId>/ack` | Gateway | 0 / false |
| Event | `draft/smart-garage/<deviceId>/event` | Gateway | 0 / false |

Gateway สร้างชื่อจาก prefix/deviceId/suffix เว็บตั้งแต่ละ topic ผ่าน environment variables ให้ตรงกัน MQTT clean session, subscribe command QoS 0 หลังเชื่อมใหม่ และไม่เก็บ motor command ใน outbox สำหรับ replay เว็บไม่ queue คำสั่ง offline และไม่ motor retry อัตโนมัติ

## Envelope

| Field | ชนิด | ข้อกำหนด |
|---|---|---|
| schemaVersion | integer | 2 |
| deviceId | string | ต้องตรงกับอุปกรณ์ที่ตั้งไว้ |
| timestamp | string / null | UTC รูปแบบ `YYYY-MM-DDTHH:mm:ss.sssZ`; null เฉพาะ offline Last Will |
| timeSynced | boolean | true สำหรับข้อมูลสดและคำสั่ง; false สำหรับ Last Will ที่ไม่ทราบเวลาตัดจริง |
| simulatedHardware | boolean | true เมื่อเป็นข้อมูลจำลองของ firmware; telemetry ต้องระบุ |
| commandId | string / null | คำสั่งต้องมี ID ยาว 1–128 ตัว เว็บสร้าง UUID; ack ต้องคืน ID เดิม |

เว็บรับ ISO พร้อม timezone สำหรับ packet ฝั่ง Gateway แต่ firmware command parser ใช้ canonical UTC 24 ตัวตามตัวอย่าง เว็บส่ง `toISOString()` จึงตรงกัน Gateway ต้องไม่สร้างเวลาเดาขึ้นมาแล้วอ้างว่า sync สำเร็จ

Firmware ยังไม่เริ่ม MQTT/TLS จนกว่าจะได้รับ SNTP sync callback ใน boot นี้ และเวลาผ่าน sanity check ปี 2026 เพื่อให้ตรวจอายุ certificate ได้ ถ้า SNTP ไม่สำเร็จ Serial จะแสดง `timeSynced=no`; ไม่รับ remote commands และไม่เผยแพร่ข้อมูลสด ใน production ต้องทบทวนแหล่งเวลาและการตรวจ clock jump เพิ่มเติม ไม่มีการปิด certificate verification เพื่อแก้ปัญหาเวลา

## Telemetry: snapshot ไม่เติมค่าปลอม

```json
{"schemaVersion":2,"deviceId":"garage-01","timestamp":"2026-11-16T03:00:00.000Z","timeSynced":true,"simulatedHardware":false,"door":null,"occupied":null,"distanceCm":null,"temperature":null,"humidity":null,"ldrRaw":null,"ledOn":null,"aqi":null,"tvocPpb":null,"eco2Ppm":null,"commandId":null,"positionConfirmed":false,"commandedServoAngle":null,"actuatorEnabled":false}
```

| Field | ชนิด / หน่วย | ความหมาย |
|---|---|---|
| door | Closed / Opening / Open / Closing / Stopped / Fault / null | null หรือขาดแสดง Unknown |
| occupied | boolean / null | สถานะพื้นที่จอด ไม่ได้ยืนยันทางเลื่อนประตูปลอดภัย |
| distanceCm | number 0–1000 / null, cm | Ultrasonic |
| temperature | number -40–80 / null, °C | DHT22 |
| humidity | number 0–100 / null, %RH | DHT22 |
| ldrRaw | number 0–65535 / null, raw | ADC ยังไม่ calibration ไม่เรียก lux |
| ledOn | boolean / null | สถานะ LED ที่ Sensor Node รายงาน |
| aqi | number 1–5 / null | ENS160 index ไม่ใช่ AQI 0–500 |
| tvocPpb | number / null, ppb | TVOC |
| eco2Ppm | number / null, ppm | **eCO2** ค่าประมาณเทียบเท่า ไม่ใช่ CO2 ที่วัดตรง |
| positionConfirmed | boolean | ยืนยันจาก feedback ที่ตกลง; ใน simulation เป็นหลักฐานเฉพาะโลกจำลอง |
| commandedServoAngle | number / null, degree | มุมที่สั่ง แยกจากตำแหน่งที่ยืนยันได้ รอบนี้เป็น null |
| actuatorEnabled | boolean | รอบนี้ false ทุก environment ไม่มี real servo driver |

ช่วง TVOC/eCO2 ที่รับปัจจุบัน 0–10000000 เป็น guard เบื้องต้น ต้องตกลงช่วงจริงและ ENS160 validity/warm-up กับทีม ค่า invalid/warm-up ให้ส่ง null ไม่แสดงว่าอากาศดี

ถ้า door เป็น Closed/Open แต่ไม่มี positionConfirmed:true เว็บปรับเป็น Unknown แทนการอนุมานจากมุม servo

Telemetry เป็น snapshot ถ้า field หายจะเป็น “ไม่มีข้อมูล” ไม่คงค่าของ packet ก่อนหน้า MQTT แบบไม่มีฮาร์ดแวร์ส่ง null จริง โหมด `gateway_simulated` เท่านั้นที่สร้างค่าเซนเซอร์ พร้อม `simulatedHardware:true` เว็บแสดงป้ายข้อมูลจำลองจาก Gateway

## Status, LWT และข้อมูลเก่า

```json
{"schemaVersion":2,"deviceId":"garage-01","timestamp":"2026-11-16T03:00:00.000Z","timeSynced":true,"simulatedHardware":false,"online":true}
```

Status heartbeat ทุก **3 วินาที** keepalive MQTT **10 วินาที**; เว็บถือว่า stale เมื่อ heartbeat หรือ telemetry เกิน **15 วินาที** หรือเวลาอนาคตเกิน 5 วินาที ทุกปุ่มรวม Stop ปิดเมื่อ offline/stale ค่าที่เห็นเป็นค่าล่าสุดก่อนขาดการเชื่อมต่อ ไม่ใช่ค่าปัจจุบัน

Last Will retain true ที่ broker ส่งเมื่อ Gateway หาย:

```json
{"schemaVersion":2,"deviceId":"garage-01","timestamp":null,"timeSynced":false,"online":false,"reason":"last_will"}
```

ไม่ใส่ timestamp ตอน CONNECT แล้วอ้างเป็นเวลาหลุดจริง เพราะ broker อาจส่งหลายชั่วโมงต่อมา เว็บรับ offline แบบ conservative ทันทีโดยอ้างเวลารับบน browser และไม่ใช้ LWT เป็น telemetry สด ข้อมูล online ที่ retain แต่เก่าจะไม่เปิดปุ่มควบคุม การต่อ Broker สำเร็จไม่เท่ากับ Gateway online

## Command และ acknowledgement

```json
{"schemaVersion":2,"deviceId":"garage-01","timestamp":"2026-11-16T03:00:01.000Z","timeSynced":true,"commandId":"c61e4ccf-2c27-4692-aef1-189b825e6b33","action":"open"}
```

action: open / close / stop เท่านั้น command ไม่ retain ทั้งฝั่งส่งและ Gateway ปฏิเสธ packet ที่ติด retain flag ตรวจอายุไม่เกิน **8 วินาที**, future skew ไม่เกิน **2 วินาที**, timestamp ต้องใหม่กว่า connection/subscription session ล่าสุด และต้องใหม่กว่า command watermark

Gateway เก็บ commandId ล่าสุด 32 รายการใน RAM พร้อม timestamp watermark เพื่อกัน replay หลัง cache eviction คำสั่ง timestamp เท่ากันหรือล้าหลังจะถูกปฏิเสธ แม้ ID ต่างกัน นี่เป็นนโยบาย fail-closed ของร่าง ไม่ใช่ durable dedup ข้าม reboot การเริ่ม session ใหม่และ TTL ช่วยป้องกัน command ก่อน reconnect ต้องมี ACL/anti-replay เพิ่มก่อนเปิดอินเทอร์เน็ต

```json
{"schemaVersion":2,"deviceId":"garage-01","timestamp":"2026-11-16T03:00:01.020Z","timeSynced":true,"simulatedHardware":true,"commandId":"c61e4ccf-2c27-4692-aef1-189b825e6b33","status":"received"}
```

| status | ความหมาย |
|---|---|
| received | แยก JSON และอ่าน ID ได้ ไม่ได้แปลว่าผ่าน validation หรือจะขยับ |
| accepted | ผ่าน policy และ controller รับคำสั่งแล้ว ยังไม่เสร็จ |
| rejected | ไม่ทำหรือทำไม่สำเร็จ พร้อม reason เช่น duplicate_command / expired_or_future_command / actuator_disabled_no_wiring_or_feedback |
| completed | ยืนยันว่าการดำเนินคำสั่งในโหมดนั้นเสร็จแล้ว ไม่ใช่หลักฐานตำแหน่งจริงจากมุม servo |

เว็บรอ 8 วินาที: received/accepted หรือ completed ack เพียงอย่างเดียวยังไม่ยืนยันตำแหน่ง ต้องมี accepted/completed acknowledgement **และ** final telemetry Open/Closed พร้อม commandId ตรงกันและ positionConfirmed:true จึงจบ pending; Stop ต้องมี completed ack และ telemetry Stopped/Closed/Open พร้อม ID ตรงกัน; rejected ที่ ID ตรงกันจบ pending ว่าไม่สำเร็จ ภาพสถานะประตูอิง telemetry เสมอ ไม่เปลี่ยนจาก publish callback ถ้าหมดเวลาจะแสดงไม่ทราบผล ไม่มี retry และให้ตรวจสถานะก่อนสั่งใหม่

ใน firmware เมื่อเปิด/ปิดจำลองสำเร็จจะส่ง completed และ final telemetry; normal MQTT mode ไม่ยอมรับ motion เพราะ actuator ยัง disabled. QoS 0 อาจหายได้ การเงียบหรือ timeout ไม่ได้ยืนยันว่าประตูหยุดแล้ว

Packet ผิดรูปแบบที่อ่าน commandId ไม่ได้จะบันทึก event/Serial โดยไม่แต่ง ID ขึ้นมาตอบ Gateway รับ command สูงสุด 1024 bytes และปฏิเสธ fragmented frame แทนประกอบบางส่วน; เว็บรับ packet สูงสุด 16 KB คิว Gateway 8 รายการ เต็มแล้วทิ้งโดยไม่ replay

## Door state, Stop และ feedback

- bring-up / normal MQTT: เริ่ม Unknown จริง ไม่มี GPIO/Servo/กล้อง/feedback จึงปฏิเสธ remote motion รวม Stop ด้วยเหตุผล actuator disabled
- simulation: position 0 (Closed) to 100 (Open), full travel 2800 ms. Reversal and repeated commands continue from current position.
- Stop in simulation cancels movement, holds current position as Stopped (endpoints remain Closed/Open). Open/close can resume without reset. Superseded commands receive rejected/superseded_by_command.
- movement timeout **6 วินาที** ด้วย monotonic clock → Fault; network lost ระหว่างเคลื่อนจำลอง → Fault และล้าง pending ไม่ replay หลัง reconnect ไม่มี auto closing
- positional servo: มุมที่สั่งไม่ใช่ feedback ประตูจริง ต้องเพิ่มเซนเซอร์ยืนยันตำแหน่ง/เงื่อนไขที่ทีมอนุมัติก่อนรายงาน Closed/Open จริง

## Event และข้อมูลซ้ำ

```json
{"schemaVersion":2,"deviceId":"garage-01","timestamp":"2026-11-16T03:00:04.000Z","timeSynced":true,"simulatedHardware":true,"type":"COMMAND_ACCEPTED","message":"open","commandId":null}
```

type/message เป็น string และไม่ render เป็น HTML เว็บแสดงล่าสุด 100 รายการในรอบนั้น Telemetry MQTT ที่ timestamp เก่ากว่าหรือซ้ำทั้ง packet จะไม่เพิ่มจุดกราฟซ้ำ แต่ยอมรับ state/commandId ใหม่ที่มี timestamp เท่ากัน เพื่อไม่ทิ้ง Stop ที่เกิดในมิลลิวินาทีเดียวกัน หากมี buffered history ให้ส่ง endpoint ประวัติแยกพร้อม deviceId + nodeId + sequence/eventId สำหรับ idempotent writes ไม่ส่งข้อมูลย้อนหลังลง topic telemetry สด

Event QoS 0 รอบนี้ยังไม่ทำ durable dedup/eventId ฝั่งคลาวด์ ต้องตกลง ID และ retention ก่อน HTTPS adapter จริง ห้ามอ้างว่า log ใน RAM เป็น durable history

## UART ร่างสำหรับ Sensor Node (ยังไม่เปิด driver จริง)

เสนอ UART 115200 baud, 8N1, logic 3.3V, common GND; **ยังไม่กำหนด TX/RX GPIO** ไม่เขียน firmware Sensor Node ในรอบนี้ ต้องให้เพื่อนยืนยัน baud/pins/framing ก่อน

Frame เสนอ: `<JSON ASCII bytes>*<CRC16 4 uppercase hex>\n` ไม่เกิน 512 bytes JSON ไม่มี newline คำนวณ CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, refin=false, refout=false, xorout=0) บน bytes ก่อน `*` ทั้งหมด ไม่รวม `*`, hex และ newline Test vector ASCII `123456789` ต้องได้ `29B1`

ตัวอย่างส่วน JSON (CRC ต้องคำนวณจาก bytes ที่ส่งจริง ไม่ใช่ placeholder):

```json
{"schemaVersion":1,"nodeId":"sensor-01","seq":42,"readings":{"temperature":29.4,"humidity":63,"ldrRaw":1820,"ledOn":false,"aqi":2,"tvocPpb":125,"eco2Ppm":540}}
```

ACK/NACK กลับเป็น JSON+CRC framing เดียวกัน:

```json
{"schemaVersion":1,"nodeId":"sensor-01","seq":42,"ack":true}
```

เสนอ ACK timeout 200 ms, retry สูงสุด 3 ครั้งด้วย nodeId/seq เดิม (รวมส่งครั้งแรกเป็น 4 attempts) checksum ผิดให้ NACK เมื่อทราบ nodeId/seq อย่างเชื่อถือได้ มิฉะนั้นทิ้งและรอ retry Gateway จำ seq ต่อ node ตอบ ACK ซ้ำแต่ไม่บันทึกซ้ำ Node ต้องตกลง bootId/seq wrap หรือ reset session handshake เพื่อไม่สับสนเมื่อ reboot Gateway เพิ่ม UTC timestamp หลังตรวจ CRC ผ่าน หากเวลาไม่ sync ให้พักเป็นข้อมูลไม่มีเวลา/monotonic ใน buffer แล้วตกลงวิธี backfill ห้ามแต่งเวลาสด

UART ใช้ binary frame ตาม `shared/protocol/garage_protocol.h` และ [protocol.md](protocol.md) (ไม่ใช่ JSON) Gateway รับใน `gateway/src/node_link.cpp` แล้วเติมค่าเข้า telemetry (`distanceCm`, temperature ฯลฯ) เมื่อไม่ได้อยู่ในโหมด simulated

## Supabase, LINE และ authentication

หน้า MQTT ใช้ข้อมูลในรอบปัจจุบัน ส่วนโหมด Demo ใช้ข้อมูลตัวอย่างในหน่วยความจำ หน้าแรกอ่านสถานะจอดรถจาก Backend

Gateway ส่งสถานะจอดรถทาง HTTPS ไปที่ Backend ซึ่งบันทึกลง Supabase ตาม [คู่มือ](parking-mvp.md) ตารางค่าจากเซนเซอร์และคำสั่งประตูเตรียมไว้ใน SQL migration ส่วน LINE Messaging API ยังอยู่ในแผน

`VITE_*` ทุกค่าถูกส่งไป browser ไม่ใช่ที่เก็บ secret ห้ามใส่ LINE token / Supabase secret key / MQTT password ถาวรใน frontend `.env` ไม่ช่วยซ่อน VITE_ เว็บยังรอระบบ login/short-lived WSS authentication หรือ backend proxy ที่ตรวจสิทธิ์ ก่อนใช้จริง

ACL ขั้นต่ำ: browser publish เฉพาะ command ของ device ที่ตนมีสิทธิ์ และ subscribe เฉพาะ telemetry/status/ack/event ของ device นั้น; Gateway มีสิทธิ์กลับด้าน จำกัดขนาด/rate และห้าม public anonymous broker ควบคุม actuator จริง Firmware sample บังคับ mqtts + per-device username/password หรือปรับเป็น mTLS หลังทีมทบทวน (mTLS ยังไม่ implement)

ขอจากเพื่อนก่อนเชื่อม: endpoint ทั้งสองแบบ, CA chain, deviceId, credentials/ACL, contract version/payload จริง, NTP, thresholds/calibration/validity, GPIO/camera map, interlocks/feedback/servo power, stop/reset behavior, LINE recipient/token owner และนโยบายเก็บข้อมูลใบหน้า

## Door motion schema v2

MQTT envelope schemaVersion is 2 on web and Gateway; update both together. Sensor UART remains version 1.
- door adds Stopped for a normal mid-travel stop, distinct from Fault.
- doorPosition: number 0 to 100 or null. 0 = fully closed; 100 = fully open. Simulation position is not physical feedback.
- Missing position: web estimates the image only from Gateway Opening/Closing telemetry; stale/offline freezes estimation. It never infers successful completion.
- Full simulation travel: 2800 ms; remaining duration scales with distance. Stop, reversal and repeated commands preserve current position.
- Completion requires matching commandId telemetry and acknowledgement. Stop requires completed ack plus Stopped/Closed/Open telemetry.
- actuatorEnabled remains false. No physical actuation enabled.
