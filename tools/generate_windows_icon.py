#!/usr/bin/env python3
"""Wrap the original PNGs in a multi-resolution ICO (no resampling)."""
from pathlib import Path
import struct

root = Path(__file__).resolve().parents[1]
images = []
for size in (16, 32, 64, 128, 256):
    data = (root / f'assets/images/SpentIt_light_{size}.png').read_bytes()
    if data[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', data[16:24]) != (size, size):
        raise SystemExit(f'Invalid PNG for size {size}')
    images.append((size, data))
offset = 6 + 16 * len(images)
entries = []
for size, data in images:
    entries.append(struct.pack('<BBBBHHII', size % 256, size % 256, 0, 0, 1, 32, len(data), offset))
    offset += len(data)
(root / 'assets/images/SpentIt.ico').write_bytes(
    struct.pack('<HHH', 0, 1, len(images)) + b''.join(entries) + b''.join(data for _, data in images))
