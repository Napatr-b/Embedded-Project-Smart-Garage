# เหตุผลเลือก PlatformIO / ESP-IDF และสถานะ ESP-WHO

ตรวจเอกสารทางการวันที่ 16 พฤศจิกายน 2569 ไม่ใช้คำว่า build ผ่านแทนทดสอบกับบอร์ดจริง

## ชุดที่เลือกสำหรับ Gateway พื้นฐาน

| รายการ | รุ่น / การตั้งค่า | หลักฐาน |
|---|---|---|
| PlatformIO Core ที่พบในเครื่อง | 6.2.0 | `platformio --version` |
| Platform | `platformio/espressif32@6.12.0` | pin exact ใน platformio.ini |
| Framework | ESP-IDF 5.5.0; package `platformio/framework-espidf@3.50500.0` | tag platform.json ระบุแพ็กเกจตระกูล 3.50500; build output ยืนยัน 5.5.0 |
| Compiler | Xtensa ESP GCC `14.2.0+20241119` | dependency ของ platform 6.12.0 และ resolved build output |
| MQTT / JSON / Wi-Fi / TLS | component ที่มากับ IDF 5.5.0 | ไม่มีไลบรารี Arduino หรือ external lib_deps เพิ่ม |
| ESP-WHO | **ไม่รวมใน build** | เก็บเฉพาะ interface ที่คืน Unavailable |

เลือก release 6.12.0 ที่ตรวจความสัมพันธ์กับ ESP-IDF 5.5.0 แล้ว ไม่ใช้ floating latest แม้ PlatformIO ปัจจุบันมี release ใหม่กว่า จึงไม่อัปเกรด framework ทั้งโครงการโดยไม่มีการทดสอบ รุ่นใหม่กว่าไม่ได้แปลว่าใช้ไม่ได้ แต่ต้องตรวจ API/BSP/dependency ชุดใหม่ก่อน

