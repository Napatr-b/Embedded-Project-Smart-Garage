# ทดสอบกล้อง OV2640 กับ Gateway

เฟิร์มแวร์ทดสอบนี้ใช้ไดรเวอร์ [esp32-camera](https://github.com/espressif/esp32-camera) ที่มากับ Arduino ESP32 เพื่อถ่ายภาพ JPEG 320×240 จากกล้อง DVP ผ่าน USB Serial บน YD ESP32-S3 N16R8 ภาพจะถูกบันทึกลงเครื่องที่รันสคริปต์ ส่วน Gateway ESP-IDF และ Face Recognition เป็นขั้นตอนรวมระบบภายหลัง

1. ตรวจชื่อรุ่นกล้องและบอร์ดแปลง เทียบผังขา แรงดันเลี้ยง และด้านหน้าสัมผัสสายแพกับคู่มือของรุ่นจริงก่อนจ่ายไฟ
2. คัดลอก [include/config.example.h](include/config.example.h) เป็น `include/config.local.h` แล้วใส่ **หมายเลข GPIO ของ Gateway** สำหรับแต่ละสัญญาณ ถ้าเอกสารใช้ชื่อ Y2–Y9 ให้ตรวจจากผังของโมดูลว่าสัญญาณตรงกับ D0–D7 อย่างไร ให้ใช้ `-1` สำหรับ PWDN หรือ RESET เมื่อบอร์ดแปลงจัดการสัญญาณนั้นเองเท่านั้น เฟิร์มแวร์จะปฏิเสธขาที่ขาด ขาซ้ำ และขาที่สงวนไว้
3. จากราก repo ให้ใช้พอร์ต USB-to-UART ของ Gateway และแทน `COM5` ด้วยพอร์ตจริง การอัปโหลดจะแทนเฟิร์มแวร์ Gateway ที่อยู่บนบอร์ด จึงควรถอด Servo ระหว่างทดสอบ

   ```powershell
   pio run -d camera-test
   pio run -d camera-test -t upload --upload-port COM5
   py -m pip install pyserial
   py tools/camera_capture.py COM5 tmp/camera.jpg
   ```

4. สร้างโฟลเดอร์ `tmp` ก่อนรันสคริปต์หากยังไม่มี และปิด Serial Monitor ก่อนเปิดพอร์ตจาก Python เปิด `tmp/camera.jpg` เพื่อตรวจว่าภาพครบและคมชัด เมื่อทดสอบจบ ให้อัปโหลด Gateway environment ที่ใช้งานอยู่กลับเข้าไป
