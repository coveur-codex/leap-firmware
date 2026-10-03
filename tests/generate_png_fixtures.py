"""Generate synthetic PNG fixtures; no user artwork is modified."""
import struct, zlib, sys
from pathlib import Path
root = Path(sys.argv[1])
root.mkdir(parents=True, exist_ok=True)
def chunk(tag,data):
 return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
for width,height in [(80,80),(256,142),(428,142),(1024,8)]:
 for mode,channels in [(2,3),(6,4)]:
  rows=[]
  for y in range(height):
   row=bytearray([0])
   for x in range(width):
    row.extend(((x*13+y*7)%256,(x*3+y*17)%256,(x*11+y*5)%256))
    if channels==4:row.append(255)
   rows.append(row)
  data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,mode,0,0,0))+chunk(b'IDAT',zlib.compress(b''.join(rows)))+chunk(b'IEND',b'')
  (root / f'{width}-{channels}.png').write_bytes(data)

# Transparent RGB is deliberately non-black; alpha must determine the background.
width, height = 80, 80
rows = []
for y in range(height):
 row = bytearray([0])
 for x in range(width):
  rgb = (0, 0, 0) if (x, y) == (2, 0) else ((x*13+y*7)%256, (x*3+y*17)%256, (x*11+y*5)%256)
  row.extend((*rgb, (0, 127, 255)[x % 3]))
 rows.append(row)
(root / '80-alpha.png').write_bytes(
 b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
 + chunk(b'IDAT', zlib.compress(b''.join(rows))) + chunk(b'IEND', b''))

# Decoder must still reject invalid headers and unsupported interlacing.
valid = (root / "80-4.png").read_bytes()
corrupt = bytearray(valid)
corrupt[29] ^= 1  # IHDR CRC
(root / "crc-reject.png").write_bytes(corrupt)
interlaced = bytearray(valid[16:29])
interlaced[-1] = 1
(root / "interlace-reject.png").write_bytes(valid[:8] + chunk(b"IHDR", interlaced) + valid[33:])
