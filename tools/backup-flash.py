"""Read-only backup using the installed project's esptool. No flash/eFuse writes."""
import hashlib
import json
import os
from pathlib import Path
import sys

sys.path.insert(0, str(Path.home() / '.platformio/packages/tool-esptoolpy'))
sys.path.insert(0, str(Path.home() / '.platformio/packages/tool-esptoolpy/_contrib'))
from esptool.targets import CHIP_DEFS

root = Path(__file__).resolve().parent.parent
dest = root / 'local-test/flash-before-test.bin'
partial = dest.with_suffix('.partial')
size, block = 0x1000000, 0x20000
if dest.exists():
    raise SystemExit('Existing completed backup preserved')
esp = None
def connect():
    e = CHIP_DEFS['esp32s3']('COM3', 115200)
    e.connect()
    e = e.run_stub()
    e.change_baud(921600)
    e.flash_spi_attach(0)
    e.flash_set_parameters(size)
    return e

try:
    esp = connect()
    with partial.open('wb') as out:
        for offset in range(0, size, block):
            for attempt in range(3):
                try:
                    data = esp.read_flash(offset, block)
                    if len(data) != block:
                        raise RuntimeError('Incomplete block')
                    break
                except Exception as exc:
                    print(f'Read retry at {offset:#x}: {type(exc).__name__}', flush=True)
                    esp._port.close()
                    if attempt == 2:
                        raise
                    esp = connect()
            out.write(data)
            out.flush()
            if (offset + block) % 0x100000 == 0:
                print(f'Verified backup {(offset+block)//0x100000}/16 MiB', flush=True)
    if partial.stat().st_size != size:
        raise RuntimeError('Wrong backup size')
    os.replace(partial, dest)
    digest = hashlib.sha256(dest.read_bytes()).hexdigest()
    dest.with_suffix('.json').write_text(json.dumps({'bytes':size,'sha256':digest,'port':'COM3','chip':'ESP32-S3','blockDigestVerified':True},indent=2))
    print(f'BACKUP COMPLETE bytes={size} SHA256={digest}',flush=True)
    esp.hard_reset()
finally:
    if esp:
        esp._port.close()
