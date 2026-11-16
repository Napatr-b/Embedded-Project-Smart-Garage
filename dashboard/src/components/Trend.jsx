import { thaiTime } from './Common.jsx';
export default function Trend({ samples }) {
  const valid = samples.filter(
    (p) => p.temperature != null || p.humidity != null,
  );
  const temps = samples.map((p) => p.temperature).filter((v) => v != null),
    hums = samples.map((p) => p.humidity).filter((v) => v != null);
  const tMin = Math.min(20, ...temps),
    tMax = Math.max(40, ...temps),
    hMin = Math.min(40, ...hums),
    hMax = Math.max(90, ...hums);
  const x = (i) => 48 + (i / Math.max(samples.length - 1, 1)) * 640;
  function line(key, min, max) {
    return samples
      .map((p, i) =>
        p[key] == null
          ? null
          : `${x(i)},${150 - ((p[key] - min) / (max - min)) * 115}`,
      )
      .reduce((a, p) => {
        if (p) {
          if (!a.length || a[a.length - 1] === null) a.push('M' + p);
          else a.push('L' + p);
        } else a.push(null);
        return a;
      }, [])
      .filter(Boolean)
      .join(' ');
  }
  return (
    <section className="panel trend">
      <div className="section-head">
        <div>
          <span className="eyebrow">ENVIRONMENT TREND</span>
          <h2>สภาพแวดล้อมช่วงล่าสุด</h2>
        </div>
        <span className="subtle">{samples.length} จุดล่าสุด</span>
      </div>
      <div className="legend">
        <span>
          <i className="temp" />
          อุณหภูมิ °C (แกนซ้าย)
        </span>
        <span>
          <i className="humid" />
          ความชื้น % (แกนขวา)
        </span>
      </div>
      {valid.length ? (
        <>
          <svg
            viewBox="0 0 740 190"
            role="img"
            aria-label="กราฟอุณหภูมิสีเขียวและความชื้นสีม่วง ใช้แกนแยกตามหน่วยที่ระบุ"
          >
            <text x="5" y="22">
              °C
            </text>
            <text x="709" y="22">
              %
            </text>
            {[0, 1, 2, 3, 4].map((i) => (
              <g key={i}>
                <line
                  x1="48"
                  x2="688"
                  y1={35 + i * 28.75}
                  y2={35 + i * 28.75}
                  stroke="#e7ede9"
                />
                <text x="10" y={39 + i * 28.75}>
                  {(tMax - (i * (tMax - tMin)) / 4).toFixed(0)}
                </text>
                <text x="700" y={39 + i * 28.75}>
                  {(hMax - (i * (hMax - hMin)) / 4).toFixed(0)}
                </text>
              </g>
            ))}
            <path d={line('temperature', tMin, tMax)} className="temp-line" />
            <path d={line('humidity', hMin, hMax)} className="humid-line" />
          </svg>
          <div className="chart-times">
            <span>{thaiTime(samples[0]?.timestamp, true)}</span>
            <span>{thaiTime(samples.at(-1)?.timestamp, true)}</span>
          </div>
          <details>
            <summary>ดูค่ากราฟเป็นตาราง</summary>
            <div className="table-wrap">
              <table>
                <thead>
                  <tr>
                    <th>เวลา</th>
                    <th>°C</th>
                    <th>%RH</th>
                  </tr>
                </thead>
                <tbody>
                  {samples.map((p, i) => (
                    <tr key={i}>
                      <td>{thaiTime(p.timestamp, true)}</td>
                      <td>{p.temperature?.toFixed(1) ?? 'ไม่มีข้อมูล'}</td>
                      <td>{p.humidity?.toFixed(1) ?? 'ไม่มีข้อมูล'}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </details>
        </>
      ) : (
        <p className="empty">ยังไม่มีข้อมูลสำหรับสร้างกราฟ</p>
      )}
    </section>
  );
}
