> เอกสารเก็บงานเดิมก่อนจัดโครงสร้าง ใช้อ้างอิงย้อนหลังเท่านั้น โปรดใช้ README.md ที่รากโครงการและ integration-contract.md ฉบับปัจจุบัน

# ร่างสำหรับตกลงกับทีม: Integration Contract v0.1

**ทุก topic และ payload ในเอกสารนี้เป็นข้อเสนอใหม่ ไม่ใช่ค่าที่กลุ่มยืนยันแล้ว** โค้ดปัจจุบันรองรับร่างนี้ ห้ามคัดลอกไปอ้างว่าเป็น interface ของ firmware จริงจนกว่าเพื่อน Gateway จะตรวจและตกลงร่วมกัน

## เส้นทางข้อมูลตาม proposal

Sensor Node (ESP32 DOIT) อ่าน LDR, DHT22, ENS160 และควบคุม LED → UART → Gateway ESP32-S3 → MQTT Broker → เว็บ ส่วน IR / Ultrasonic / Servo / OV2640 อยู่ฝั่ง Gateway การรับรู้ใบหน้าใช้ ESP-WHO บนบอร์ด เว็บไม่รัน AI ไม่เก็บ embedding และยังไม่รับภาพกล้อง

UART มี Node ID, sequence number, readings, checksum, acknowledgement/retry ตาม proposal ให้ Gateway ตรวจความถูกต้องก่อนสร้าง payload เว็บไม่จัดการ UART โดยตรง Gateway เพิ่ม timestamp และพักข้อมูลเมื่อ Cloud ล่ม การส่งข้อมูลย้อนหลังใช้ช่องประวัติแยก ไม่ทำให้ telemetry เก่าถูกแสดงว่าเป็นข้อมูลสด

## Topic ที่เสนอ

แทน `<garageId>` ด้วยรหัสอุปกรณ์ที่ทีมตกลงจริง ตั้งค่าแต่ละ topic ผ่าน `.env` ไม่มีค่าเริ่มต้นในโค้ด MQTT ใช้ browser WSS เท่านั้น

| ประเภท | Topic ร่าง | ผู้ส่ง → ผู้รับ | QoS / retain ที่เสนอ |
|---|---|---|---|
| Telemetry | `draft/smart-garage/<garageId>/telemetry` | Gateway → เว็บ | 0 / false |
| Device status | `draft/smart-garage/<garageId>/status` | Gateway → เว็บ | 0 / true พร้อม timestamp และ heartbeat |
| Command | `draft/smart-garage/<garageId>/command` | เว็บ → Gateway | **0 / false** |
| Acknowledgement | `draft/smart-garage/<garageId>/ack` | Gateway → เว็บ | 0 / false |
| Event | `draft/smart-garage/<garageId>/event` | Gateway → เว็บ | 0 / false |

แยก topic ทั้ง 5 ค่า ไม่อนุญาต wildcard ใน frontend แต่ละเว็บใช้ random clientId และ clean session สั่งงานไม่เก็บคิวตอนขาดการเชื่อมต่อ reconnect ทุก 3 วินาทีแล้ว subscribe ใหม่ MQTT จะปิด connection/session เดิมตอนเปลี่ยนโหมดหรือ unmount จึงไม่มี subscription ค้างใน session

## ข้อตกลง timestamp และ field

ทุก payload ต้องเป็น JSON object UTF-8 ไม่เกิน 16 KB และมี `timestamp` ISO 8601 พร้อม timezone เช่น `2026-11-16T03:00:00.000Z` (10:00 กรุงเทพฯ) เก็บและรับส่งเป็น UTC แสดงผลไทย Asia/Bangkok ทุกที่ Gateway ต้อง sync NTP ข้อมูลเก่ากว่า 15 วินาทีหรือเวลาอนาคตเกิน 5 วินาทีไม่ถือว่าสด หน้าต่าง heartbeat ต้องตกลงตามเครือข่ายจริง ปัจจุบันเสนอส่งทุก 3–5 วินาที

Telemetry ใช้ snapshot: field ที่ขาดหรือ null แปลว่าไม่มีข้อมูล ไม่ใช่ให้คงค่าเดิม กรณีเซนเซอร์ warm-up/invalid ให้ส่ง null ยังไม่ตีความเป็นอากาศดี กรณี ENS160 validity/warm-up เพิ่ม field ภายหลังได้เมื่อทีมตกลง ปัจจุบัน UI แสดงเฉพาะค่าที่รองรับ

