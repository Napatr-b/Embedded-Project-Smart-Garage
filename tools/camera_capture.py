"""Request one JPEG from the camera-test firmware over USB serial."""

import argparse
from pathlib import Path
from time import monotonic, sleep

import serial


def capture(port):
    with serial.Serial(port, 115200, timeout=0.5, write_timeout=2) as connection:
        # Opening the port can reset the ESP32 through its USB-UART bridge.
        sleep(3)
        connection.reset_input_buffer()
        connection.write(b"c")

        deadline = monotonic() + 8
        while monotonic() < deadline:
            line = connection.read_until(size=256).strip()
            if line.startswith(b"ERR "):
                raise ValueError(line.decode("ascii", errors="replace"))
            if line.startswith(b"JPEG "):
                length = int(line[5:])
                if not 4 <= length <= 320 * 240 * 2:
                    raise ValueError(f"Invalid JPEG size: {length}")
                connection.timeout = 20
                image = connection.read(length)
                if len(image) != length or not (
                    image.startswith(b"\xff\xd8") and image.endswith(b"\xff\xd9")
                ):
                    raise ValueError("Incomplete or invalid JPEG")
                return image
        raise ValueError("No camera response. Check the port and firmware.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Gateway USB serial port, e.g. COM5")
    parser.add_argument("output", type=Path, help="Destination JPEG file")
    args = parser.parse_args()
    try:
        args.output.write_bytes(capture(args.port))
    except (OSError, ValueError, serial.SerialException) as error:
        parser.exit(1, f"{error}\n")
    print(f"Saved {args.output}")


if __name__ == "__main__":
    main()
