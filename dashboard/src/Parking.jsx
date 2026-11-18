import { useEffect, useState } from 'react';
import './parking.css';

const labels = { OCCUPIED: 'มีรถจอด', VACANT: 'ว่าง', UNKNOWN: 'ยังไม่มีข้อมูล' };
const thaiTime = new Intl.DateTimeFormat('th-TH', {
  timeZone: 'Asia/Bangkok', dateStyle: 'long', timeStyle: 'medium',
});

export default function Parking() {
  const [data, setData] = useState(null);
  const [failed, setFailed] = useState(false);

  useEffect(() => {
    const lifetime = new AbortController();
    let loading = false;
    async function refresh() {
      if (loading) return;
      loading = true;
      try {
        const response = await fetch('/api/parking', {
          cache: 'no-store', signal: AbortSignal.any([lifetime.signal, AbortSignal.timeout(4500)]),
        });
        if (!response.ok) throw new Error('Parking API unavailable');
        const result = await response.json();
        if (!lifetime.signal.aborted) {
          setData(result);
          setFailed(false);
        }
      } catch {
        if (!lifetime.signal.aborted) setFailed(true);
      } finally {
        loading = false;
      }
    }
    refresh();
    const timer = setInterval(refresh, 5000);
    return () => {
      clearInterval(timer);
      lifetime.abort();
    };
  }, []);

  return (
    <main className="parking-page">
      <header><strong>Smart Garage</strong><a href="?view=garage">แดชบอร์ด MQTT</a></header>
      <section className="parking-card" aria-live="polite">
        <h1>ช่องจอดรถ 1</h1>
        <p className="parking-state" data-state={data?.state}>
          {data ? labels[data.state] : failed ? 'ยังไม่มีข้อมูล' : 'กำลังโหลด'}
        </p>
        {data?.occurred_at && (
          <p>เหตุการณ์ล่าสุด <time dateTime={data.occurred_at}>{thaiTime.format(new Date(data.occurred_at))}</time> (เวลาไทย)</p>
        )}
        {failed && <p className="parking-error" role="alert">
          โหลดข้อมูลไม่สำเร็จ {data && 'กำลังแสดงข้อมูลจากการโหลดครั้งล่าสุด'}
        </p>}
      </section>
    </main>
  );
}