| Field | ชนิด / หน่วย / ช่วงที่รับ | ความหมาย |
|---|---|---|
| `door` | enum Closed, Opening, Open, Closing, Fault; null ได้ | ถ้าขาดแสดง Unknown; ไม่มี enum Stopped ใน proposal |
| `occupied` | boolean / null | มีรถจอดหรือว่างจาก logic ฝั่ง Gateway |
| `distanceCm` | number 0–1000 / null, cm | ระยะ Ultrasonic |
| `temperature` | number -40–80 / null, °C | DHT22 |
| `humidity` | number 0–100 / null, %RH | DHT22 |
| `ldrRaw` | number 0–65535 / null, raw ADC | ไม่ใช่ lux ต้องตกลง resolution และ calibration |
| `ledOn` | boolean / null | สถานะ LED จริงจาก Sensor Node |
| `aqi` | number 1–5 / null, index | ENS160 AQI ไม่ใช่ค่ามาตรฐาน AQI 0–500 |
| `tvocPpb` | number 0–10000000 / null, ppb | ENS160 TVOC ช่วงนี้เป็น guard เบื้องต้น ต้องตกลงช่วงตาม firmware |
| `eco2Ppm` | number 0–10000000 / null, ppm | ENS160 **eCO2** ค่าประมาณเทียบเท่า ไม่ใช่ CO2 โดยตรง |
| `commandId` | string / null | คำสั่งที่ทำให้เกิด state นี้ ใช้ยืนยันเฉพาะ ID ที่ตรงกับ pending |

ตัวอย่าง telemetry:

```json
{"timestamp":"2026-11-16T03:00:00.000Z","door":"Closed","occupied":false,"distanceCm":180,"temperature":29.4,"humidity":63,"ldrRaw":1820,"ledOn":false,"aqi":2,"tvocPpb":125,"eco2Ppm":540,"commandId":null}
```

ตัวอย่าง status (แยกจาก broker connection):

```json
{"timestamp":"2026-11-16T03:00:00.000Z","online":true}
```

`online` ต้อง boolean ใช้ heartbeat ที่ timestamp สด เว็บต่อ Broker ได้ไม่ได้หมายถึง Gateway online แนะนำ Gateway ตั้ง LWT offline และส่ง birth/heartbeat online หลังต่อสำเร็จ LWT แบบ timestamp คงที่อาจเก่าแล้วเมื่อ broker ส่ง: เว็บจะใช้ heartbeat timeout ปิดคำสั่งอยู่ดี ต้องตกลงรูปแบบ LWT หรือ backend ที่ stamp เวลาให้ชัดเจน

ตัวอย่าง command:

```json
{"timestamp":"2026-11-16T03:00:01.000Z","commandId":"c61e4ccf-2c27-4692-aef1-189b825e6b33","action":"open"}
```

`action` เป็น open / close / stop, `commandId` เป็น UUID ที่เว็บสร้างใหม่ทุกครั้ง ไม่ retain และไม่ส่งซ้ำอัตโนมัติ Gateway ต้องตรวจ freshness, TTL (เสนอไม่เกิน 8 วินาที), deduplicate commandId และปฏิเสธ retained/stale/replayed command ฝั่ง Gateway เอง การใช้ QoS 0 แปลว่าข้อความอาจหาย ต้องแสดงผลไม่ทราบและให้คนตรวจ ไม่ retry มอเตอร์อัตโนมัติ การเปิดหน้าเว็บไม่ทำให้ส่งคำสั่งใด ๆ

ตัวอย่าง ack: ส่ง accepted หลังตรวจคำสั่ง แล้วส่ง completed หลังทำเสร็จจริง หรือ rejected ถ้าทำไม่ได้

```json
{"timestamp":"2026-11-16T03:00:01.100Z","commandId":"c61e4ccf-2c27-4692-aef1-189b825e6b33","status":"accepted"}
```

```json
{"timestamp":"2026-11-16T03:00:03.900Z","commandId":"c61e4ccf-2c27-4692-aef1-189b825e6b33","status":"completed"}
```

```json
{"timestamp":"2026-11-16T03:00:01.100Z","commandId":"c61e4ccf-2c27-4692-aef1-189b825e6b33","status":"rejected","reason":"motor_fault"}
```

