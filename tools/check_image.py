"""Artifact sanity/identity check; the device also validates the complete ESP image."""
from pathlib import Path
import argparse
import hashlib
import json
import re

parser = argparse.ArgumentParser()
parser.add_argument("image", type=Path)
parser.add_argument("--manifest", type=Path)
args = parser.parse_args()
p = args.image
data = p.read_bytes()
assert 24 < len(data) <= 4 * 1024 * 1024, "App must fit the 4 MiB inactive slot"
assert data[0] == 0xE9 and int.from_bytes(data[12:14], "little") == 9, "Expected ESP32-S3 app"
assert b"LEAP_UNIVERSAL_NVS_V1" in data, "OTA must load device configuration from NVS"
assert b"LEAP_DEVICE_PROVISIONING_V1" not in data, "Never distribute a provisioning image over OTA"
build = (Path(__file__).resolve().parents[1] / "LEAP/src/FirmwareBuild.h").read_text()
expected = re.search(r'^#define LEAP_FIRMWARE_VERSION "([^"]+)"$', build, re.M).group(1)
versions = set(re.findall(rb"LEAP_APP_VERSION_V1=([0-9a-z.-]{1,40});\x00", data))
assert versions == {expected.encode()}, f"Expected embedded firmware version {expected}, got {versions!r}"
digest = hashlib.sha256(data).hexdigest()
if args.manifest:
    args.manifest.write_text(json.dumps({"version": expected, "file": p.name,
        "size": len(data), "sha256": digest, "chip": "esp32s3", "mode": "universal"}, indent=2)+"\n")
    p.with_suffix(p.suffix+".sha256").write_text(f"{digest}  {p.name}\n")
print(f"PASS: {p.name}: version={expected}, {len(data)} bytes, ESP32-S3, SHA256 {digest}")
