#!/usr/bin/env python3
"""Summarize completed common-driver runs; no acceptance threshold is inferred."""
import argparse
import csv
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('evidence',type=Path)
a=p.parse_args()
if not (a.evidence/'complete.json').exists():p.error('Run is incomplete; refusing a complete comparison.')
with (a.evidence/'final-summary.csv').open() as f: finals=list(csv.DictReader(f))
rows=[]
for final in finals:
    mode=final['workload'];count=int(final['rows_per_source']);variant=final['variant']
    log=a.evidence/f'{mode}-{count}-{variant}.txt'
    peaks={'vfs_cumulative_peak':0,'peak_inverse_codec_bytes':0,'peak_pinned_only_codec_bytes':0}
    phases=0
    for line in log.read_text().splitlines():
        if not line.startswith(('phase,','logical,')):continue
        if line.startswith('phase,'):phases+=1
        fields=dict(field.split('=',1) for field in line.split(',')[3:])
        for key in peaks:
            if key in fields:peaks[key]=max(peaks[key],int(fields[key]))
    if not phases or not peaks['vfs_cumulative_peak']:raise RuntimeError('Missing VFS measurements in '+str(log))
    rows.append({**final,**peaks})
with (a.evidence/'cost-summary.csv').open('w',newline='') as f:
    writer=csv.DictWriter(f,fieldnames=list(rows[0]));writer.writeheader();writer.writerows(rows)
index={(r['workload'],r['rows_per_source'],r['variant']):r for r in rows}
comparisons=[]
for r in rows:
    if r['variant']!='prototype':continue
    for variant in ('baseline','current'):
        base=index[(r['workload'],r['rows_per_source'],variant)]
        for metric in ('file_bytes','btree_payload_bytes','vfs_cumulative_peak'):
            old=int(base[metric]);new=int(r[metric])
            comparisons.append([r['workload'],r['rows_per_source'],variant,metric,old,new,new-old,f'{100*(new-old)/old:.4f}'])
with (a.evidence/'comparisons.csv').open('w',newline='') as f:
    w=csv.writer(f);w.writerow(['workload','rows_per_source','comparator','metric','comparator_bytes','prototype_bytes','delta_bytes','delta_percent']);w.writerows(comparisons)
print('Wrote cost-summary.csv and comparisons.csv. VFS peaks are simultaneous logical file lengths; inverse/pinned bytes are codec sizes, not allocated pages.')
