"""Offline regression: simulate CAD withdrawal, rebuild, reload its retained amount."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

parser = argparse.ArgumentParser()
parser.add_argument('--compiler', default='c++')
args = parser.parse_args()
root = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='spentit-currency-refresh-') as temp:
    work = Path(temp)
    for directory in ('src/engine', 'third_party/iso4217', 'tools'):
        shutil.copytree(root / directory, work / directory)
    reference = work / 'third_party/iso4217'
    xml = ET.parse(reference / 'list-one.xml')
    table = xml.find('.//CcyTbl')
    for entry in list(table):
        if entry.findtext('Ccy') == 'CAD':
            table.remove(entry)
    xml.write(reference / 'list-one.xml')
    selection = json.loads((reference / 'current-definitions.json').read_text())
    del selection['CAD']
    (reference / 'current-definitions.json').write_text(json.dumps(selection))
    subprocess.run([sys.executable, str(work / 'tools/generate_currency_catalog.py')], check=True)
    assert (work / 'src/engine/amount_currency_definitions.inc').read_bytes() == (
        root / 'src/engine/amount_currency_definitions.inc').read_bytes()
    probe = work / 'probe.cpp'
    probe.write_text('''#include <spentit/money.hpp>
int main() {
    using namespace spentit;
    auto amount = Money(-2931, Currency::from_definition("six-2026-09-17:CAD"));
    if (amount.units() != -2931 || amount.currency().code() != "CAD" || amount.currency().scale() != 2) return 1;
    try { Currency current("CAD"); return 2; }
    catch (const FinancialError& e) { if (e.code() != ErrorCode::unsupported_currency) return 3; }
    try { Currency::from_definition("unresolved:CAD"); return 4; }
    catch (const FinancialError& e) { if (e.code() != ErrorCode::unresolved_currency_definition) return 5; }
}
''')
    subprocess.run([args.compiler, '-std=c++20', '-I' + str(work / 'src/engine/include'),
                    str(work / 'src/engine/money.cpp'), str(probe), '-o', str(work / 'probe')], check=True)
    subprocess.run([str(work / 'probe')], check=True)
print('Simulated catalog withdrawal preserves exact historical CAD amount')
