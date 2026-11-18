#!/usr/bin/env python3
"""
uart_probe.py: check the node <-> gateway UART link from a PC, using a USB-TTL adapter (3.3 V!).

Decodes the same frames as shared/protocol/garage_protocol.h (SOF 0xA5, CRC16-CCITT), so you can
see what is actually on the wire instead of trusting either board's own log.

Modes
  sniff         Listen only. Adapter RX -> the wire you want to watch, adapter GND -> common GND.
                Leave adapter TX unconnected. Both boards stay wired to each other.
                  node TX (node GPIO17)    -> see READINGS / ULTRASONIC frames
                  gateway TX (gw GPIO17)   -> see ACK frames
  fake-gateway  The PC replaces the gateway: decodes node frames and ACKs them.
                Tests the NODE alone. Adapter RX <- node GPIO17, adapter TX -> node GPIO16, GND.
  fake-node     The PC replaces the node: sends ULTRASONIC frames every second and waits for ACKs.
                Tests the GATEWAY alone. Adapter TX -> gateway GPIO18, adapter RX <- gateway GPIO17, GND.

Add --raw to also hex-dump every byte (useful when nothing decodes: wrong baud shows up as garbage).

No USB-TTL adapter? A spare ESP32 DevKit works as one: wire its EN pin to GND (holds the ESP32 in
reset) and use its TX0/RX0 header pins; they go straight to the onboard USB-UART chip.

Needs: pip install pyserial
Examples:
  python tools/uart_probe.py sniff --port COM5
  python tools/uart_probe.py fake-gateway --port COM5
  python tools/uart_probe.py fake-node --port COM5 --count 100
"""
import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial missing: pip install pyserial")

# ---- protocol (keep in sync with shared/protocol/garage_protocol.h) ----
SOF, VERSION, MAX_PAYLOAD, HEADER_LEN, CRC_LEN = 0xA5, 1, 32, 7, 2
MSG_READINGS, MSG_ULTRASONIC, MSG_ACK, MSG_NACK = 0x01, 0x02, 0x80, 0x81
TYPE_NAMES = {MSG_READINGS: "READINGS", MSG_ULTRASONIC: "ULTRASONIC", MSG_ACK: "ACK", MSG_NACK: "NACK"}
ACK_TIMEOUT_S, MAX_RETRIES = 0.100, 3  # same as sensor-node/include/config.h


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def build_frame(msg_type: int, node_id: int, seq: int, payload: bytes = b"") -> bytes:
    body = bytes([VERSION, msg_type, node_id, seq & 0xFF, (seq >> 8) & 0xFF, len(payload)]) + payload
    return bytes([SOF]) + body + struct.pack("<H", crc16(body))


class Parser:
    """Byte-by-byte parser, same state machine as gp_parser_feed(). Also counts CRC failures,
    which the firmware parser drops silently: a non-zero crc_errors means noise or a loose wire."""

    def __init__(self):
        self.buf = bytearray()
        self.expected = 0
        self.crc_errors = 0

    def feed(self, byte: int):
        if not self.buf and byte != SOF:
            return None
        self.buf.append(byte)
        if len(self.buf) == HEADER_LEN:
            if self.buf[1] != VERSION or self.buf[6] > MAX_PAYLOAD:
                self.buf.clear()
                return None
            self.expected = HEADER_LEN + self.buf[6] + CRC_LEN
        if len(self.buf) >= HEADER_LEN and len(self.buf) == self.expected:
            frame = bytes(self.buf)
            self.buf.clear()
            if crc16(frame[1:-2]) == struct.unpack_from("<H", frame, len(frame) - 2)[0]:
                return frame
            self.crc_errors += 1
        return None


def describe(frame: bytes) -> str:
    msg_type, node_id = frame[2], frame[3]
    seq = struct.unpack_from("<H", frame, 4)[0]
    payload = frame[HEADER_LEN:-CRC_LEN]
    head = f"{TYPE_NAMES.get(msg_type, hex(msg_type)):<10} node={node_id} seq={seq:<5}"
    if msg_type == MSG_ULTRASONIC and len(payload) == 3:
        dist, status = struct.unpack("<HB", payload)
        return head + (f" dist={dist} mm" if status == 0 else " dist=NO_ECHO")
    if msg_type == MSG_READINGS and len(payload) == 12:
        t, h, ldr, eco2, tvoc, aqi, led = struct.unpack("<hHHHHBB", payload)
        return head + f" T={t / 100:.2f} H={h / 100:.2f} ldr={ldr} eco2={eco2} tvoc={tvoc} aqi={aqi} led={led}"
    return head + (f" payload={payload.hex(' ')}" if payload else "")


class Stats:
    def __init__(self):
        self.bytes = self.frames = self.dups = self.lost = 0
        self.last_seq = {}

    def frame(self, frame: bytes):
        """Counts dup / lost by SEQ per node, same rules as the gateway (docs/protocol.md)."""
        node_id, seq = frame[3], struct.unpack_from("<H", frame, 4)[0]
        last = self.last_seq.get(node_id)
        if last == seq:
            self.dups += 1
            return "dup"
        if last is not None and seq != 0:
            self.lost += (seq - last - 1) & 0xFFFF
        self.last_seq[node_id] = seq
        self.frames += 1
        return ""

    def line(self, parser: Parser) -> str:
        expected = self.frames + self.lost
        delivery = f"{100 * self.frames / expected:.1f}%" if expected else "-"
        return (f"stats: bytes={self.bytes} frames={self.frames} dup={self.dups} lost={self.lost} "
                f"crc_err={parser.crc_errors} delivery={delivery}")


