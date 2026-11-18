# Checking the Node ↔ Gateway UART link

There are several ways to check the link. Each one tests a different part, so when the link fails, use them to narrow down **which board or which wire** is the problem.

Wiring reminder: node GPIO17 (TX) → gateway GPIO18 (RX), node GPIO16 (RX) ← gateway GPIO17 (TX), **common GND**. Format: 115200 8N1. Frame format: [protocol.md](protocol.md).

| # | Check | Needs | What a pass proves |
|---|---|---|---|
| 1 | Serial logs on both boards | 2 USB cables | Both boards see each other (already existed) |
| 2 | **ACK LED** on the node | nothing, just look | Full round trip works, no laptop needed |
| 3 | **Gateway ONLINE/OFFLINE + bytes_10s** | gateway USB | Data is arriving *right now*, not only at some point in the past |
| 4 | **Loopback self-test** (each board alone) | 1 jumper wire | That board's UART pins + driver work |
| 5 | **PC probe** `tools/uart_probe.py` | USB-TTL adapter (3.3 V) | What is really on the wire; test one board without the other |

## 2. ACK LED (node)

The blue onboard LED (GPIO2 on the DOIT DevKit) flashes once for each frame the gateway **ACKed**.

- Steady 1 Hz blink: link OK in both directions (ultrasonic sends every 1 s).
- Dark: no ACKs. The node may still be sending; the problem is the gateway, the node→gateway wire, or the gateway→node wire.

The LED lights only after a valid ACK comes back, so one blink means node TX → gateway RX → gateway CRC OK → gateway TX → node RX all worked.
Turn it off with `PIN_LINK_LED -1` in `sensor-node/include/pins.h`.

## 3. Gateway ONLINE / OFFLINE and bytes_10s

`gateway/src/node_link.cpp` now logs:

```
I node_link: node=1 ONLINE (first valid frame seq=0)
I node_link: stats: bytes_10s=130 bytes=2210 frames=170 dup=0 lost=0 bad=0 delivery=100.0%
I node_link:   node=1 online, last frame 0.4 s ago, last seq=169
W node_link: node=1 OFFLINE (no valid frame for 6.0 s)      <- pull a wire to see this
```

- `bytes_10s=0` means nothing is arriving now, even if the total `bytes` is large.
- OFFLINE triggers after 6 s with no valid frame. That is longer than the slowest send period (READINGS every 5 s).
- Live demo: pull the node TX wire to get OFFLINE, plug it back to get ONLINE. The node side shows `NO ACK` and retries meanwhile.

## 4. Loopback self-test (one board, one jumper)

Unplug the link wires between the boards, then put **one jumper from TX to RX on the same board**. The board sends 20 real frames to itself and checks that each comes back with a valid CRC, byte for byte. This repeats every 2 s, so you can wiggle the jumper and watch the result change.

| Board | Jumper | How to run |
|---|---|---|
| Node | GPIO17 → GPIO16 | `LINK_LOOPBACK_TEST 1` in `sensor-node/include/config.h`, upload, monitor |
| Gateway | GPIO17 → GPIO18 | PlatformIO env `gateway_uart_loopback`, upload, monitor |

```
[loopback] 20/20 frames echoed OK, 240 bytes back: PASS, this board's UART TX/RX pins work
[loopback] 0/20 frames echoed OK, 0 bytes back: FAIL, nothing back: jumper GPIO17 -> GPIO16 missing, or pin dead
```

If both boards PASS but the real link fails, the problem is in the wiring between the boards (crossed TX/RX, no common GND, a bad jumper wire). **Set `LINK_LOOPBACK_TEST` back to 0 afterwards.**

## 5. PC probe (`tools/uart_probe.py`)

A USB-TTL adapter (CP2102 / CH340 / FT232, **3.3 V logic**) lets the PC read or join the link. `pip install pyserial`.
The script uses the same frame parser and CRC16 as `garage_protocol.h`. It also counts CRC errors, which the firmware drops without logging.

**No adapter?** Use a spare ESP32 DevKit: wire its **EN pin to GND** (this holds its chip in reset), then use its TX0/RX0 header pins. They connect straight to the onboard USB-UART chip.

| Mode | Wiring (always add GND) | Use it to |
|---|---|---|
| `sniff` | adapter RX → the wire you want to watch, adapter TX unconnected | See the real frames while both boards run. Watch node GPIO17 for data, gateway GPIO17 for ACKs |
| `fake-gateway` | adapter RX ← node GPIO17, adapter TX → node GPIO16 | Test the **node alone**: the PC decodes and ACKs, so the node LED should blink |
| `fake-node` | adapter TX → gateway GPIO18, adapter RX ← gateway GPIO17 | Test the **gateway alone**: the PC sends ULTRASONIC frames as node 7 and measures the ACK round trip |

```
python tools/uart_probe.py sniff --port COM5
python tools/uart_probe.py sniff --port COM5 --raw          # hex dump too (to diagnose a baud mismatch)
python tools/uart_probe.py fake-gateway --port COM5
python tools/uart_probe.py fake-node --port COM5 --count 100
```

`fake-node` prints `delivered a/b` and the round trip time (mean/min/max). The time includes the adapter's USB latency, so treat it as an upper bound.

## Which check first?

```
Node LED blinking? ── yes ─▶ link OK. Read gateway stats for delivery %.
        │ no
        ▼
Gateway log: bytes_10s > 0?
  ├─ 0          ─▶ node→gateway wire / GND, or node not sending. Loopback-test the node.
  ├─ >0, bad/no frames ─▶ baud or noise. Use sniff --raw.
  └─ frames OK, node says NO ACK ─▶ gateway TX (GPIO17) → node RX (GPIO16) wire. Loopback-test the gateway.
Still unclear ─▶ fake-gateway (node alone) and fake-node (gateway alone) show which board is at fault.
```

**Status:** written and syntax-checked on the host. The Python frame encoding and decoding was cross-checked byte for byte against `garage_protocol.h`. **Not yet run on real boards.**
