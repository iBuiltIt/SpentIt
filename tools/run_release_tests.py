#!/usr/bin/env python3
"""Run the app Release CTest inventory and record which cases finished."""
import argparse
import collections
import json
from pathlib import Path
import subprocess
import sys
import time
import xml.etree.ElementTree as ET


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build/import-ui-rules-release"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--jobs", type=int, default=4)
    selection = parser.add_mutually_exclusive_group()
    selection.add_argument("--label", choices=("core", "durability", "native"))
    selection.add_argument("--exclude-native", action="store_true",
                           help="Run the complete offscreen and storage suite without a desktop session")
    parser.add_argument("--verbose", action="store_true", help="Stream individual test progress")
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs requires a positive number")

    args.output.mkdir(parents=True, exist_ok=False)
    ctest = ["ctest", "--test-dir", str(args.build.resolve())]
    if args.label:
        ctest += ["-L", f"^{args.label}$"]
    if args.exclude_native:
        ctest += ["-LE", "^native$"]
    inventory = subprocess.check_output([*ctest, "--show-only=json-v1"], text=True)
    (args.output / "inventory.json").write_text(inventory, encoding="utf-8")
    planned = [test["name"] for test in json.loads(inventory)["tests"]]
    if not planned:
        parser.error("CTest selected no tests")

    junit = (args.output / "results.xml").resolve()
    log = (args.output / "ctest.log").resolve()
    started = time.monotonic()
    status = None
    try:
        status = subprocess.call([
            *ctest, "-j", str(args.jobs), "--output-on-failure",
            "--output-log", str(log), "--output-junit", str(junit),
            *(["--verbose"] if args.verbose else []),
        ])
    finally:
        try:
            cases = list(ET.parse(junit).iter("testcase"))
        except (ET.ParseError, FileNotFoundError):
            cases = []
        results = {}
        for case in cases:
            if case.find("failure") is not None:
                result = "failed"
            elif case.find("skipped") is not None or case.get("status") == "notrun":
                result = "skipped"
            else:
                result = "passed"
            results[case.attrib["name"]] = result

        complete = collections.Counter(planned) == collections.Counter(
            case.attrib["name"] for case in cases
        ) and "skipped" not in results.values()
        passed = complete and status == 0 and all(result == "passed" for result in results.values())
        (args.output / "manifest.json").write_text(json.dumps({
            "complete": complete,
            "passed": passed,
            "ctest_exit": status,
            "elapsed_seconds": round(time.monotonic() - started, 2),
            "jobs": args.jobs,
            "label": args.label,
            "exclude_native": args.exclude_native,
            "planned": planned,
            "results": results,
            "missing": sorted(set(planned) - results.keys()),
        }, indent=2) + "\n", encoding="utf-8")
        print(f"Run manifest: {args.output / 'manifest.json'}", flush=True)
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
