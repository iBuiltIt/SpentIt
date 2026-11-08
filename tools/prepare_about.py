#!/usr/bin/env python3
"""Bundle reviewed, offline notices. Never retrieve licenses at application runtime."""
import argparse
import json
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--notices', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--qt-version', required=True)
parser.add_argument('--platform', required=True)
parser.add_argument('--qt-cmake-dir', type=Path, required=True)
args = parser.parse_args()
rows = []
coverage = []
# This reviewed macOS profile is tied to the installed Homebrew dependency set.
# Never label a different installation with those versions merely because it is macOS.
brew_prefix = next((p.parent for p in args.qt_cmake_dir.resolve().parents if p.name == 'Cellar'), None)
for entry in json.loads((args.notices / 'manifest.json').read_text()):
    if entry['platform'] not in ('all', args.platform):
        continue
    if 'package' in entry:
        actual = (brew_prefix / 'opt' / entry['package']).resolve() if brew_prefix else None
        if actual is None or not actual.exists() or actual.name != entry['version']:
            coverage.append(entry['name'] + ': installed version does not match the reviewed notice set.')
            continue
    version = entry['version'].replace('@QT_VERSION@', args.qt_version)
    label = entry['name'] + ' — ' + version
    text = label + '\n\n' + entry.get('note', '') + '\n\n'
    text += '\n\n'.join((args.notices / name).read_text() for name in entry['files'])
    rows.append({'label': label, 'text': text})
# Do not pass off a pinned attribution catalog as an audited distribution closure.
if args.platform != 'Darwin' or args.qt_version != '6.12.0':
    coverage.append('The bundled Qt third-party catalog covers Qt 6.12.0. Additional platform dependency notices must be reviewed before distributing this build.')
if coverage:
    rows.insert(0, {'label': 'Notice coverage', 'text': '\n\n'.join(coverage)})
args.output.parent.mkdir(parents=True, exist_ok=True)
content = json.dumps(rows, ensure_ascii=False, indent=2) + '\n'
if not args.output.exists() or args.output.read_text() != content:
    args.output.write_text(content)
