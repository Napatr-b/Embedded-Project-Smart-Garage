// Interface: adapter.load() -> Promise<{events, samples, source}>

export const mockHistoryAdapter = {
  async load() {
    const now = Date.now();
    return {
      source: 'ข้อมูลตัวอย่างในหน่วยความจำ',
      events: [
        {
          id: 's1',
          timestamp: new Date(now - 120000).toISOString(),
          type: 'ACCESS',
          message: 'ตัวอย่าง: ยืนยันสิทธิ์บนบอร์ดสำเร็จ',
        },
        {
          id: 's2',
          timestamp: new Date(now - 60000).toISOString(),
          type: 'DOOR',
          message: 'ตัวอย่าง: ประตูปิดแล้ว',
        },
      ],
      samples: Array.from({ length: 20 }, (_, i) => ({
        timestamp: new Date(now - (20 - i) * 5000).toISOString(),
        temperature: 29 + Math.sin(i / 4) * 0.6,
        humidity: 63 + Math.cos(i / 5) * 2,
      })),
    };
  },
};
export const unconfiguredHistoryAdapter = {
  async load() {
    return {
      source: 'แสดงข้อมูล MQTT ในรอบนี้',
      events: [],
      samples: [],
    };
  },
};
