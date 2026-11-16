import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    // เอกสารและไฟล์ชั่วคราวไม่ใช่ source ของหน้าเว็บ ลด file lock บน Windows
    watch: { ignored: ['**/docs/**', '**/tmp/**'] },
  },
});
