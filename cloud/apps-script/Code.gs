/**
 * Smart Garage: Google Sheets logger (Apps Script web app).
 *
 * Setup: create the Sheet -> Extensions > Apps Script -> paste this file.
 * Project Settings > Script properties: add SHARED_KEY.
 * Deploy > New deployment > Web app (Execute as: Me, Who has access: Anyone).
 * Put the /exec URL + key in gateway/include/secrets.h. Never commit them.
 *
 * POST body (JSON):
 *   { "key": "...", "kind": "reading" | "event",
 *     "rows": [ { "ts": "...", "node_id": 1, "seq": 42, "data": {...} }, ... ] }
 * "rows" is an array so the gateway can replay a buffered backlog in one request (E4).
 */
function doPost(e) {
  const key = PropertiesService.getScriptProperties().getProperty('SHARED_KEY');
  const body = JSON.parse(e.postData.contents);
  if (!key || body.key !== key) return json_({ ok: false, error: 'unauthorized' });

  const sheetName = body.kind === 'event' ? 'events' : 'readings';
  const ss = SpreadsheetApp.getActiveSpreadsheet();
  const sheet = ss.getSheetByName(sheetName) || ss.insertSheet(sheetName);

  const rows = (body.rows || []).map(r => [
    new Date(), r.ts || '', r.node_id ?? '', r.seq ?? '', JSON.stringify(r.data ?? {})
  ]);
  if (rows.length) {
    sheet.getRange(sheet.getLastRow() + 1, 1, rows.length, rows[0].length).setValues(rows);
  }
  return json_({ ok: true, count: rows.length });
}

function json_(obj) {
  return ContentService.createTextOutput(JSON.stringify(obj))
    .setMimeType(ContentService.MimeType.JSON);
}
