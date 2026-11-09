#!/usr/bin/env python3
"""Assemble complete comparisons from the reviewed equivalent-read components.
Never includes partial groups. Raw text is copied byte-for-byte; databases are not.
"""
import argparse,csv,hashlib,json,shutil
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--first',type=Path,required=True)
p.add_argument('--continuation',type=Path,required=True)
p.add_argument('--mixed',type=Path,help='Separate complete mixed-workload component using identical continuation binaries')
p.add_argument('--output',type=Path,required=True)
a=p.parse_args()
if a.output.exists():p.error('Output must be fresh.')
if not a.mixed and not (a.continuation/'complete.json').exists():p.error('Continuation is not complete.')
if a.mixed and not (a.mixed/'complete.json').exists():p.error('Separate mixed component is not complete.')
first_groups={(m,1000) for m in ('import','edits','undo','linked','mixed')}|{('import',10000)}
next_groups={(m,10000) for m in (('edits','undo','linked') if a.mixed else ('edits','undo','linked','mixed'))}
components=[(a.first,first_groups),(a.continuation,next_groups)]
if a.mixed:components.append((a.mixed,{('mixed',10000)}))
records=[];provenance=[];artifacts=[];manifests=[]
for source,groups in components:
    manifest=json.loads((source/'manifest.json').read_text());manifests.append(manifest)
    with (source/'final-summary.csv').open() as f:
        selected=[r for r in csv.DictReader(f) if (r['workload'],int(r['rows_per_source'])) in groups]
    actual={(r['workload'],int(r['rows_per_source']),r['variant']) for r in selected}
    expected={(m,n,v) for m,n in groups for v in ('baseline','current','prototype')}
    if actual!=expected or len(selected)!=len(expected):raise RuntimeError('Incomplete/duplicate component '+str(source))
    for row in selected:
        name=f"{row['workload']}-{row['rows_per_source']}-{row['variant']}"
        for suffix in ('.txt','-pages.csv'):
            file=source/(name+suffix)
            if not file.is_file():raise RuntimeError('Missing '+str(file))
            artifacts.append((file,name+suffix))
        records.append({**row,'component':source.name})
    provenance.append({'component':source.name,'manifest_sha256':hashlib.sha256((source/'manifest.json').read_bytes()).hexdigest(),'selected_cases':sorted(groups)})
for manifest in manifests[1:]:
    for key in ('driver_sha256','dbstat_sha256'):
        if manifests[0][key]!=manifest[key]:raise RuntimeError('Common input differs: '+key)
    for variant in ('baseline','current'):
        if manifests[0]['executables'][variant]!=manifest['executables'][variant]:raise RuntimeError('Comparator executable changed: '+variant)
if a.mixed and manifests[1]['executables']!=manifests[2]['executables']:raise RuntimeError('Mixed continuation executable changed')
a.output.mkdir(parents=True)
for source,name in artifacts:shutil.copyfile(source,a.output/name)
with (a.output/'final-summary.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(records[0]));writer.writeheader();writer.writerows(records)
(a.output/'component-provenance.json').write_text(json.dumps({'components':provenance,'limits':'Two independently reviewed equivalent prepared-read variants of the same storage representation. See query-optimization and allocation-query-optimization proofs; no timing comparison or full-feature storage guarantee.'},indent=2)+'\n')
(a.output/'complete.json').write_text(json.dumps({'completed_runs':len(records),'status':'Complete30-run matrix assembled only from complete comparison groups; no acceptance threshold applied.'},indent=2)+'\n')
print('Assembled',len(records),'completed runs.')
