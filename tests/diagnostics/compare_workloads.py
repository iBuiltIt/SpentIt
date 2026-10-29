"""Serial synthetic comparisons. Output is written only to an explicit destination.

Do not run builds or other benchmarks concurrently. These are warm-reopen,
offscreen observations, not OS disk-cold or native presentation measurements.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument("binary", type=Path)
parser.add_argument("output_directory", type=Path)
parser.add_argument("--repetitions", type=int, default=3, choices=range(1, 11))
args = parser.parse_args()
binary = args.binary.resolve()
args.output_directory.mkdir(parents=True, exist_ok=True)
binary_hash = hashlib.sha256(binary.read_bytes()).hexdigest()
results = []
for count, mode in [(18972, "--groups"), (19000, None)]:
    for iteration in range(args.repetitions):
        for enabled in [False, True]:
            env = os.environ.copy()
            env["QT_QPA_PLATFORM"] = "offscreen"
            env.pop("SPENTIT_BENCHMARK_DIAGNOSTICS", None)
            if enabled:
                env["SPENTIT_BENCHMARK_DIAGNOSTICS"] = "1"
            command = [str(binary), str(count)] + ([mode] if mode else [])
            started = time.monotonic()
            run = subprocess.run(command, env=env, capture_output=True, text=True, timeout=600)
            name = f"{count}-{iteration}-{int(enabled)}"
            (args.output_directory / f"{name}.log").write_text(run.stdout + run.stderr)
            phases = {}
            for line in run.stdout.splitlines():
                fields = dict(word.split("=", 1) for word in line.split() if "=" in word)
                if "phase" in fields and "ms" in fields:
                    phases[fields["phase"]] = float(fields["ms"])
            assert hashlib.sha256(binary.read_bytes()).hexdigest() == binary_hash, "Binary changed during comparison"
            results.append(dict(binary_sha256=binary_hash, rows=count, iteration=iteration,
                                enabled=enabled, seconds=time.monotonic()-started,
                                returncode=run.returncode, phases=phases))
            (args.output_directory / "comparisons.json").write_text(json.dumps(results, indent=2) + "\n")
            print(name, run.returncode, round(results[-1]["seconds"], 2), flush=True)
            if run.returncode:
                raise SystemExit("Workload failed; partial results and log retained")
