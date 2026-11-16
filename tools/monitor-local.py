import serial
import time
import re
from pathlib import Path

s = serial.Serial('COM3', 115200, timeout=1)
end = time.time() + 25
with Path('local-test/serial-test.log').open('w', encoding='utf-8') as out:
    while time.time() < end:
        line = s.readline().decode(errors='replace').strip()
        if line:
            line = re.sub(r'(?i)(password|ssid|username)[=: ].*', '[redacted]', line)
            print(line, flush=True)
            out.write(line + '\n')
s.close()
