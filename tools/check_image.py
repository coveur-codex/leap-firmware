"""Quick artifact sanity check; the device additionally uses esp_ota_end()."""
from pathlib import Path
import hashlib
import sys
p = Path(sys.argv[1])
data = p.read_bytes()
assert 24 < len(data) <= 4 * 1024 * 1024, "App must fit the 4 MiB inactive slot"
assert data[0] == 0xE9 and int.from_bytes(data[12:14], "little") == 9, "Expected ESP32-S3 app"
print(f"PASS: {p.name}: {len(data)} bytes, ESP32-S3, SHA256 {hashlib.sha256(data).hexdigest()}")
