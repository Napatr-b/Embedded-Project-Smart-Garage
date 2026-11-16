# Gateway Firmware สำหรับ PlatformIO

เปิดโฟลเดอร์นี้ใน VS Code อีกหน้าต่างจากเว็บ ติดตั้ง extension PlatformIO IDE ผู้เผยแพร่ PlatformIO จาก Marketplace แล้วรอ Core/tools ติดตั้งครบ อ่าน [คู่มือหลัก](../README.md) และ [เหตุผลเลือก framework](../docs/framework-decision.md) ก่อน upload

## Build โดยไม่ใช้บอร์ด

```powershell
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe"
& $pio run -e gateway_bringup -j 4
& $pio run -e gateway_mqtt -j 4
& $pio run -e gateway_simulated -j 4
```

ทุก env ใช้บอร์ด N16R8 เดียวกัน:
- `gateway_bringup` (ค่าเริ่มต้น): ไม่เปิด network รับข้อมูลจาก Sensor Node ทาง UART1 (TX=17, RX=18) แล้ว log ทาง Serial ใช้ทดสอบสาย UART
- `gateway_mqtt`: ข้อมูลจริงจาก Node ส่งขึ้น MQTT (ค่าที่ Node ไม่ได้ส่งเป็น null)
- `gateway_simulated` / `gateway_local_n16r8`: ฮาร์ดแวร์จำลอง สำหรับทำ dashboard

ยังไม่มี actuator driver จริงในทุก env

**Windows: path ห้ามมีช่องว่าง** ESP-IDF build ไม่ได้ถ้า path ของโปรเจกต์มี space (เช่น `C:\Users\ASUS Vivobook\...`) ให้ map เป็น drive ก่อน: `subst G: "<path ของ repo>"` แล้ว build จาก `G:\gateway`

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

ตรวจ Serial ต้องพบ Flash=16777216 และ PSRAM=8388608 bytes ไม่อาศัยชื่อ board preset มายืนยันฮาร์ดแวร์ ตรวจฉลาก N16R8 จริงด้วย ไม่ต่อ Servo เพื่อทดสอบ boot

## ไฟล์ที่ควรอ่าน

- `include/config.h` / `config.example.h`: network gates และตัวอย่างที่ไม่มี secret
- `include/pins.h`: ทุกขา -1 ยังไม่กำหนด
- `include/hardware.h`: SensorSnapshot + sampleHardware (ค่าจำลอง; ยังไม่มี driver จริง)
- `src/door_controller.cpp`: Stop → Fault ใน simulation, conflict และ movement timeout
- `src/protocol.cpp`: envelope, canonical timestamp และ JSON validation
- `src/network.cpp`: Wi-Fi/SNTP, TLS, Last Will, queue และ MQTT

ไม่มี AI หรือ driver จริงซ่อนอยู่ในคำว่า disabled อย่าสรุปว่าเติมขาแล้วใช้ ESP-WHO/Servo ได้ ต้อง implement และทดสอบ hardware integration ตาม [wiring.md](../docs/wiring.md) ก่อน
