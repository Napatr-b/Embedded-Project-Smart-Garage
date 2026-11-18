# ผลการตรวจฉบับ Web + Gateway

วันที่ 16 พฤศจิกายน 2569 เขตเวลา Asia/Bangkok

## ผลที่ตรวจจริง

| ส่วน | ผล | ขอบเขต |
|---|---|---|
| เว็บเดิมก่อนย้าย | build ผ่าน | จึงย้าย source/dependencies ไป dashboard/ โดยรักษาไฟล์เดิม |
| `npm.cmd test --prefix web` | 16 tests ผ่าน / 0 failed | logic บน Node ไม่ใช่ browser end-to-end |
| `npm.cmd run build --prefix web` | ผ่าน | Vite production build; แยก MQTT lazy chunk |
| `node tools/sync-contract.mjs --check` | ผ่าน | ค่ากลาง JS/C++ ตรง docs/contract.json |
| `gateway_bringup` | SUCCESS | PlatformIO 6.12.0 + ESP-IDF 5.5.0 |
| `gateway_mqtt` | SUCCESS | build ด้วย config ตัวอย่างว่าง ไม่มีการเชื่อม broker จริง |
| `gateway_simulated` | SUCCESS | โหมดจำลองฮาร์ดแวร์ ไม่มี Servo จริง |
| Host C++ DoorController/CommandGuard | ผ่าน | source เดียวกับ firmware, Zig 0.15.2 บน Windows |
| Host C++ JSON parser | ผ่าน | protocol.cpp จริง + cJSON จาก IDF 5.5.0 |
| HTTP preview หลังย้าย | 200 | `http://127.0.0.1:5174/` ระหว่าง dev server ทำงาน |

Build เฟิร์มแวร์ฉบับสุดท้ายทั้ง 3 environment ใช้เวลารวมประมาณ 50 วินาทีหลัง cache พร้อมแล้ว ครั้งแรกใช้เวลาหลายนาทีเพื่อดาวน์โหลด/compile SDK คำสั่งไม่มี `-t upload` และไม่ได้เปิด serial port

## Logic ที่ทดสอบ

Web: JSON เสีย/null/field ผิดชนิด/ค่าขาด, schemaVersion และ deviceId ไม่ตรง, offline LWT timestamp null, timeSynced gate, heartbeat/telemetry เก่า, clock อนาคต, commandId ไม่ตรง, received/accepted ยัง pending, completed ack อย่างเดียวไม่ยืนยันตำแหน่ง, ต้องได้ state พร้อม feedback, timeout ไม่ retry, Stop แทน pending, packet ซ้ำ และ Stop ใหม่ที่เวลาเท่ากับ packet ก่อนหน้า

ทดสอบ mqtt.js ผ่าน fake transport: QoS 0 / retain false, ไม่ queue QoS 0 ตอน offline, reconnect subscribe ใหม่, ไม่รับ retained ack, cleanup connection/listeners และ SUBACK ปฏิเสธต้องไม่แสดง connected; ไม่ใช่การเชื่อม broker จริง

C++: คำสั่งซ้ำ, replay หลัง bounded cache eviction, timestamp หมดอายุ/อนาคต, clock ไม่ sync, retained command, actuator disabled, unknown, คำสั่งเคลื่อนซ้อน, Stop → Fault, movement timeout และ offline → Fault; parser ตรวจ JSON/อุปกรณ์/schema/action/date/canonical UTC ผิด, payload เกิน 1024 bytes และ nesting เกิน 4 ก่อน recursive parser; SNTP flag ต้องถูกยืนยันก่อน clockSynced (ทดสอบ callback marker บน host ไม่ได้อ้างว่า SNTP จากบอร์ดผ่านจริง)

## Config ที่ตรวจจากผล build

`framework-espidf@3.50500.0` ยืนยัน IDF 5.5.0; compiler `14.2.0+20241119`; CMake 3.30.2; esptool 4.9.0; Flash size 8MB; SPIRAM enabled + Octal + 80MHz + boot init + memory test; partition binary สร้างผ่านและขอบเขตสิ้นสุด 0x800000