แหล่งอ้างอิงหลัก: [PlatformIO 6.12.0 manifest](https://github.com/platformio/platform-espressif32/blob/v6.12.0/platform.json), [release 6.12.0](https://github.com/platformio/platform-espressif32/releases/tag/v6.12.0), [PlatformIO ESP-IDF](https://docs.platformio.org/en/latest/frameworks/espidf.html)

## N16R8 ไม่ใช่ค่า default ของ board profile

เอกสาร [Espressif DevKitC-1](https://documentation.espressif.com/api/resource/path/docs/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/user_guide_v1.0.html) ระบุ N8R8 เป็น Flash 8 MB Quad SPI และ PSRAM 8 MB Octal SPI ส่วน [โปรไฟล์ esp32-s3-devkitc-1 ของ PlatformIO](https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html) ระบุ N8 ไม่มี PSRAM

บอร์ดจริงของกลุ่มคือ N16R8 (Flash 16 MB, PSRAM 8 MB Octal) ใช้ board file `gateway/boards/yd-esp32-s3-n16r8.json` จึงตั้งใน sdkconfig.defaults: Flash 16 MB, QIO 80 MHz; SPIRAM enabled, Octal, 80 MHz, boot init และ memory test `IGNORE_NOTFOUND` ให้ Serial แจ้งปัญหาถ้าไม่พบ แทนการสรุปว่า RAM ถูกต้องจากชื่อบอร์ด ต้องอ่านค่าจริงตอน boot: Flash=16777216 bytes และ PSRAM=8388608 bytes ถ้าผิดต้องตรวจฉลาก module, การจ่ายไฟและ config ก่อนต่อ hardware

Partition เป็น nvs 0x9000/0x6000, phy 0xf000/0x1000, factory 0x10000/0x400000, storage 0x410000/0x3f0000 สิ้นสุดที่ 0x800000 (8 MB แรกของ flash 16 MB ที่เหลือยังว่าง) ไม่มี OTA slots ในรอบนี้ ไม่อ้างว่ามี OTA แล้ว storage เป็นการจองพื้นที่ ไม่ได้ mount/เขียน filesystem จริง

Serial ใช้ UART console 115200 ผ่านพอร์ต USB-to-UART ของบอร์ด ไม่ตั้ง camera/UART sensor pins ตาม console โดยอัตโนมัติ หากใช้พอร์ต native USB ต้องตกลง console configuration ใหม่

## ESP-WHO: framework support ไม่เท่ากับ board integration ผ่าน

จุดอ้างอิงที่ตรวจ: ESP-WHO commit [`1abda05e1c0782237fcb9e8d33a2fa7e105f06b2`](https://github.com/espressif/esp-who/tree/1abda05e1c0782237fcb9e8d33a2fa7e105f06b2) ใช้เป็น snapshot อ้างอิงสำหรับการวางแผน **ไม่ใช่ dependency ที่ผ่าน integration test แล้ว**

[README ที่ตรวจ](https://github.com/espressif/esp-who/blob/1abda05e1c0782237fcb9e8d33a2fa7e105f06b2/README.md) ระบุรองรับสาย ESP-IDF 5.4/5.5/6.0/6.1 และบอร์ดตัวอย่าง เช่น S3-EYE/Korvo-2 ไม่ได้ให้ preset DevKitC-1-N8R8 + ผัง OV2640 ของกลุ่ม

[human_face_recognition CMakeLists](https://github.com/espressif/esp-who/blob/1abda05e1c0782237fcb9e8d33a2fa7e105f06b2/examples/human_face_recognition/CMakeLists.txt) ใช้หลาย local components, ตัวแปร BSP และ IDF extra actions ตาม [ขั้นตอน ESP-WHO](https://github.com/espressif/esp-who/blob/1abda05e1c0782237fcb9e8d33a2fa7e105f06b2/README.md) จึงไม่ใช่การใส่ชื่อ library ใน PlatformIO แล้วสมมติว่าจะใช้ได้ทันที

ข้อสรุป: ESP-IDF 5.5.0 อยู่ในสายที่ upstream ระบุรองรับ แต่ **ยังไม่ยืนยันว่า ESP-WHO snapshot นี้ build/run กับ PlatformIO + บอร์ด + กล้องของกลุ่มได้** ไม่ได้สรุปว่า PlatformIO ทำไม่ได้ เราไม่เปลี่ยนเครื่องมือทั้งโครงการและไม่อ้าง Face Recognition สำเร็จ ขณะนี้ยังไม่มีโค้ด Face Recognition และไม่มี AI จำลอง

งานถัดไปหลังได้ pin map: ทำ BSP กล้อง/PSRAM ที่ถูกต้อง, ตรวจ ESP-WHO component lock และ partitions, ทดสอบ capture จากกล้อง, ทดสอบ model/registration/threshold บนบอร์ดจริง แล้วจึงผูกผล Granted/Denied เข้ากับ controller และ interlocks หากจำเป็นต้องใช้ native idf.py สำหรับ PoC จะอธิบายเหตุผลและแยกโฟลเดอร์โดยไม่แทน PlatformIO Gateway โดยเงียบ ๆ

## Network และเครื่องมือทดสอบ

ใช้ [ESP-MQTT ของ IDF 5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/protocols/mqtt.html): TLS ต้องตรวจ CA/hostname; QoS 0 ไม่ retransmit แบบ QoS 1/2 และใช้ clean session ไม่มีคำสั่งมอเตอร์ replay

ทดสอบ host logic ด้วย Zig 0.15.2 (compiler ชั่วคราวจาก PyPI ใน tmp; ไม่ใช่ firmware dependency) รันไฟล์ C++ เดียวกับ DoorController/CommandGuard จริง ไม่ใช่แบบจำลอง JavaScript ของ firmware จัดรูปแบบด้วย clang-format 20.1.8 ชั่วคราว ไม่จำเป็นต้องติดตั้งเพื่อ build PlatformIO

PlatformIO native ไม่ติดตั้ง GCC ให้เองตาม [เอกสาร Native](https://docs.platformio.org/en/latest/platforms/native.html) จึงไม่ได้สมมติว่า Windows ใหม่มี compiler สำหรับ host tests อยู่แล้ว README มีวิธีใช้ Zig หรือ compiler C++ ที่มีอยู่ โดยไม่เปลี่ยนระบบ PATH ถาวร
