# Cloud

- `apps-script/Code.gs`: Google Sheets logger (cloud storage for M5). Setup steps are in the file header.
- MQTT broker: TBD. It must support TLS (E5) and WebSocket over TLS (WSS) for the browser dashboard.
- LINE Messaging API alerts (M6): TBD. Decide which events trigger an alert (see docs/protocol.md).

Credentials go in `gateway/include/secrets.h` / dashboard env vars only. Never commit them.
