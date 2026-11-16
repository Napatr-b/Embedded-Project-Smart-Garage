import { thaiTime } from './Common.jsx';
export default function EventLog({ events, source }) {
  return (
    <section className="panel event-panel" id="events">
      <div className="section-head">
        <div>
          <span className="eyebrow">ACTIVITY LOG</span>
          <h2>ประวัติเหตุการณ์</h2>
        </div>
        <span className="badge neutral">{events.length} รายการ</span>
      </div>
      <p className="subtle">{source} • เก็บล่าสุด 100 รายการต่อรอบ</p>
      <div className="table-wrap">
        <table>
          <thead>
            <tr>
              <th>เวลา (กรุงเทพฯ)</th>
              <th>ประเภท</th>
              <th>รายละเอียด</th>
            </tr>
          </thead>
          <tbody>
            {events.map((e) => (
              <tr key={e.id}>
                <td className="nowrap">{thaiTime(e.timestamp)}</td>
                <td>
                  <span className="event-type">{e.type}</span>
                </td>
                <td>{e.message}</td>
              </tr>
            ))}
          </tbody>
        </table>
        {!events.length && <p className="empty">ยังไม่มีเหตุการณ์</p>}
      </div>
    </section>
  );
}
