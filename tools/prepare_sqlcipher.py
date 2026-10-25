"""Verify a supplied pinned archive and generate SQLCipher sources (no downloads)."""
import argparse
import hashlib
from pathlib import Path
import subprocess
import tarfile

parser = argparse.ArgumentParser()
parser.add_argument('archive', type=Path)
parser.add_argument('--directory', type=Path, default=Path(__file__).resolve().parents[1] / 'build/dependencies')
args = parser.parse_args()
expected = '7075f96cbabe45b4ecfc2e6b1745a625f856f695b0827a5506ce9ed85b906aa0'
if hashlib.sha256(args.archive.read_bytes()).hexdigest() != expected:
    raise SystemExit('SQLCipher archive checksum mismatch')
base = args.directory.resolve()
base.mkdir(parents=True, exist_ok=True)
source = base / 'sqlcipher-4.19.0'
if not source.exists():
    with tarfile.open(args.archive) as archive:
        # Support Python 3.9 as well as newer versions with extraction filters.
        # Verify the pin first and reject traversal, links and special files.
        for member in archive.getmembers():
            relative = Path(member.name)
            if (relative.is_absolute() or '..' in relative.parts or
                    not relative.parts or relative.parts[0] != 'sqlcipher-4.19.0' or
                    not (member.isfile() or member.isdir())):
                raise SystemExit('Unexpected archive member')
        archive.extractall(base)
build = base / 'sqlcipher-build'
build.mkdir(exist_ok=True)
subprocess.run([str(source / 'configure'), '--with-tempstore=yes', '--disable-tcl'], cwd=build, check=True)
subprocess.run(['make', 'sqlite3.c'], cwd=build, check=True)
for name, digest in {
    'sqlite3.c': '8640c653acadf665cce6331646f60b5b74a4690746f2c4a2d8f688a0570a0c0c',
    'sqlite3.h': '8a9d1bff44d75174ca6dea3ea9bac50a6104d86facb566647b8bb839375b7b3a',
}.items():
    if hashlib.sha256((build / name).read_bytes()).hexdigest() != digest:
        raise SystemExit(f'Generated {name} checksum mismatch')
print(f'Pinned SQLCipher generated sources verified: {build}')
