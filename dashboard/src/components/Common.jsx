export const thaiTime = (value, short = false) =>
  value
    ? new Intl.DateTimeFormat('th-TH', {
        timeZone: 'Asia/Bangkok',
        ...(short ? {} : { day: '2-digit', month: 'short', year: 'numeric' }),
        hour: '2-digit',
        minute: '2-digit',
        second: '2-digit',
        hour12: false,
      }).format(new Date(value))
    : 'ยังไม่ได้รับข้อมูล';
export const value = (v, digits = 0) =>
  v == null
    ? 'ไม่มีข้อมูล'
    : Number(v).toLocaleString('th-TH', { maximumFractionDigits: digits });
export function Badge({ tone = 'neutral', children }) {
  return (
    <span className={`badge ${tone}`}>
      <span className="dot" />
      {children}
    </span>
  );
}
export function Metric({ label, number, unit, note, icon }) {
  return (
    <article className="metric">
      <div className="metric-top">
        <span>{label}</span>
        <span className="metric-icon" aria-hidden="true">
          {icon}
        </span>
      </div>
      <div className="metric-value">
        {number}
        <small>{number === 'ไม่มีข้อมูล' ? '' : unit}</small>
      </div>
      <p>{note}</p>
    </article>
  );
}
