# Local MQTT board test — 2026-11-16

## Latest result: connectivity passed at 14:48 Asia/Bangkok

Updated the existing local Wi-Fi settings using the user's latest value, preserving
spaces in the password (not reproduced here). PC LAN changed to `10.72.140.80/24`;
updated existing broker host/subnet and firmware MQTT URI, restarted the existing
broker, reused Vite and web config. Build/upload on the same COM3 N16R8 succeeded.

Found a second runtime issue: ESP-MQTT discards `enqueue(qos=0, store=false)`.
Changed gateway outbound messages to direct QoS 0 publish guarded by the current
MQTT connection, with no offline outbox. Rebuilt/uploaded and verified:

- Serial: Wi-Fi online, MQTT online, time synchronized, no panic observed.
- WebSocket client using the web payload parser received two fresh board telemetry
  timestamps about three seconds apart, `simulatedHardware=true`, `actuatorEnabled=false`.
- Chrome MQTT Mode displayed Broker connected, Gateway `garage-01` online, fresh
  telemetry time 14:48:49 and the explicit simulated sensor/position notice.
- Physical actuator remains disabled; no physical motion was commanded.

Evidence: ignored `local-test/live-telemetry.json` and `local-test/serial-test.log`.
Current web URL remains http://127.0.0.1:5173/.
Open/Close/Stop and deliberate offline/reconnect tests below remain unverified;
this follow-up verified connectivity only. The initial blocker below is historical.

Target: `gateway_local_n16r8`, ESP32-S3 N16R8, user-supplied carrier marking
`YD-ESP32-23 2022-V1.3`, COM3 matched to the USB identity in the backup record.
Do not use the older N8R8 build instructions for this board.

## Verified

- Existing flash backup: 16,777,216 bytes; SHA-256 matches its existing metadata;
  metadata records block digest verification. Backup was preserved before upload.
- Build and upload succeeded; esptool verified uploaded image hashes.
- Runtime reports flash 16,777,216 bytes and PSRAM 8,388,608 bytes, PSRAM memory test OK.
- Physical actuator disabled, GPIO unassigned, no actuator driver. Sensor readings
  and door position in this environment are simulated on the physical gateway.
- Existing broker and Vite server reused. Board uses PC LAN address on TCP 1883;
  browser uses `ws://127.0.0.1:8083/mqtt`. Web URL: http://127.0.0.1:5173/.
- Config comparison passed: device ID, board URI, WebSocket URL, five topics,
  board credentials and web credentials. No secret values printed.
- Host broker check passed anonymous rejection, authentication, subscription ACL,
  and non-retained WebSocket-to-TCP transport. These are host tests, not ESP32 tests.
- Web unit tests: 22/22 passed. These do not establish board connectivity.

## Fix and current blocker

Initial boot repeatedly panicked during Wi-Fi initialization with heap allocation
backtrace. Increased main task stack from 3584 to 8192 bytes in the N16R8 defaults
and generated local sdkconfig, rebuilt and uploaded. Subsequent monitored execution
remained running without that panic. This is an observed fix, not a long-duration soak test.

Added Wi-Fi disconnect reason logging without SSID/password output. Board repeatedly
reports reason 201 (`WIFI_REASON_NO_AP_FOUND`, installed ESP-IDF header), Wi-Fi offline,
MQTT offline, clock unsynchronized. Windows AP scan was unavailable due to Location
permission; it did not prove that an SSID was absent.

User reported the original hotspot enabled. Still requires matching the configured
SSID locally and ensuring 2.4 GHz / hotspot compatibility, then retrying. Do not
change the broker or create credentials to work around AP discovery.

## Not yet passed

- Gateway online, live board telemetry and updating timestamps in web UI.
- Open/Close/Stop command IDs, acknowledgements and final states from board.
- Remain open, close motion, intermediate stop, resume in either direction.
- Actual offline/stale/reconnect and absence of old-command replay.

Web accessibility showed MQTT Mode / broker connected / Gateway unknown / no
telemetry. Browser automation subsequently failed with geometry unavailable and
app approval timeout; no successful browser button test is claimed.

## Retry using existing configuration

`node tools/local-broker/check-config.mjs` compares settings without printing secrets.
`node tools/local-broker/check.mjs` checks host broker transport.
`tools/monitor-local.py` captures a bounded serial observation on COM3.
`node tools/local-broker/board-test.mjs` uses the existing web credentials and endpoint
to send commands to the physical board's simulated state machine, validates
acknowledgements and positions, and saves messages in `local-test/board-test.json`.
Only run it with physical actuator disabled. A timeout is a failed test.
The script does not replace UI or reconnect testing.

Local backup, serial log and failed board test output remain under ignored
`local-test/`; do not publish that directory or local credential files.
