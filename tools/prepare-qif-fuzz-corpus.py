#!/usr/bin/env python3
"""Copy synthetic seeds and generate large boundary seeds in an ignored corpus."""
import argparse
from pathlib import Path
import shutil
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
root = Path(__file__).resolve().parents[1]
for source in (root / 'tests/fixtures/qif/fuzz').glob('*.qif'):
    shutil.copyfile(source, args.output / source.name)
for count in (65536, 65537):
    (args.output / f'field-{count}.qif').write_text('D1/1/2026\nT1\nP' + 'x' * count + '\n^\n')
for count in (1000, 1001):
    (args.output / f'splits-{count}.qif').write_text('D1/1/2026\nT0\n' + 'SFood\n$0\n' * count + '^\n')
    (args.output / f'accounts-{count}.qif').write_text(''.join(
        f'!Account\nNAccount {i}\nTBank\n^\n!Type:Bank\nD1/1/2026\nT0\n^\n'
        for i in range(count)))
(args.output / 'invalid-utf8.qif').write_bytes(b'D1/1/2026\nT1\nP\xff\n^\n')
(args.output / 'bom-crlf.qif').write_bytes(b'\xef\xbb\xbf!Type:Bank\r\nD1/1/2026\r\nT1\r\n^\r\n')
