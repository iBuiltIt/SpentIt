#!/usr/bin/env python3
"""Run only on completed synthetic UUID fixtures; preserve originals and raw logs."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

p=argparse.ArgumentParser()
p.add_argument('--probe',type=Path,required=True)
p.add_argument('--source-root',type=Path,required=True)
p.add_argument('--work',type=Path,required=True)
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args()
assert not a.work.exists(), 'Refusing existing work directory'
assert not a.evidence.exists(), 'Refusing existing evidence directory'
a.work.mkdir(parents=True)
a.evidence.mkdir(parents=True)
def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while block:=f.read(1024*1024): h.update(block)
    return h.hexdigest()
record={'probe_sha256':digest(a.probe),'variants':{}}
for variant in ('current','prototype'):
    source=a.source_root/f'import-50000-{variant}'/'import.spentit'
    before=digest(source)
    with (a.evidence/f'{variant}.txt').open('w') as log:
        result=subprocess.run([str(a.probe.resolve()),str(source.resolve()),str((a.work/variant).resolve())],stdout=log,stderr=subprocess.STDOUT)
    after=digest(source)
    record['variants'][variant]={'source_sha256_before':before,'source_sha256_after':after,'source_unchanged':before==after,'exit_code':result.returncode}
    (a.evidence/'manifest.json').write_text(json.dumps(record,indent=2)+'\n')
    assert before==after, 'Original fixture changed'
    assert result.returncode==0, 'Probe failed; inspect raw evidence'
print('Both completed synthetic originals unchanged; all copy checks passed.')
