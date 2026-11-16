> เอกสารเก็บงานเดิมก่อนจัดโครงสร้าง ใช้อ้างอิงย้อนหลังเท่านั้น โปรดใช้ README.md ที่รากโครงการและ integration-contract.md ฉบับปัจจุบัน

# Smart Garage

แดชบอร์ดภาษาไทยสำหรับงานกลุ่มวิชา Embedded System สร้างด้วย React + Vite + JavaScript และ CSS ปกติ ใช้ npm รันในเครื่องได้ ไม่ต้องมีฐานข้อมูลหรือบัญชีภายนอกสำหรับเดโม

อ่านข้อกำหนดจาก `docs/Smart_Garage_Proposal_TH.pdf` ครบ 10 หน้าแล้ว เก็บไฟล์ต้นฉบับไว้ ไม่เปลี่ยน proposal เป็นคำสั่งของโปรแกรม

## เริ่มใช้งานบน Windows + VS Code

1. เปิด VS Code → File → Open Folder → เลือก `C:\Users\tbbbo\Desktop\smart-garage`
2. เลือก Terminal → New Terminal (PowerShell)
3. รันทีละบรรทัด:

```powershell
cd C:\Users\tbbbo\Desktop\smart-garage
node --version
npm.cmd --version
npm.cmd install
npm.cmd run dev
```

4. เปิด browser ที่ `http://127.0.0.1:5173/` หากพอร์ตไม่ว่าง Vite จะพิมพ์ URL พอร์ตถัดไป ให้ใช้ URL ที่แสดงใน Terminal
5. เริ่มที่ Demo Mode เห็นป้าย **ข้อมูลจำลอง** ตลอดการใช้งาน เลื่อนหน้าจอเพื่อดูกราฟ เครื่องมือจำลอง และ Event Log
6. หยุด server: คลิก Terminal แล้วกด **Ctrl+C** ถ้าถาม Terminate batch job ให้ตอบ Y แล้ว Enter

ใช้ `npm.cmd` เพื่อหลีกเลี่ยงปัญหา PowerShell ไม่อนุญาตรัน npm.ps1 ไม่ต้องเปลี่ยน Execution Policy หากขึ้นว่า node ไม่พบ ให้ติดตั้ง Node.js รุ่น LTS จากเว็บไซต์ทางการแล้วปิดเปิด VS Code ใหม่ (Vite รุ่นนี้ต้อง Node 20.19+ หรือ 22.12+; แนะนำ LTS ที่ใหม่กว่า) เครื่องที่สร้างโครงการนี้ใช้ Node 26.13.1

## สิ่งที่ทำได้

- Demo เริ่มต้น: ค่าจำลองเปลี่ยนทุก 3 วินาที ประตูเปิด/ปิดใช้เวลาประมาณ 2.8 วินาที ปุ่มหยุดเด่นชัด หยุดแล้วแสดง Fault ต้องรีเซ็ต Fault ก่อนเคลื่อนต่อ
- จำลอง Gateway offline, พัก telemetry/heartbeat เพื่อให้เก่าหลัง 15 วินาที, fault และรถเข้า/ออก พร้อมประวัติตัวอย่าง ปุ่มรีเซ็ตล้างรอบเดโมและสร้างตัวอย่างใหม่
- แยก Broker / Gateway / เวลารับ telemetry ล่าสุด หากข้อมูลเก่าค่าที่เห็นเป็นค่าครั้งก่อน ไม่ใช่ค่าปัจจุบัน
- ประตู Closed, Opening, Open, Closing, Fault และ Unknown เมื่อไม่มีข้อมูล
- Ultrasonic, DHT22, LDR raw (ไม่ใช่ lux), LED และ ENS160: AQI / TVOC / eCO2 ข้อมูลที่ขาดแสดง “ไม่มีข้อมูล” eCO2 ไม่ใช่ CO2 ที่วัดโดยตรง
- กราฟล่าสุด 40 จุด มีตารางอ่านค่าทางเลือก และ Event Log ล่าสุด 100 รายการในหน่วยความจำ เวลาไทย Asia/Bangkok
- ไม่มีการปิดประตูอัตโนมัติ พื้นที่จอดว่างไม่รับรองความปลอดภัยของทางประตู Stop บนเว็บไม่แทนปุ่มฮาร์ดแวร์

