"""Synthetic-only privacy checks for the local diagnostic executable."""
from pathlib import Path
import subprocess
import sys
import tempfile

binary = str(Path(sys.argv[1]).resolve())
with tempfile.TemporaryDirectory() as directory:
    source = Path(directory) / 'PRIVATE_SYNTHETIC_FILENAME.qif'
    payload = b'!Type:Bank\nD2026-01-01\nT1\nPPRIVATE_SYNTHETIC_PAYEE\rT999\nSPRIVATE_SYNTHETIC_CATEGORY\n^\n'
    source.write_bytes(payload)
    result = subprocess.run([binary, 'CAD', 'dmy'], input=str(source)+'\n', text=True, capture_output=True, check=False)
    output = result.stdout + result.stderr
    assert result.returncode == 1, output
    assert 'source lines 2–6' in output and 'missing its $ amount' in output, output
    assert 'fields: D T P S ^' in output, output
    assert 'PRIVATE_' not in output and directory not in output and '999' not in output, output
    assert source.read_bytes() == payload
    assert list(Path(directory).iterdir()) == [source]
    accepted = b'!Type:Bank\nD2026-01-01\nT1\nEPRIVATE_SYNTHETIC_MEMO\nSPRIVATE_SYNTHETIC_CATEGORY\n$1\n^\n'
    source.write_bytes(accepted)
    passed = subprocess.run([binary, 'CAD', 'dmy'], input=str(source)+'\n', text=True, capture_output=True, check=False)
    assert passed.returncode == 0 and 'diagnostics: 0' in passed.stdout
    assert 'PRIVATE_' not in passed.stdout+passed.stderr and source.read_bytes() == accepted
    failed = subprocess.run([binary, 'CAD', 'dmy'], input=str(source)+'MISSING\n', text=True, capture_output=True, check=False)
    assert failed.returncode == 2 and 'PRIVATE_' not in failed.stdout+failed.stderr
    invalid = subprocess.run([binary, 'PRIVATE_INVALID_CURRENCY', 'dmy'], text=True, capture_output=True, check=False)
    assert invalid.returncode == 2 and 'PRIVATE_' not in invalid.stdout+invalid.stderr
print('Synthetic diagnostic privacy/read-only checks passed')
