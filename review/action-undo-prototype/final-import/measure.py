#!/usr/bin/env python3
"""Run identical session workload CLIs; preserve raw evidence and assess size only.
Does not certify semantic coverage or transient inverse/pin peak requirements.
"""
import argparse,csv,json,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--baseline',type=Path,required=True)
p.add_argument('--candidate',type=Path,required=True)
p.add_argument('--dbstat',type=Path,required=True)
p.add_argument('--output',type=Path,required=True)
p.add_argument('--rows',type=int,default=1000)
p.add_argument('--modes',nargs='+',choices=['import','edits','undo','linked'],default=['import','edits','undo','linked'])
a=p.parse_args()
if a.output.exists():p.error('Output directory must not exist; preserving earlier evidence')
a.output.mkdir(parents=True)
results={}
for mode in a.modes:
 for variant,exe in [('baseline',a.baseline),('candidate',a.candidate)]:
  root=a.output/(variant+'-'+mode)
  args=[str(exe.resolve()),str(root.resolve()),mode,str(a.rows)]
  with (a.output/(variant+'-'+mode+'.txt')).open('w') as log:
   subprocess.run(args,stdout=log,stderr=subprocess.STDOUT,check=True)
  text=(a.output/(variant+'-'+mode+'.txt')).read_text()
  if re.search(r'\bFAIL(?:ED)?\b',text):raise RuntimeError('Failure marker in '+variant+' '+mode)
  checkpoints={}
  for line in text.splitlines():
   if ',file=' not in line:continue
   parts=line.split(',');checkpoints[parts[0]]={k:int(v) for k,v in (x.split('=',1) for x in parts[1:])}
  if not checkpoints:raise RuntimeError('Missing measurements')
  db=root/(mode+'.spentit')
  raw=subprocess.check_output([str(a.dbstat.resolve()),str(db.resolve())],text=True)
  (a.output/(variant+'-'+mode+'-pages.csv')).write_text(raw)
  payload=sum(int(row[3]) for row in csv.reader(raw.splitlines()))
  results[(mode,variant)]={'checkpoints':checkpoints,'payload':payload}
  print(variant,mode,'completed',flush=True)
failures=[];summary=[]
for mode in a.modes:
 b=results[(mode,'baseline')];c=results[(mode,'candidate')]
 if b['checkpoints'].keys()!=c['checkpoints'].keys():raise RuntimeError('Checkpoint mismatch '+mode)
 for point in b['checkpoints']:
  for metric in ['file','live_page_bytes']:
   old=b['checkpoints'][point][metric];new=c['checkpoints'][point][metric]
   summary.append([mode,point,metric,old,new,new-old])
   if new>old:failures.append([mode,point,metric,new-old])
 summary.append([mode,'final','btree_payload_bytes',b['payload'],c['payload'],c['payload']-b['payload']])
 if c['payload']>b['payload']:failures.append([mode,'final','btree_payload_bytes',c['payload']-b['payload']])
with (a.output/'summary.csv').open('w',newline='') as f:
 w=csv.writer(f);w.writerow(['workload','checkpoint','metric','baseline','candidate','delta']);w.writerows(summary)
(a.output/'size-result.json').write_text(json.dumps({'size_excesses':failures,'limits':'Size comparison only; no complete Undo, recovery, or transient peak acceptance claim.'},indent=2)+'\n')
print('Size excesses:',len(failures))
raise SystemExit(2 if failures else 0)
