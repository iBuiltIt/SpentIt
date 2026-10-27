#!/usr/bin/env python3
"""Build a pinned standalone libFuzzer archive; no system installation or download."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
from pathlib import Path
import subprocess
import tarfile
import sys

if sys.version_info < (3, 12):
    raise SystemExit("Use Python 3.12 or later for safe archive extraction")

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('archive', type=Path)
parser.add_argument('--output', type=Path, default=Path('build/libfuzzer-runtime'))
parser.add_argument('--compiler', default='clang++')
args = parser.parse_args()
expected = '11b8d09dcf92a0f91c5c82defb5ad9ff4acf5cf073a80c317204baa922d136b4'
if hashlib.sha256(args.archive.read_bytes()).hexdigest() != expected:
    raise SystemExit('compiler-rt 17.0.6 archive hash mismatch')
out = args.output.resolve()
out.mkdir(parents=True, exist_ok=True)
with tarfile.open(args.archive) as archive:
    archive.extractall(out, filter='data')
source = out / 'compiler-rt-17.0.6.src/lib/fuzzer'

def compile_one(path):
    # Match ASan libc++ container annotations in the harness. The upstream
    # runtime is not coverage-instrumented and uses sanitizer-internal pointer
    # arithmetic that is not suitable for UBSan; the target retains both.
    subprocess.run([args.compiler, '-std=c++17', '-g', '-O2',
                    '-fno-omit-frame-pointer', '-fsanitize=address', '-c',
                    str(path), '-o', str(out / (path.stem + '.o'))], check=True)

with ThreadPoolExecutor(max_workers=2) as pool:
    list(pool.map(compile_one, sorted(source.glob('*.cpp'))))
subprocess.run(['ar', 'rcs', str(out / 'libFuzzer.a'),
                *map(str, sorted(out.glob('Fuzzer*.o')))], check=True)
print(out / 'libFuzzer.a')