def hint(stats: Stats, parser: Parser, mode: str) -> str:
    if stats.bytes == 0:
        wire = {"sniff": "adapter RX on the TX wire you want to watch",
                "fake-gateway": "node GPIO17 -> adapter RX",
                "fake-node": "gateway GPIO17 -> adapter RX"}[mode]
        return f"  -> no bytes at all: check {wire}, common GND, board powered and running"
    if stats.frames == 0:
        return "  -> bytes but no valid frame: baud mismatch or noisy/loose wire (try --raw)"
    return ""


def open_port(args):
    try:
        return serial.Serial(args.port, args.baud, timeout=0.02)
    except serial.SerialException as e:
        sys.exit(f"cannot open {args.port}: {e}")


def listen(args, ack: bool):
    """sniff (ack=False) and fake-gateway (ack=True)."""
    port, parser, stats = open_port(args), Parser(), Stats()
    mode = "fake-gateway" if ack else "sniff"
    print(f"[{mode}] {args.port} @ {args.baud}. Ctrl+C to stop.")
    start = last_stats = time.time()
    try:
        while args.seconds <= 0 or time.time() - start < args.seconds:
            data = port.read(64)
            if data and args.raw:
                print("  raw:", data.hex(" "))
            for b in data:
                stats.bytes += 1
                frame = parser.feed(b)
                if not frame:
                    continue
                if ack and frame[2] in (MSG_READINGS, MSG_ULTRASONIC):
                    # ACK echoes NODE_ID + SEQ, exactly like gateway/src/node_link.cpp.
                    port.write(build_frame(MSG_ACK, frame[3], struct.unpack_from("<H", frame, 4)[0]))
                # ACKs carry the SEQ of the frame they confirm, so don't run them through loss counting.
                note = stats.frame(frame) if frame[2] < 0x80 else ""
                print(f"{time.strftime('%H:%M:%S')} {describe(frame)} {note}".rstrip())
            if time.time() - last_stats >= 5:
                last_stats = time.time()
                print(stats.line(parser) + "\n" + hint(stats, parser, mode))
    except KeyboardInterrupt:
        pass
    print(stats.line(parser))
    port.close()


def fake_node(args):
    port, parser = open_port(args), Parser()
    print(f"[fake-node] {args.port} @ {args.baud}, node_id={args.node_id}, sending {args.count} frame(s).")
    acked = sent = total_tries = 0
    latencies = []
    try:
        for seq in range(args.count):
            dist = 100 + (seq * 7) % 300  # fake distance that changes so you can spot it on the gateway log
            frame = build_frame(MSG_ULTRASONIC, args.node_id, seq, struct.pack("<HB", dist, 0))
            port.reset_input_buffer()
            ok, tries = False, 0
            while not ok and tries <= MAX_RETRIES:
                tries += 1
                t0 = time.perf_counter()
                port.write(frame)
                while time.perf_counter() - t0 < ACK_TIMEOUT_S and not ok:
                    for b in port.read(16):
                        f = parser.feed(b)
                        if f and f[2] == MSG_ACK and f[3] == args.node_id and struct.unpack_from("<H", f, 4)[0] == seq:
                            ok = True
                            latencies.append((time.perf_counter() - t0) * 1000)
            sent += 1
            total_tries += tries
            acked += ok
            print(f"seq={seq:<4} dist={dist} mm -> {'ACK' if ok else 'NO ACK'} (tries={tries}) delivered {acked}/{sent}")
            time.sleep(args.period)
    except KeyboardInterrupt:
        pass
    if latencies:
        # Includes USB-serial latency of the adapter (often 1-16 ms), so it's an upper bound on the UART round trip.
        print(f"round trip ms: mean={sum(latencies) / len(latencies):.1f} "
              f"min={min(latencies):.1f} max={max(latencies):.1f}")
    print(f"delivered {acked}/{sent}, retries used={total_tries - sent}, crc_err={parser.crc_errors}")
    if acked == 0:
        print("  -> no ACK: check adapter TX -> gateway GPIO18, gateway GPIO17 -> adapter RX, common GND, "
              "and that the gateway log shows node_link 'listening on UART1'")
    port.close()


def main():
    ap = argparse.ArgumentParser(description="Check the Smart Garage node<->gateway UART link from a PC.")
    ap.add_argument("mode", choices=["sniff", "fake-gateway", "fake-node"])
    ap.add_argument("--port", required=True, help="USB-TTL adapter port, e.g. COM5 or /dev/ttyUSB0")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--raw", action="store_true", help="hex-dump every received byte")
    ap.add_argument("--seconds", type=float, default=0, help="sniff/fake-gateway: stop after N s (0 = until Ctrl+C)")
    ap.add_argument("--count", type=int, default=30, help="fake-node: frames to send")
    ap.add_argument("--period", type=float, default=1.0, help="fake-node: seconds between frames")
    ap.add_argument("--node-id", type=int, default=7, help="fake-node: node ID (default 7, so it's distinct from the real node 1)")
    args = ap.parse_args()
    if args.mode == "fake-node":
        fake_node(args)
    else:
        listen(args, ack=args.mode == "fake-gateway")


if __name__ == "__main__":
    main()
