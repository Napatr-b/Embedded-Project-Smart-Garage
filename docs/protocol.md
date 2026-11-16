# Protocols

## Node -> Gateway (UART, 115200 8N1)

Defined in `shared/protocol/garage_protocol.h`.

| Byte | Field |
|---|---|
| 0 | SOF `0xA5` |
| 1 | Version (1) |
| 2 | Type: `0x01` READINGS, `0x80` ACK, `0x81` NACK |
| 3 | Node ID |
| 4-5 | Sequence number (LE) |
| 6 | Payload length |
| 7.. | Payload |
| last 2 | CRC16-CCITT over bytes 1..end of payload (LE) |

- Node sends READINGS every 5 s and waits 200 ms for an ACK, retrying up to 3 times.
- Gateway ACKs every valid READINGS (including duplicates) and drops a duplicate SEQ per node.
- TODO: measure packet delivery rate (metric for section IV).

## Gateway -> Cloud

### MQTT topics (draft)
| Topic | Direction | Payload |
|---|---|---|
| `garage/<gw>/node/<id>/readings` | gw -> cloud | JSON readings + ts + seq |
| `garage/<gw>/status` | gw -> cloud | door state, bay occupied, IR obstacle, online (retained) |
| `garage/<gw>/events` | gw -> cloud | door opened/closed, face matched/rejected, fault |
| `garage/<gw>/door/cmd` | cloud -> gw | `open` / `close` / `stop` |

### Google Sheets
See `cloud/apps-script/Code.gs`.

### LINE alerts (draft)
Door fault/timeout, unknown face at entrance, obstacle blocking close, node offline.
