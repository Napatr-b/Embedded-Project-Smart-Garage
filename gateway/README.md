# Gateway Firmware สำหรับ PlatformIO

เปิดโฟลเดอร์นี้ใน VS Code อีกหน้าต่างจากเว็บ ติดตั้ง extension PlatformIO IDE ผู้เผยแพร่ PlatformIO จาก Marketplace แล้วรอ Core/tools ติดตั้งครบ อ่าน [คู่มือหลัก](../../README.md) และ [เหตุผลเลือก framework](../../docs/framework-decision.md) ก่อน upload

## Build โดยไม่ใช้บอร์ด

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e gateway_bringup -j 4
& $pio run -e gateway_mqtt -j 4
& $pio run -e gateway_simulated -j 4
```

ค่าเริ่มต้น gateway_bringup ไม่เปิด network และไม่มี actuator driver ทำงาน `gateway_mqtt` ใช้ sensor null/Unknown ส่วน `gateway_simulated` เป็นฮาร์ดแวร์จำลองเท่านั้น ไม่มี real hardware environment ในรอบนี้

หากไม่มี config.local.h จะใช้ config.example.h ที่ไม่มีรหัส การ build ยังผ่านได้ แต่ network จะไม่เชื่อมต่อ เมื่อจะตั้งค่าจริงให้ `Copy-Item include/config.example.h include/config.local.h` และกรอกในเครื่อง อย่า commit ไฟล์จริง

## Host logic tests (ไม่ใช่ ESP32 test)

ไฟล์ `test/host/logic_test.cpp` เรียก DoorController และ CommandGuard ตัวเดียวกับ firmware ใช้ C++17 compiler บน Windows หากมี Zig 0.15.2 หรือ g++ แล้ว จากรากโครงการ:

```powershell
.\tools\test-firmware.ps1 -Compiler 'zig'
# หรือระบุ full path ไปยัง zig.exe / g++.exe ที่ติดตั้งจริง
```

ถ้า PowerShell policy บล็อกไฟล์ .ps1 ไม่ต้องลด security policy ให้รันคำสั่ง compiler โดยตรงจากรากแทน:

```powershell
zig c++ -std=c++17 -Igateway/include gateway/test/host/logic_test.cpp gateway/src/door_controller.cpp -o tmp/gateway-logic-tests.exe
.\tmp\gateway-logic-tests.exe
```

เครื่องที่พัฒนานี้ติดตั้ง compiler ชั่วคราวใน tmp ผ่าน Python ของ PlatformIO จึงไม่ต้องแก้ PATH ถาวร หากต้องทำซ้ำด้วยตัวเอง (ต้องมีอินเทอร์เน็ต):

```powershell
$python = "$env:USERPROFILE\.platformio\penv\Scripts\python.exe"
& $python -m pip install ziglang==0.15.2 --target tmp/host-compiler
.\tools\test-firmware.ps1 -Compiler '.\tmp\host-compiler\ziglang\zig.exe'
```

`protocol_test.cpp` ทดสอบ parser C++ จริงกับ cJSON ของ ESP-IDF ต้อง compile cJSON.c ของ framework เป็น object แล้ว link กับ protocol.cpp โดยใช้ timezone UTC ตัวอย่างคำสั่ง/ผลจริงอยู่ใน docs/verification.md ไม่ต้องมี compiler host เพื่อ build firmware ESP32; toolchain ข้ามสถาปัตยกรรมถูก PlatformIO ดาวน์โหลดเอง

## Upload และพอร์ต

ยังไม่ upload อัตโนมัติ ใช้ `$pio device list` และ Device Manager เปรียบเทียบก่อน/หลังเสียบสาย เลือกพอร์ตของบอร์ดจริงผ่าน `$port = Read-Host 'พอร์ตจริงของบอร์ด'` แล้วจึง `& $pio run -e gateway_bringup -t upload --upload-port $port` เปิด monitor `& $pio device monitor --port $port --baud 115200` หยุด Ctrl+C

ตรวจ Serial ต้องพบ Flash/PSRAM อย่างละ 8388608 bytes; ไม่อาศัยชื่อ board preset ว่า No PSRAM มายืนยันฮาร์ดแวร์ ตรวจฉลาก N8R8 จริงด้วย ไม่ต่อ Servo เพื่อทดสอบ boot

## ไฟล์ที่ควรอ่าน

- `include/config.h` / `config.example.h`: network gates และตัวอย่างที่ไม่มี secret
- `include/pins.h`: ทุกขา -1 ยังไม่กำหนด
- `include/hardware.h`: interfaces/disabled adapters ของ Sensor UART, IR, Ultrasonic, Servo, Face Recognition
- `src/door_controller.cpp`: Stop → Fault ใน simulation, conflict และ movement timeout
- `src/protocol.cpp`: envelope, canonical timestamp และ JSON validation
- `src/network.cpp`: Wi-Fi/SNTP, TLS, Last Will, queue และ MQTT
- `include/external_services.h`: interface-only สำหรับ history/LINE ไม่ทำ HTTPS request จริง

ไม่มี AI หรือ driver จริงซ่อนอยู่ในคำว่า disabled อย่าสรุปว่าเติมขาแล้วใช้ ESP-WHO/Servo ได้ ต้อง implement และทดสอบ hardware integration ตาม [wiring.md](../../docs/wiring.md) ก่อน