accepted ยัง pending ไม่แปลว่าประตูขยับแล้ว publish สำเร็จไม่ยืนยันผล UI รอ completed/rejected หรือ telemetry final state พร้อม commandId ที่ตรงกัน โดย final state ยืนยัน open → Open, close → Closed เท่านั้น Stop ต้องรอ completed ack ส่วน telemetry ยังคงบอกสถานะจริงเสมอ หาก Stop ระหว่าง pending จะแทนคำสั่งก่อนและไม่ยอมให้ ack เก่าล้าง pending ของ Stop

ครบ 8 วินาทีไม่มีการยืนยัน: แสดง timeout ไม่ทราบผล ไม่ส่งซ้ำ ไม่สรุปว่าประตูหยุดหรือเปิดสำเร็จ ประตูที่ข้อมูลสดแต่ยัง Opening/Closing จะไม่อนุญาตเปิด/ปิดเพิ่ม มี Stop ให้ขอหยุดเมื่อ connection ยังสด ข้อมูลเก่าหรือ Gateway offline ปิดการส่งทุกคำสั่งรวม Stop ใช้ปุ่มฮาร์ดแวร์แทน

ตัวอย่าง event:

```json
{"timestamp":"2026-11-16T03:00:04.000Z","type":"ACCESS_GRANTED","message":"บอร์ดยืนยันผู้มีสิทธิ์เข้าใช้งาน","commandId":null}
```

`type` และ `message` เป็น string; `commandId` optional ใช้เชื่อมเหตุการณ์ (ไม่ใส่ข้อมูลใบหน้าหรือชื่อจริงโดยไม่จำเป็น) React render ข้อความเป็นข้อความธรรมดา ไม่ render HTML จาก payload

## JSON Schema ร่าง

ตัวอย่าง JSON Schema Draft 2020-12 สำหรับ envelope ของแต่ละชนิด ใช้ร่วมกับ field table และตัวตรวจ runtime ใน `src/services/protocol.js` การ validate schema แบบเต็มยังไม่ใช่ dependency ของเว็บ ร่างนี้แยกชนิดตาม topic ไม่ต้องมี type field ใน payload

```json
{
  "$schema":"https://json-schema.org/draft/2020-12/schema",
  "$defs":{
    "base":{"type":"object","required":["timestamp"],"properties":{"timestamp":{"type":"string","format":"date-time"}}},
    "telemetry":{"allOf":[{"$ref":"#/$defs/base"},{"properties":{
      "door":{"enum":["Closed","Opening","Open","Closing","Fault",null]},
      "temperature":{"type":["number","null"],"minimum":-40,"maximum":80},
      "humidity":{"type":["number","null"],"minimum":0,"maximum":100},
      "ldrRaw":{"type":["number","null"],"minimum":0,"maximum":65535},
      "distanceCm":{"type":["number","null"],"minimum":0,"maximum":1000},
      "occupied":{"type":["boolean","null"]},"ledOn":{"type":["boolean","null"]},
      "aqi":{"type":["number","null"],"minimum":1,"maximum":5},
      "tvocPpb":{"type":["number","null"],"minimum":0,"maximum":10000000},
      "eco2Ppm":{"type":["number","null"],"minimum":0,"maximum":10000000},
      "commandId":{"type":["string","null"]}
    }}]},
    "status":{"allOf":[{"$ref":"#/$defs/base"},{"required":["online"],"properties":{"online":{"type":"boolean"}}}]},
    "command":{"allOf":[{"$ref":"#/$defs/base"},{"required":["commandId","action"],"properties":{"commandId":{"type":"string","minLength":1},"action":{"enum":["open","close","stop"]}}}]},
    "ack":{"allOf":[{"$ref":"#/$defs/base"},{"required":["commandId","status"],"properties":{"commandId":{"type":"string","minLength":1},"status":{"enum":["accepted","completed","rejected"]},"reason":{"type":"string"}}}]},
    "event":{"allOf":[{"$ref":"#/$defs/base"},{"required":["type","message"],"properties":{"type":{"type":"string"},"message":{"type":"string"},"commandId":{"type":["string","null"]}}}]}
  }
}
```

