# Protocols

## Node -> Gateway (ESP-NOW by default, UART optional)

Defined in `shared/protocol/garage_protocol.h`. The same frame travels over either transport:

| Transport | Switch | Notes |
|---|---|---|
| **ESP-NOW** (default since 18 Nov 2026) | node `LINK_USE_ESPNOW 1`, gateway default (`GARAGE_LINK_ESPNOW=1`) | One frame per ESP-NOW packet. The node broadcasts until the first ACK, then unicasts to the gateway MAC. With no router the gateway sits on channel `GARAGE_ESPNOW_CHANNEL` (1). With a router it follows the router's channel, and the node hops channels after `ESPNOW_MISSES_BEFORE_HOP` unanswered messages. The gateway keeps Wi-Fi power save off so it doesn't miss packets. Not encrypted yet (TODO E5: PMK/LMK) |
| UART 115200 8N1 | node `LINK_USE_ESPNOW 0`, gateway `-DGARAGE_LINK_ESPNOW=0` | Original wired design. Switched away from it after breadboard wiring kept failing (see [uart-link-checks.md](uart-link-checks.md)) |

Verified on the bench, 18 Nov 2026: node (DOIT) + gateway (S3 N16R8), `gateway_bringup`, ESP-NOW channel 1. 22/22 test frames and 21/21 ultrasonic frames were ACKed on the first try, `delivery=100.0%`, `lost=0`. This is a short run, not yet the repeated metric runs for the report.

| Byte | Field |
|---|---|
| 0 | SOF `0xA5` |
| 1 | Version (1) |
| 2 | Type: `0x01` READINGS, `0x02` ULTRASONIC, `0x80` ACK, `0x81` NACK |
| 3 | Node ID |
| 4-5 | Sequence number (LE) |
| 6 | Payload length |
| 7.. | Payload |
| last 2 | CRC16-CCITT over bytes 1..end of payload (LE) |

### Payloads (little-endian, packed)

| Type | Payload | Bytes |
|---|---|---|
| READINGS `0x01` | `int16 temp_c_x100, uint16 humidity_x100, uint16 ldr_raw, uint16 eco2_ppm, uint16 tvoc_ppb, uint8 aqi, uint8 led_on` | 12 |
| ULTRASONIC `0x02` | `uint16 distance_mm, uint8 status` (0 = OK, 1 = no echo) | 3 |
| ACK `0x80` | none (NODE_ID + SEQ identify the frame being acknowledged) | 0 |

### Rules
- Node sends ULTRASONIC every 1 s and READINGS every 5 s (switches + periods in `sensor-node/include/config.h`).
- After each send the node waits 100 ms for an ACK and resends the same frame (same SEQ) up to 3 more times.
- SEQ is one counter per node for all message types. The gateway ACKs every valid frame (duplicates too, since the first ACK may have been lost), drops a repeated SEQ, and counts SEQ gaps as lost frames. SEQ 0 after a higher SEQ = node reboot, not loss.
- Code: node `sensor-node/src/link.cpp`, gateway `gateway/src/node_link.cpp`.

### Bring-up test over ESP-NOW (default)

No wires between the boards. Flash the node (`sensor-node/`, default config) and the gateway (env `gateway_bringup`), then open both monitors. Expected:
```
[link] gateway found on channel 1: 30:ED:A0:BD:5D:68
[node] seq=12 dist=153 mm ACK (tries=1) delivered 13/13
I node_link: node=1 seq=12 dist=153 mm
I node_link: stats: bytes_10s=120 ... lost=0 bad=0 delivery=100.0%
```
Set `FEATURE_LINK_TEST 1` (and `FEATURE_ULTRASONIC 0`) in the node's `config.h` to send a counting test pattern without any sensor.

### Bring-up test (node -> gateway over UART)

Wiring: node GPIO17 (TX2) -> gateway GPIO18 (RX1), node GPIO16 (RX2) <- gateway GPIO17 (TX1), **common GND**. HC-SR04 on the node: see `docs/pin-map.md` (ECHO needs a divider).

1. Node: `sensor-node/`, default config (ultrasonic on, env off). Upload + monitor.
2. Gateway: `gateway/`, env `gateway_bringup`. Upload + monitor (on Windows build from a path without spaces, see `gateway/README.md`).

Expected:
```
[node] seq=12 dist=153 mm ACK (tries=1) delivered 13/13
I (...) node_link: node=1 seq=12 dist=153 mm
I (...) node_link: stats: bytes=... frames=13 dup=0 lost=0 bad=0 delivery=100.0%
```

| Symptom | Likely cause |
|---|---|
| gateway `no bytes received` | TX/RX not crossed, no common GND |
| gateway `bytes but no valid frames` | baud mismatch, loose wire |
| gateway shows readings, node says `NO ACK` | gateway TX (GPIO17) -> node RX (GPIO16) wire |
| `dist=NO_ECHO` always | ECHO/TRIG wiring, HC-SR04 not on 5 V |

More ways to check the link (node ACK LED, gateway ONLINE/OFFLINE, per-board loopback self-test, PC sniffer / fake node / fake gateway): see [uart-link-checks.md](uart-link-checks.md).

Delivery rate (node `delivered a/b`, gateway `delivery=`) is the packet delivery metric for the report: repeat runs and record mean/min/max in `docs/test-plan.md`.

## Gateway -> Cloud

### MQTT topics (draft)
| Topic | Direction | Payload |
|---|---|---|
| `garage/<gw>/node/<id>/readings` | gw -> cloud | JSON readings + ts + seq |
| `garage/<gw>/status` | gw -> cloud | door state, bay occupied, IR obstacle, online (retained) |
| `garage/<gw>/events` | gw -> cloud | door opened/closed, face matched/rejected, fault |
| `garage/<gw>/door/cmd` | cloud -> gw | `open` / `close` / `stop` |

### ประวัติช่องจอดรถ
Gateway ส่ง `POST /api/parking` ผ่าน HTTPS ไปยัง Backend ที่บันทึกลง Supabase ดู [สัญญา API และการตั้งค่า](parking-mvp.md)

### LINE alerts (draft)
Door fault/timeout, unknown face at entrance, obstacle blocking close, node offline.
