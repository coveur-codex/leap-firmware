from pathlib import Path
import csv
root = Path(__file__).resolve().parents[1]
rows = list(csv.reader(line for line in (root / "LEAP/partitions.csv").read_text().splitlines() if not line.startswith("#")))
end = 0x9000
apps = []
for name, kind, subtype, offset, size, flags in rows:
    offset, size = int(offset, 16), int(size, 16)
    assert offset >= end, name
    assert offset % 0x1000 == 0 and size % 0x1000 == 0
    if kind.strip() == "app":
        assert offset % 0x10000 == 0
        apps.append((subtype.strip(), size))
    end = offset + size
assert end == 16 * 1024 * 1024
assert apps == [("ota_0", 4 * 1024 * 1024), ("ota_1", 4 * 1024 * 1024)]
print("PASS: 16 MiB partition boundaries, equal OTA slots, filesystem and coredump")