## MQTT Mode (ยังเป็นโครง integration)

โหมดนี้ยังไม่พร้อมเปิดควบคุมผ่านอินเทอร์เน็ต ต้องตกลง authentication, ACL และสัญญาข้อมูลกับทีมก่อน ไม่มี config จะขึ้น “ยังไม่ตั้งค่า” และไม่เชื่อมต่อปลอม

1. อ่าน `docs/integration-contract.md` แล้วตกลงกับเพื่อน Gateway ก่อน
2. คัดลอกตัวอย่าง:

```powershell
Copy-Item .env.example .env
```

3. กรอก URL `wss://` และ topic 5 ช่องใน `.env` ตามค่าที่ทีมยืนยัน (ทุก topic ต้องต่างกัน ไม่ใช้ + หรือ #) ห้ามใส่ username/password ใน URL
4. หยุดแล้วรัน `npm.cmd run dev` ใหม่ เลือก MQTT Mode

Browser ใช้ MQTT over Secure WebSocket เท่านั้น ใช้ mqtt.js reconnect ทุก 3 วินาที subscribe ใหม่หลัง reconnect ใช้ clean session และยุติ connection เมื่อสลับโหมด/ออกจากหน้า ไม่มีการเข้าคิว publish ขณะ offline คำสั่งใช้ QoS 0 และ **retain: false** ไม่มี motor retry อัตโนมัติ

ต้องได้รับ heartbeat และ telemetry ที่อายุไม่เกิน 15 วินาทีจึงส่งคำสั่งได้ ปุ่มเปิด/ปิดถูกปิดเมื่อ Unknown, Fault, กำลังเคลื่อน หรือรอคำสั่ง Stop ส่งแทน pending เดิมได้เมื่อ connection ยังสด

ส่งแล้วจะแสดง pending พร้อม commandId การ publish สำเร็จไม่เปลี่ยนสถานะประตู ต้องรอ telemetry หรือ acknowledgement จาก Gateway; accepted หมายถึงรับคำสั่งเท่านั้น completed หมายถึง Gateway ยืนยันการดำเนินคำสั่ง แต่ภาพ/สถานะประตูบนเว็บยังยึด telemetry จริง ถ้าครบ 8 วินาทีไม่มีคำยืนยัน จะแจ้งไม่ทราบผลและไม่ส่งซ้ำ ต้องตรวจสถานะก่อนสั่งใหม่

ตรวจ JSON, timestamp พร้อมเขตเวลา, ชนิดและช่วงค่าพื้นฐาน payload ผิดจะบันทึก ERROR และละเว้น packet ไม่ทำให้หน้าเว็บพัง field เซนเซอร์ที่ขาดไม่ใช้ค่าค้างมาทดแทน packet เก่า/ย้อนหลังเกิน 15 วินาทีไม่ใช้เป็นสถานะสด ต้องใช้ endpoint ประวัติแยกสำหรับข้อมูลย้อนหลัง Gateway ต้องซิงก์เวลา NTP

**VITE_* ทุกค่าถูกฝังใน JavaScript และส่งไป browser จึงไม่ใช่ที่เก็บ secret** `.env` ไม่ได้ทำให้ secret ปลอดภัยถ้าใช้ชื่อ VITE_ ห้ามใส่ LINE token, service-account key หรือรหัส MQTT ถาวรใน frontend ปัจจุบันยังไม่ใส่ระบบ login หรือ credential ให้เจรจา backend ที่ออกสิทธิ์อายุสั้นหรือรูปแบบที่ทีมยอมรับก่อนใช้งานจริง

## โครงสร้างไฟล์

```text
src/
  App.jsx                  หน้า dashboard และปุ่มควบคุม
  useGarage.js             วงจร service, freshness และ pending
  styles.css               สี layout และ responsive
  components/              การ์ด กราฟ และ Event Log
  services/
    demo.js                อุปกรณ์จำลองและ timer cleanup
    mqtt.js                config / connect / subscribe / publish
    protocol.js            payload validation และ command timeout
    history.js             interface ประวัติและ mock adapter
 tests/garage.test.js       ทดสอบ logic ด้วย node:test
 docs/                     proposal, ร่าง integration, บทสาธิต
```

ประวัติเดโมเป็น mock ในหน่วยความจำ ไม่ใช่ประวัติจริงจาก Google Sheets รีเฟรช/สลับโหมดจะเริ่มใหม่ MQTT แสดงเฉพาะเหตุการณ์ที่รับในรอบปัจจุบัน ยังไม่สร้าง HTTPS adapter จริง ไม่มี LINE sender, AI หรือกล้องจริงบนเว็บ Face Recognition ใช้ ESP-WHO บนบอร์ด วิธีแสดงภาพกล้องรอทีมกำหนด

## ทดสอบและตรวจหน้าจอ

```powershell
npm.cmd test
npm.cmd run build
npm.cmd run preview
```

Preview เปิด `http://127.0.0.1:4173/` (หรือพอร์ตที่ Terminal แจ้ง) กด Ctrl+C เพื่อหยุด `dist/` คือผล build ไม่ต้องแก้ไฟล์ในนั้น

ชุดทดสอบครอบคลุม malformed payload, field ขาด, freshness, acknowledgement ที่ commandId ผิด, timeout, Stop แทน pending, config และสถานะประตูเดโม การ build ผ่านไม่ยืนยันการเชื่อมอุปกรณ์จริง

ตรวจหน้าจอด้วย Chrome: เปิดเดโม → กด F12 → Ctrl+Shift+M → เลือก Responsive 390 × 844 แล้วทดสอบ เปิด/ปิด/หยุด, Fault, offline และพักข้อมูล 16 วินาที จากนั้นปิด Device Toolbar ตรวจบนจอคอม ใช้ Tab/Shift+Tab และ Enter/Space เพื่อทดสอบปุ่ม เลือก “ดูค่ากราฟเป็นตาราง” เพื่ออ่านข้อมูลโดยไม่พึ่งสี

การตรวจผ่านเครื่องมืออัตโนมัติในรอบสร้างนี้: build และ unit tests ดู `docs/verification.md` ส่วน browser QA ถูกระบบ Computer Use ยุติเนื่องจากระบุ URL Chrome ไม่ได้ จึงยังไม่ยืนยันภาพจอคอม/มือถือ ไม่มีการทดสอบ MQTT broker จริง, ESP32, Sheets, LINE หรือ Vercel deployment

## นำขึ้น Vercel ด้วยตัวเอง (ยังไม่ได้ deploy)

1. รัน test และ build ให้ผ่านก่อน ตรวจว่า `.env` ไม่อยู่ใน Git และไม่มี secret ใน source
2. สร้าง Git repository ของตัวเอง แล้ว push โค้ดขึ้น GitHub ผ่าน VS Code Source Control อย่าเพิ่ม `node_modules` หรือ `.env`
3. ใน Vercel เลือก Add New → Project → Import repository ของคุณ แล้วเลือก framework Vite
4. ตั้ง Root Directory เป็นรากโครงการ, Install Command `npm ci`, Build Command `npm run build`, Output Directory `dist` ใช้ Node รุ่นที่ Vercel รองรับและตรงตามข้อกำหนด Vite
5. สำหรับเดโม **ไม่ต้องตั้ง Environment Variables** ให้กด Deploy ด้วยตัวเองแล้วเปิด URL ที่ Vercel ให้ เว็บจะเริ่ม Demo Mode
6. ถ้าจะทดสอบ MQTT ให้กรอกเฉพาะค่าที่เผยแพร่ได้จาก `.env.example` ใน Environment Variables แล้ว redeploy ค่าของ VITE_* ถูกอ่านตอน build ไม่ใช่ runtime
7. หลัง deploy ทดสอบมือถือ คีย์บอร์ด และปุ่มเดโมอีกครั้ง ห้ามเปิดคำสั่งประตูจริงบนอินเทอร์เน็ตก่อนทบทวน authentication, topic ACL, การกัน replay และ interlock ฝั่งอุปกรณ์กับทีม

เว็บเป็น static Vite app หน้าเดียว ไม่ต้องมีฐานข้อมูล ไม่ต้องสร้าง backend เพื่อเดโม บริการ hosting/broker อาจมีโควตาหรือเงื่อนไข ให้ตรวจแผนบริการก่อนสมัครใช้งานจริง
