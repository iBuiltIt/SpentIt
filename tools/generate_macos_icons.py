#!/usr/bin/env python3
"""Package the supplied PNG sizes in the existing PNG-backed ICNS layout."""
from pathlib import Path
import struct

assets = Path(__file__).resolve().parents[1] / 'assets/images'
# Standard/Retina representations, matching the original supplied ICNS files.
representations = ((b'icp4',16),(b'icp5',32),(b'icp6',64),(b'ic07',128),
                   (b'ic08',256),(b'ic09',512),(b'ic10',1024),
                   (b'ic11',32),(b'ic12',64),(b'ic13',256),(b'ic14',512))
for variant in ('light', 'dark'):
    chunks = []
    for kind, size in representations:
        data = (assets / f'SpentIt_{variant}_{size}.png').read_bytes()
        if data[:8] != b'\x89PNG\r\n\x1a\n' or struct.unpack('>II', data[16:24]) != (size, size):
            raise SystemExit(f'Invalid {variant} PNG for size {size}')
        chunks.append(struct.pack('>4sI', kind, 8 + len(data)) + data)
    body = b''.join(chunks)
    (assets / f'SpentIt_{variant}.icns').write_bytes(struct.pack('>4sI', b'icns', 8 + len(body)) + body)