เลือก `$defs` ตาม topic ใน schema validator ของทีมก่อนนำไปใช้ ร่างยังไม่ล็อก additionalProperties เพื่อให้เพิ่ม version/nodeId/sequence ได้ภายหลัง ต้องทบทวนร่วมกับ firmware

## Google Sheets ผ่าน HTTPS

ข้อเสนอ interface ฝั่งเว็บ: `adapter.load(): Promise<{source, events, samples}>` โดย events เป็น `{id,timestamp,type,message}` และ samples ใช้ telemetry shape ข้างต้น ปัจจุบัน `mockHistoryAdapter` คืนตัวอย่างเฉพาะ Demo ส่วน MQTT ใช้ `unconfiguredHistoryAdapter` คืนรายการว่างและระบุว่ายังไม่เชื่อม Sheets

โครงอนาคต: Gateway ส่งข้อมูลดิบและ Event Log ผ่าน HTTPS ไป endpoint ที่ทีมควบคุม → ผู้ให้บริการฝั่ง server เขียน Google Sheets พร้อม deduplicate โดย deviceId/sequence/eventId และ timestamp เดิม เว็บเรียก read-only backend ที่ตรวจสิทธิ์และจำกัดจำนวน/ช่วงเวลา ห้าม embed service-account key ใน frontend ต้องตกลง CORS, authentication, rate limit, pagination, schema คอลัมน์, timezone, id สำหรับกันข้อมูลซ้ำ และการ merge MQTT สดกับประวัติย้อนหลัง รวมถึง buffer/retry เฉพาะข้อมูลประวัติ ไม่ใช่คำสั่งมอเตอร์

## LINE Messaging API

ให้ Gateway เรียก backend ผ่าน HTTPS ที่ยืนยันสิทธิ์ แล้ว backend ส่ง LINE ด้วย token ที่เก็บฝั่ง server หรือให้ Gateway ส่งเองหากทีมจัดเก็บ token อย่างเหมาะสมบนอุปกรณ์ ไม่ส่ง token ให้ browser ต้องตกลง recipient, event ที่แจ้ง, cooldown กันแจ้งซ้ำ และนโยบายข้อมูลส่วนบุคคล รอบนี้ไม่สร้างการส่ง LINE จริง

## ต้องขอข้อมูลจากเพื่อน Gateway ก่อนเชื่อมจริง

- Broker URL พร้อม path ของ WSS, TLS, allowed origins, port และข้อจำกัด connection
- authentication ที่เหมาะกับ browser, สิทธิ์อายุสั้น, ACL: เว็บ publish ได้เฉพาะ command ของอุปกรณ์ตัวเอง และ subscribe ได้เฉพาะ 4 topic ของตัวเอง; Gateway มีสิทธิ์กลับด้าน ห้าม broker สาธารณะไร้ ACL ควบคุมมอเตอร์
- Topic 5 ค่า / deviceId / version ที่ใช้จริง, payload ตัวอย่างจริงทุกชนิด รวม null, warm-up และ error
- heartbeat interval, LWT/birth, NTP, sequence และนโยบายข้อมูล out-of-order / clock skew
- นิยาม accepted/completed/rejected, correlation commandId, stop semantics, timeout และ TTL, dedup/replay protection และ retained command rejection
- Door interlock / obstruction detection, movement timeout, การรีเซ็ต Fault และปุ่มหยุดฮาร์ดแวร์ ต้องไม่อาศัยพื้นที่จอดว่างเป็นหลักฐานว่าทางประตูปลอดภัย ยังไม่เพิ่ม automatic closing
- LDR resolution/calibration, ENS160 validity และหน่วย, ระยะ Ultrasonic และ threshold รถที่วัดจริง
- HTTPS endpoints, Sheet columns, สิทธิ์อ่าน/เขียน, timestamp, buffer/retry และการกันแถวซ้ำ
- ผู้ดูแล LINE token / backend, กลุ่มผู้รับและเหตุการณ์แจ้งเตือน
- วิธีแสดงกล้องที่ทีมเลือกภายหลัง ปัจจุบันไม่มี endpoint กล้องและไม่มี face AI บนเว็บ

ทดสอบบนโต๊ะโดยถอดกลไกอันตรายและใช้ mock actuator ก่อนทดสอบ servo จริง ทดสอบ network loss, stale retained status, JSON เสีย, duplicate command, timeout, stop และการ reconnect บน broker ที่ทีมควบคุมก่อนเปิดใช้งาน
