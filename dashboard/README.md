# Web Dashboard

รัน `npm.cmd ci` แล้วตั้งค่า Backend ตาม [คู่มือช่องจอดรถ](../docs/parking-mvp.md) จากนั้นรัน `npm.cmd run dev` หน้าแรกแสดงสถานะช่องจอดรถ ลิงก์แดชบอร์ด MQTT เปิดหน้าเดิมที่มี Demo Mode

ทดสอบ `npm.cmd test` และ `npm.cmd run build` โหมด MQTT ใช้ WSS ตั้งค่าจาก `.env.example` ห้ามใส่ secret ใน VITE_* เพราะ browser อ่านได้

คู่มือหลัก: [README ราก](../README.md) / [ร่าง integration](../docs/integration-contract.md) / [บทเดโม](../docs/demo-script.md)

`src/services/contract.generated.js` ถูกสร้างจาก docs/contract.json ที่ราก ถ้าทีมแก้สัญญาให้รัน `node tools/sync-contract.mjs` จากรากแล้ว commit generated files ทั้งสองฝั่ง เว็บ build แยกในโฟลเดอร์นี้ได้โดยไม่ต้องอ่านไฟล์ภายนอก Root Directory ของ Vercel
