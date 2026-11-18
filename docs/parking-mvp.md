# Smart Garage · ตั้งค่าช่องจอดรถ

Ultrasonic บน Sensor Node ส่งระยะผ่าน UART ให้ ESP32 Gateway เมื่อสถานะเปลี่ยน Gateway ส่งเหตุการณ์ไปที่ `POST /api/parking` ผ่าน HTTPS เพื่อบันทึกลง Supabase PostgreSQL แม้ไม่มีใครเปิด Dashboard หน้าเว็บอ่าน `GET /api/parking` ทุก 5 วินาทีและแสดงเวลาไทย เมนู **แดชบอร์ด MQTT** เปิดหน้า telemetry และคำสั่งประตูเดิม

## สร้าง Supabase project และใช้ migration

1. สร้างโปรเจกต์ใน Supabase และเปิด Data API สำหรับ schema `public`
2. ใน **SQL Editor** ของโปรเจกต์ที่ต้องการใช้ ให้วางและรัน [SQL migration](../supabase/migrations/20261118000000_smart_garage.sql) หนึ่งครั้ง เพื่อสร้าง `garage_events`, `sensor_readings` และ `door_commands` พร้อมเปิด RLS และกำหนดสิทธิ์ Backend
3. คัดลอก Project URL และ Secret key แบบ `sb_secret_…` จาก Data API และ API Keys ตามลำดับ [คู่มือ Supabase API Keys](https://supabase.com/docs/guides/getting-started/api-keys)

Backend บันทึกเหตุการณ์ `OCCUPANCY_CHANGED` ด้วย `source=ULTRASONIC` และสถานะที่ Gateway วัดได้ใน `state_to` โดยเว้น `state_from`, `reason_code` และ `command_id` ไว้ ตารางเซนเซอร์และคำสั่งประตูเตรียมไว้สำหรับเพิ่มการบันทึกภายหลัง

## ตั้งค่าเว็บและ deploy

ใช้ Node.js 22 จากโฟลเดอร์ `dashboard` รัน `npm.cmd ci` แล้วคัดลอก `.env.example` เป็น `.env.local`

| ตัวแปร Backend | ค่า |
| --- | --- |
| `SUPABASE_URL` | Project URL เช่น `https://PROJECT_REF.supabase.co` |
| `SUPABASE_SECRET_KEY` | Secret key ของโปรเจกต์ |
| `PARKING_GATEWAY_ID` | รหัสเดียวกับ `GARAGE_DEVICE_ID` ค่าเริ่มต้นคือ `garage-01` |
| `PARKING_DEVICE_TOKEN` | ค่าสุ่มที่ตรงกับ `GARAGE_PARKING_TOKEN` บน Gateway |

รัน `npm.cmd run dev` แล้วเปิด URL ที่ Vite แสดง สำหรับทดสอบโดยไม่ใช้ Supabase ให้ตั้ง `PARKING_MOCK=1` และตั้ง device token ไว้ด้วย ข้อมูลจำลองอยู่ในหน่วยความจำและหายเมื่อรีสตาร์ต Vite

บน Vercel ตั้ง **Root Directory** เป็น `dashboard` ใช้ Vite preset และตั้งตัวแปร Backend ทั้งสี่ใน Project Environment Variables เก็บ secret ฝั่งเซิร์ฟเวอร์ ห้ามตั้งชื่อขึ้นต้นด้วย `VITE_` หรือ commit ไฟล์ตั้งค่าจริง

## ตั้งค่า Gateway ESP32

1. เปิด Ultrasonic บน Sensor Node และต่อ UART ตาม [ผังขา](pin-map.md) ติดตั้งเซนเซอร์ให้มีพื้นผิวสะท้อนคลื่นกลับเมื่อช่องจอดว่าง
2. คัดลอก `gateway/include/config.example.h` เป็น `config.local.h` แล้วตั้ง Wi-Fi, `GARAGE_DEVICE_ID`, `GARAGE_PARKING_URL` เป็น URL HTTPS เต็มที่ลงท้ายด้วย `/api/parking` และ `GARAGE_PARKING_TOKEN` ให้ตรงกับ Backend
3. ปรับ `GARAGE_PARKING_OCCUPIED_CM`, `GARAGE_PARKING_VACANT_CM` และ `GARAGE_PARKING_STABLE_SAMPLES` ให้ตรงกับหน้างาน ค่าเริ่มต้นคือ 3 ค่าต่อเนื่องที่ ≤ 60 ซม. สำหรับมีรถ และ ≥ 80 ซม. สำหรับว่าง
4. ใน `gateway` รัน `pio run -e gateway_mqtt` เพื่อคอมไพล์ และ `pio run -e gateway_mqtt -t upload` เพื่อแฟลช จากนั้นใช้ `pio device monitor` ดูผล POST

Gateway รอ SNTP ก่อนส่งสถานะเสถียรครั้งแรกและทุกครั้งที่เปลี่ยน ใช้ certificate bundle ตรวจ TLS และส่ง payload เดิมซ้ำเมื่อยังไม่ได้ HTTP 200 หรือ 201 คิวใน RAM เก็บได้ 16 รายการนอกเหนือจากรายการที่กำลังส่ง และข้อมูลในคิวหายเมื่อรีบูต

## ทดสอบ API

จาก `dashboard` ใช้ `npm.cmd test` และ `npm.cmd run build` สำหรับทดสอบ HTTP ให้ตั้ง `PARKING_MOCK=1` และ `PARKING_DEVICE_TOKEN=local-mvp-token` ใน `.env.local` ก่อนเปิด Vite แล้วรันตัวอย่างนี้ใน PowerShell อีกหน้าต่าง หากทดสอบกับ Supabase ให้ใช้โปรเจกต์ทดสอบและปรับ URL, token และ Gateway ให้ตรงกับระบบนั้น

```powershell
$url = 'http://127.0.0.1:5173/api/parking'
$headers = @{ Authorization = 'Bearer local-mvp-token' }
$now = [DateTimeOffset]::UtcNow
function Send-Parking($eventId, $occupied, $time) {
  $body = @{
    event_id = $eventId
    gateway_id = 'garage-01'
    occupied = $occupied
    occurred_at = $time.UtcDateTime.ToString('yyyy-MM-ddTHH:mm:ss.fffZ')
  } | ConvertTo-Json
  Invoke-RestMethod $url -Method Post -Headers $headers -ContentType 'application/json' -Body $body
}

$entryId = [guid]::NewGuid().ToString()
$exitId = [guid]::NewGuid().ToString()
$olderId = [guid]::NewGuid().ToString()
Send-Parking $entryId $true ($now.AddSeconds(-1))
Invoke-RestMethod $url
Send-Parking $exitId $false $now
Send-Parking $exitId $false $now
Send-Parking $olderId $true ($now.AddSeconds(-2))
Invoke-RestMethod $url
```

GET ครั้งแรกคืน `OCCUPIED` และครั้งสุดท้ายคืน `VACANT` โดยหน้าเว็บแสดง “ว่าง” ภายในรอบ polling เหตุการณ์ใหม่คืน HTTP 201 เหตุการณ์เดิมคืน 200 พร้อม `duplicate: true` และ ID เดิมที่เปลี่ยนข้อมูลคืน 409 ตรวจใน `garage_events` ด้วย ID ทั้งสาม ควรพบ 3 แถว

`occurred_at` รับ ISO 8601 แบบมี timezone และทศนิยมวินาทีสูงสุด 6 หลัก GET อ่านเฉพาะ Gateway ที่ตั้งไว้ เรียงตาม `occurred_at` ล่าสุด ตามด้วย `ingested_at` และ `event_id` เมื่อเวลาเท่ากัน ถ้ายังไม่มีเหตุการณ์จะคืน `UNKNOWN` และแสดง “ยังไม่มีข้อมูล”
