"""Sample synthetic storage workloads; no production database inputs."""
import argparse
import json
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("mode", choices=["import", "edits", "undo", "linked", "accumulated"])
parser.add_argument("rows", type=int)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
if any(args.output.iterdir()):
    parser.error("Use a fresh empty output directory")
started = time.monotonic()
peak_journal = peak_combined = 0
with (args.output / "driver.log").open("w") as log:
    process = subprocess.Popen(
        [str(args.binary.resolve()), str(args.output.resolve()), args.mode, str(args.rows)],
        stdout=log, stderr=subprocess.STDOUT,
    )
    while process.poll() is None:
        journal = combined = 0
        for path in args.output.iterdir():
            try:
                size = path.stat().st_size
            except FileNotFoundError:
                continue
            if path.name.endswith(("-journal", "-wal", "-shm")):
                journal += size
                combined += size
            elif path.name.endswith(".spentit"):
                combined += size
        peak_journal = max(peak_journal, journal)
        peak_combined = max(peak_combined, combined)
        time.sleep(0.002)
result = {
    "mode": args.mode, "rows": args.rows,
    "seconds": time.monotonic() - started,
    "sampled_peak_journal_bytes": peak_journal,
    "sampled_peak_database_and_journal_bytes": peak_combined,
    "exit": process.returncode,
    "output": (args.output / "driver.log").read_text(),
}
(args.output / "measurement.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
raise SystemExit(process.returncode)
