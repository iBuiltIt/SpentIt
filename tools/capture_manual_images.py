#!/usr/bin/env python3
"""Recapture the pictures linked from the user manual.

Each chapter walkthrough in spentit_help_ui_tests creates a temporary
household file, opens the real window, and saves a picture when
SPENTIT_GUIDE_CAPTURE is set. This runs those walkthroughs into a staging
directory and checks the result against the image links in manual/.

Pictures replace manual/images only with --install, and only after every
walkthrough has passed and the produced names match the links. The test
binary embeds the interface, so this refuses to run when a src/ui QML file
or tests/ui/help_ui_tests.cpp is newer than that binary.

Rebuild first:
  cmake --build build/import-ui-rules-release --target spentit_help_ui_tests -j 2
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BINARY = ROOT / "build" / "import-ui-rules-release" / "src" / "ui" / "spentit_help_ui_tests"
WALKTHROUGHS = (
    "createOpenChapterWalkthrough",
    "accountsChapterWalkthrough",
    "importChapterWalkthrough",
    "transactionsChapterWalkthrough",
    "reviewChapterWalkthrough",
    "editingChapterWalkthrough",
    "categoriesChapterWalkthrough",
    "rulesChapterWalkthrough",
    "acceptanceChapterWalkthrough",
    "reportsChapterWalkthrough",
    "recoveryChapterWalkthrough",
)


class CaptureError(Exception):
    pass


IMAGE_RE = re.compile(r"!\[([^\]]*)\]\(([^)\s]+)\)")


def fail(message: str) -> None:
    raise CaptureError(message)


def linked_images(manual: Path) -> list[str]:
    """Return picture filenames linked from the manual, in first-seen order."""
    names: list[str] = []
    seen: set[str] = set()
    if not manual.is_dir():
        fail(f"Manual directory is missing: {manual}")
    for path in sorted(manual.glob("*.md")):
        text = path.read_text(encoding="utf-8")
        for _alt, target in IMAGE_RE.findall(text):
            prefix = "images/"
            name = target[len(prefix) :]
            if not target.startswith(prefix) or not name.endswith(".png") or "/" in name:
                fail(f"{path.name} links {target}, which is not images/<name>.png")
            if name not in seen:
                seen.add(name)
                names.append(name)
    if not names:
        fail(f"{manual} has no manual pictures")
    return names


def listed_functions(listing: str) -> list[str]:
    names = []
    for line in listing.splitlines():
        line = line.strip()
        if line.endswith("()"):
            names.append(line[:-2])
    return names


def walkthroughs_to_run(available: list[str]) -> list[str]:
    found = set(available)
    missing = [name for name in WALKTHROUGHS if name not in found]
    if missing:
        fail("The help UI test does not provide: " + ", ".join(missing))
    extra = sorted(name for name in found if name.endswith("ChapterWalkthrough") and name not in WALKTHROUGHS)
    if extra:
        fail("Add these chapter walkthroughs to the capture list: " + ", ".join(extra))
    return list(WALKTHROUGHS)


def newer_sources(binary: Path, root: Path = ROOT) -> list[Path]:
    """QML and the walkthrough source that the test binary embeds or runs."""
    stamp = binary.stat().st_mtime
    stale: list[Path] = []
    ui = root / "src" / "ui"
    for path in sorted(ui.rglob("*.qml")):
        if path.stat().st_mtime > stamp:
            stale.append(path)
    walkthrough = root / "tests" / "ui" / "help_ui_tests.cpp"
    if walkthrough.stat().st_mtime > stamp:
        stale.append(walkthrough)
    return stale


def assess(expected: list[str], output: Path) -> tuple[list[str], list[str]]:
    produced = {path.name for path in output.glob("*.png")}
    missing = [name for name in expected if name not in produced]
    unexpected = sorted(name for name in produced if name not in set(expected))
    return missing, unexpected


def install_images(expected: list[str], output: Path, destination: Path) -> None:
    missing, unexpected = assess(expected, output)
    if missing or unexpected:
        fail("Refusing to install an incomplete capture")
    destination.mkdir(parents=True, exist_ok=True)
    for name in expected:
        shutil.copy2(output / name, destination / name)


def capture_directory(path: Path | None) -> Path:
    if path is None:
        return Path(tempfile.mkdtemp(prefix="spentit-manual-images."))
    manual = ROOT / "manual"
    resolved = path.resolve()
    if resolved == manual.resolve() or manual.resolve() in resolved.parents:
        fail(f"Capture into a staging directory, not {resolved}")
    resolved.mkdir(parents=True, exist_ok=True)
    return resolved


def run_walkthrough(binary: Path, name: str, output: Path, log: Path) -> int:
    env = os.environ.copy()
    env.pop("QT_QPA_PLATFORM", None)
    env["SPENTIT_GUIDE_CAPTURE"] = str(output)
    with log.open("w", encoding="utf-8") as handle:
        completed = subprocess.run(
            [str(binary), name],
            cwd=ROOT,
            env=env,
            stdout=handle,
            stderr=subprocess.STDOUT,
            timeout=240,
        )
    return completed.returncode


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="Recapture the SpentIt manual pictures")
    parser.add_argument("--binary", type=Path, default=DEFAULT_BINARY, help="spentit_help_ui_tests executable")
    parser.add_argument("--manual", type=Path, default=ROOT / "manual", help="Manual directory whose image links are required")
    parser.add_argument("--output", type=Path, help="Staging directory. The default is a new temporary directory.")
    parser.add_argument("--install", action="store_true", help="After a complete capture, copy the pictures into manual/images")
    args = parser.parse_args(argv)
    try:
        binary = args.binary.resolve()
        if not binary.is_file():
            fail(f"Help UI test binary is missing: {binary}")
        stale = newer_sources(binary)
        if stale:
            shown = "\n".join(f"  {path.relative_to(ROOT)}" for path in stale[:20])
            fail(
                "The help UI test binary is older than the interface it would capture.\n"
                + shown
                + "\nRebuild it, then run this again:\n"
                "  cmake --build build/import-ui-rules-release --target spentit_help_ui_tests -j 2"
            )
        expected = linked_images(args.manual)
        listing = subprocess.run(
            [str(binary), "-functions"],
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
        )
        if listing.returncode != 0:
            fail(listing.stderr.strip() or "Could not list help UI tests")
        names = walkthroughs_to_run(listed_functions(listing.stdout))
        output = capture_directory(args.output)
        for old in output.glob("*.png"):
            old.unlink()
        logs = output / "logs"
        logs.mkdir(exist_ok=True)
        print(f"Capturing {len(expected)} pictures into {output}")
        failed: list[str] = []
        for name in names:
            print(f"  {name}", flush=True)
            code = run_walkthrough(binary, name, output, logs / f"{name}.log")
            if code != 0:
                failed.append(name)
                print(f"    failed ({code}). Log: {logs / f'{name}.log'}", file=sys.stderr)
        missing, unexpected = assess(expected, output)
        if failed or missing or unexpected:
            if missing:
                print("Missing pictures:\n  " + "\n  ".join(missing), file=sys.stderr)
            if unexpected:
                print("Unexpected pictures:\n  " + "\n  ".join(unexpected), file=sys.stderr)
            fail(f"Capture incomplete. Pictures remain in {output}")
        print(f"Captured {len(expected)} pictures in {output}")
        if args.install:
            destination = args.manual / "images"
            install_images(expected, output, destination)
            print(f"Installed {len(expected)} pictures into {destination}")
    except CaptureError as error:
        print(error, file=sys.stderr)
        return 1
    except subprocess.TimeoutExpired as error:
        print(f"{error.cmd[-1] if error.cmd else 'A walkthrough'} exceeded 240 seconds", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
