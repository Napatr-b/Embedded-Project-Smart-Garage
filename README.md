# Smart Garage: CEDT Embedded Systems Final Project 2026

Miniature smart garage: a sliding door that opens for enrolled faces (on-device Face Recognition on ESP32-S3), bay occupancy sensing, automatic lighting, environmental logging to the cloud, a web dashboard and LINE alerts.

## เริ่มใช้งานระบบจอดรถ

1. สร้าง Supabase project แล้วรัน [SQL migration](supabase/migrations/20261118000000_smart_garage.sql) ใน SQL Editor ของโปรเจกต์ที่ตั้งใจใช้
2. คัดลอก `dashboard/.env.example` เป็น `.env.local` ตั้ง `SUPABASE_URL`, `SUPABASE_SECRET_KEY`, `PARKING_GATEWAY_ID` และ `PARKING_DEVICE_TOKEN` รัน `npm.cmd ci` และ `npm.cmd run dev` จาก `dashboard`
3. เมื่อ deploy เว็บบน Vercel ให้ตั้ง Root Directory เป็น `dashboard` และเก็บค่าฝั่ง Backend ใน Project Environment Variables ตั้ง `GARAGE_PARKING_URL` บน ESP32 เป็น HTTPS URL ที่ลงท้ายด้วย `/api/parking` และตั้ง `GARAGE_PARKING_TOKEN` ให้ตรงกับ Backend ในไฟล์ `gateway/include/config.local.h`
4. ใช้ `npm.cmd test` และ `npm.cmd run build` จาก `dashboard` แล้วทดลองส่งรถเข้า รถออก เหตุการณ์ซ้ำ และเหตุการณ์ย้อนหลังตาม [คู่มือตั้งค่าและทดสอบ API](docs/parking-mvp.md)

## โครงสร้างโปรเจกต์

```
gateway/        ESP32-S3 N16R8, PlatformIO + ESP-IDF
sensor-node/    ESP32 DOIT DevKit, PlatformIO + Arduino
shared/         UART protocol shared by both firmwares
cloud/          บันทึกการตั้งค่า MQTT / LINE
supabase/       PostgreSQL migrations
dashboard/      Web dashboard (Vercel)
docs/           Design handoff, proposal, protocol, pin map, test plan
```

## Getting started

1. Install VS Code + PlatformIO.
2. Open `gateway/` or `sensor-node/` as a PlatformIO project, then Build / Upload.
3. Copy `gateway/include/config.example.h` to `config.local.h` (git-ignored) and fill it in. **Never commit secrets.**

## Team

| Member | Role |
|---|---|
| TBD | System Architect & Integration |
| TBD | Embedded Developer (Sensor Node) |
| TBD | Embedded Developer (Gateway & Cloud) |
| TBD | UI/UX & Dashboard |
| TBD | Project Manager & Test Lead |

## Credits and AI tool declaration

- Third-party code and libraries: list here (e.g. ESP-WHO, Adafruit DHT).
- AI tools used: list tools and which parts they helped with (required by section 4.6).


## Dashboard / Gateway import

See [import and run notes](docs/LOCAL-IMPORT.md) for the imported dashboard, N16R8 Gateway, local configuration exclusions, and integration limitations.
