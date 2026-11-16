# Test plan

## Test cases
| # | Case | Type | Steps | Expected | Result |
|---|---|---|---|---|---|
| T1 | Node readings reach Sheets | normal | | | |
| T2 | Enrolled face opens door | normal | | | |
| T3 | Unknown face, door stays closed + LINE alert | edge | | | |
| T4 | Obstacle during closing | edge | | | |
| T5 | UART unplugged and replugged | failure | | | |
| T6 | Wi-Fi down 5 min, then back: buffered data replayed | failure | | | |
| T7 | Gateway power loss, auto-recovers | failure | | | |

## Metrics (at least 3, repeated, mean + min/max)
| Metric | Method | Runs | Mean | Min | Max |
|---|---|---|---|---|---|
| End-to-end latency (sensor -> dashboard) | | | | | |
| Packet delivery rate (node -> gateway) | | | | | |
| Face recognition accuracy | | | | | |
| Uptime (24 h run) | | | | | |
