# Web Dashboard

เปิด Terminal ที่โฟลเดอร์นี้แล้วรัน `npm.cmd install` และ `npm.cmd run dev` เปิด URL ที่ Vite แสดง หยุดด้วย Ctrl+C เริ่มต้น Demo Mode โดยไม่ต้องมีบัญชี/บอร์ด

ทดสอบ `npm.cmd test` และ `npm.cmd run build` โหมด MQTT ใช้ WSS ตั้งค่าจาก `.env.example` ห้ามใส่ secret ใน VITE_* เพราะ browser อ่านได้

คู่มือหลัก: [README ราก](../README.md) / [ร่าง integration](../docs/integration-contract.md) / [บทเดโม](../docs/demo-script.md)

`src/services/contract.generated.js` ถูกสร้างจาก docs/contract.json ที่ราก ถ้าทีมแก้สัญญาให้รัน `node tools/sync-contract.mjs` จากรากแล้ว commit generated files ทั้งสองฝั่ง เว็บ build แยกในโฟลเดอร์นี้ได้โดยไม่ต้องอ่านไฟล์ภายนอก Root Directory ของ Vercel
