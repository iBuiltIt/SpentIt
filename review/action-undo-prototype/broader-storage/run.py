#!/usr/bin/env python3
"""Run synthetic storage-cost comparisons. This does not set an acceptance limit.
Executables share action_storage_workloads.cpp and CLI DIR MODE ROWS.
The database work directory must be outside the repository; only text evidence
belongs in version control. Each executable is copied once to freeze the build.
"""
import argparse
import csv
import hashlib
import json
import shutil
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

p = argparse.ArgumentParser(description=__doc__)
for name in ('baseline', 'current', 'prototype', 'dbstat', 'driver', 'work', 'evidence'):
    p.add_argument('--' + name, type=Path, required=True)
p.add_argument('--jobs', type=int, choices=(1,2,3), default=1, help='Independent processes/documents; timings are not comparative benchmarks')
p.add_argument('--cases', nargs='+', default=[f'{mode}:{rows}' for rows in (1000, 10000) for mode in ('import', 'edits', 'undo', 'linked', 'mixed')])
a = p.parse_args()
if a.work.exists() or a.evidence.exists():
    p.error('Work and evidence directories must be fresh; previous results are preserved.')
cases=[]
for case in a.cases:
    mode, rows = case.split(':')
    if mode not in ('import', 'edits', 'undo', 'linked', 'mixed') or not (0 < int(rows) <= 100000):
        p.error('Invalid case: ' + case)
    cases.append((mode, int(rows)))
if len(set(cases)) != len(cases):p.error('Duplicate cases would share a document; refusing.')
a.work.mkdir(parents=True)
a.evidence.mkdir(parents=True)
manifest={'driver_sha256': hashlib.sha256(a.driver.read_bytes()).hexdigest(), 'executables':{}, 'cases':cases,
          'concurrent_jobs':a.jobs,
          'scope':'Measured prototype costs, not full unified Undo correctness/recovery acceptance or a final storage limit.'}
executables={}
for variant in ('baseline', 'current', 'prototype'):
    original=getattr(a,variant)
    frozen=a.work/('executable-'+variant)
    shutil.copy2(original,frozen)
    executables[variant]=frozen.resolve()
    manifest['executables'][variant]=hashlib.sha256(frozen.read_bytes()).hexdigest()
analyzer=a.work/'dbstat'
shutil.copy2(a.dbstat,analyzer)
analyzer=analyzer.resolve()
manifest['dbstat_sha256']=hashlib.sha256(analyzer.read_bytes()).hexdigest()
(a.evidence/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
def run_case(case):
    mode, rows, variant = case
    name=f'{mode}-{rows}-{variant}'
    root=a.work/name
    log=a.evidence/(name+'.txt')
    with log.open('w') as out:
        result=subprocess.run([str(executables[variant]),str(root.resolve()),mode,str(rows)],stdout=out,stderr=subprocess.STDOUT)
    if result.returncode:
        raise RuntimeError(f'{name} failed with exit {result.returncode}; inspect {log}')
    output=log.read_text()
    if 'FAIL' in output:
        raise RuntimeError(f'Failure marker in {log}')
    database=root/(mode+'.spentit')
    raw=subprocess.check_output([str(analyzer),str(database.resolve())],text=True)
    (a.evidence/(name+'-pages.csv')).write_text(raw)
    pages=list(csv.reader(raw.splitlines()))
    return name,[mode,rows,variant,database.stat().st_size,sum(int(row[2]) for row in pages),sum(int(row[3]) for row in pages)]

summary=[]
failures=[]
# Each child owns a distinct document and VFS state. Only this parent writes
# the shared summary; elapsed timings are diagnostics, not comparison results.
work=[(mode,rows,variant) for mode,rows in cases for variant in ('baseline','current','prototype')]
with ThreadPoolExecutor(max_workers=a.jobs) as pool:
    futures=[pool.submit(run_case,case) for case in work]
    for case,future in zip(work,futures):
        try:
            name,row=future.result()
        except Exception as error:
            failures.append({'case':case,'error':str(error)})
            print(str(error),flush=True)
            continue
        summary.append(row)
        with (a.evidence/'final-summary.csv').open('w',newline='') as out:
            writer=csv.writer(out)
            writer.writerow(['workload','rows_per_source','variant','file_bytes','btree_allocated_bytes','btree_payload_bytes'])
            writer.writerows(summary)
        print(name+' completed',flush=True)
if failures:
    (a.evidence/'failures.json').write_text(json.dumps(failures,indent=2)+'\n')
    raise RuntimeError(f'{len(failures)} measurement runs failed; no complete result is claimed.')
(a.evidence/'complete.json').write_text(json.dumps({'completed_runs':len(summary),'status':'all requested runs completed; no storage threshold applied'},indent=2)+'\n')