ชื่อ preset ของ PlatformIO ยังคงเป็น N8 / No PSRAM ไม่ใช่ผลตรวจ memory บนบอร์ด ใน SDK config QIO=y แต่ image header ของ IDF อาจระบุ DIO ขณะ bootloader เปิดโหมด Quad ตามการตั้งค่า ไม่ใช้ข้อความ header เพียงอย่างเดียวอ้างว่าได้ทดสอบชิปจริงแล้ว

พบและแก้ปัญหาระหว่างทำ: ติดตั้ง toolchain ผ่านสิทธิ์ดาวน์โหลดที่อนุญาต; Windows host ไม่มี gmtime_r จึงใช้ gmtime_s เฉพาะ host; เลือก IDF components แบบ minimal ต้องคง `__pio_env` ของ PlatformIO; เพิ่ม SNTP sync callback gate และ feedback/ack correlation; จำกัด JSON nesting; แยก atomic connection flag เพื่อให้ queue เต็มไม่ทำให้อ้าง online ค้าง

## ยังไม่ยืนยัน / ไม่ได้ทำ

- ไม่ได้ flash, เปิด serial port, ตรวจ memory บนบอร์ดจริง หรือขยับ Servo
- ไม่ได้ทดสอบ Wi-Fi, SNTP, TLS certificate, broker authentication/ACL, Last Will timing และ reconnect บนเครือข่ายจริง config ยังว่าง
- ไม่ได้ทดสอบ GPIO/UART/IR/Ultrasonic/Servo/feedback/hardware stop/OV2640/ESP-WHO จริง disabled interfaces ไม่ใช่ driver ที่ใช้งานจริงแล้ว
- LINE ยังเป็น config ตัวอย่าง ส่วนการตั้งค่า Supabase สำหรับสถานะจอดรถอยู่ใน [คู่มือ](parking-mvp.md)
- ไม่ได้ deploy Vercel
- ยังไม่ได้ยืนยันภาพ desktop/mobile หรือ keyboard ผ่าน browser automation ในรอบนี้ รอบก่อน browser access host ไม่พร้อม และ Computer Use ยุติการเข้าถึง Chrome เพราะระบุ URL ไม่ได้แน่ชัด รอบนี้ตรวจ HTTP/build และให้ URL ระหว่างทำ ไม่อ้างว่า screenshot/visual QA ผ่าน

## ทำซ้ำ

จากรากโครงการ:

```powershell
node tools/sync-contract.mjs --check
npm.cmd test --prefix web
npm.cmd run build --prefix web
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -d gateway -e gateway_bringup -e gateway_mqtt -e gateway_simulated -j 4
.\tools\test-firmware.ps1 -Compiler '.\tmp\host-compiler\ziglang\zig.exe'
```

Host compiler ใน tmp เป็นเครื่องมือชั่วคราว ไม่จำเป็นสำหรับ build ESP32 และไม่ถูก commit ดูวิธีติดตั้ง/ใช้ compiler ที่มีอยู่ใน gateway/README.md หาก PowerShell บล็อก .ps1 ให้รัน compiler โดยตรง ไม่ต้องลด security policy

ตัวอย่าง parser test โดยตรงหลัง PlatformIO ดาวน์โหลด IDF แล้วและมี Zig:

```powershell
$env:TZ='UTC0'
$taskIdf = "$env:USERPROFILE\.platformio\packages\framework-espidf"
zig cc -c "$taskIdf/components/json/cJSON/cJSON.c" -o tmp/cjson.o
zig c++ -std=c++17 -Igateway/include "-I$taskIdf/components/json/cJSON" gateway/test/host/protocol_test.cpp gateway/src/protocol.cpp tmp/cjson.o -o tmp/gateway-protocol-tests.exe
.\tmp\gateway-protocol-tests.exe
```

ก่อนสาธิตจริงให้รัน browser demo ตาม docs/demo-script.md และตรวจ Chrome desktop/Responsive 390×844 ด้วยคีย์บอร์ด หลังต่อบอร์ดด้วยตัวเองจึงค่อยตรวจ Serial → Wi-Fi → MQTT → Web → อุปกรณ์ ตาม README
