#!/usr/bin/env python3
"""The manual capture runner inventories pictures and refuses a partial install."""

import importlib.util
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    "capture_manual_images", ROOT / "tools" / "capture_manual_images.py"
)
capture = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(capture)


def topic(name: str, image: str) -> str:
    return f"---\nid: {name}\ntitle: {name}\norder: 1\n---\n\n# {name}\n\n![{name}](images/{image})\n"


class CaptureManualImageTests(unittest.TestCase):
    def test_manual_links_match_the_picture_files(self):
        manual = ROOT / "manual"
        linked = capture.linked_images(manual)
        stored = sorted(path.name for path in (manual / "images").glob("*.png"))
        self.assertEqual(stored, sorted(linked))
        self.assertEqual(len(linked), len(set(linked)))

    def test_linked_images_keep_first_seen_order_and_reject_other_targets(self):
        with tempfile.TemporaryDirectory() as directory:
            manual = Path(directory)
            (manual / "b.md").write_text(topic("b", "second.png"), encoding="utf-8")
            (manual / "a.md").write_text(
                topic("a", "first.png") + "\nSee ![again](images/second.png).\n",
                encoding="utf-8",
            )
            self.assertEqual(capture.linked_images(manual), ["first.png", "second.png"])
            (manual / "c.md").write_text("![Outside](../secret.png)\n", encoding="utf-8")
            with self.assertRaises(capture.CaptureError):
                capture.linked_images(manual)

    def test_walkthrough_list_must_match_the_test_binary(self):
        available = [name + "()" for name in capture.WALKTHROUGHS]
        self.assertEqual(
            capture.walkthroughs_to_run(capture.listed_functions("\n".join(available) + "\n")),
            list(capture.WALKTHROUGHS),
        )
        with self.assertRaises(capture.CaptureError):
            capture.walkthroughs_to_run(capture.listed_functions("createOpenChapterWalkthrough()\n"))
        with self.assertRaises(capture.CaptureError):
            capture.walkthroughs_to_run(
                capture.listed_functions("\n".join(available) + "\nextraChapterWalkthrough()\n")
            )

    def test_assess_reports_missing_and_unexpected_pictures(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            (output / "first.png").write_bytes(b"one")
            (output / "extra.png").write_bytes(b"no")
            missing, unexpected = capture.assess(["first.png", "second.png"], output)
            self.assertEqual(missing, ["second.png"])
            self.assertEqual(unexpected, ["extra.png"])
            with self.assertRaises(capture.CaptureError):
                capture.install_images(["first.png", "second.png"], output, output / "installed")

    def test_install_copies_the_linked_pictures(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "staging"
            destination = root / "images"
            output.mkdir()
            (output / "first.png").write_bytes(b"picture")
            (output / "notes.txt").write_text("log", encoding="utf-8")
            capture.install_images(["first.png"], output, destination)
            self.assertEqual((destination / "first.png").read_bytes(), b"picture")
            self.assertFalse((destination / "notes.txt").exists())

    def test_staging_directory_cannot_be_the_manual(self):
        with self.assertRaises(capture.CaptureError):
            capture.capture_directory(ROOT / "manual" / "images")
        with tempfile.TemporaryDirectory() as directory:
            staging = capture.capture_directory(Path(directory) / "shots")
            self.assertTrue(staging.is_dir())

    def test_newer_interface_source_is_reported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            binary = root / "spentit_help_ui_tests"
            binary.write_bytes(b"binary")
            ui = root / "src" / "ui"
            ui.mkdir(parents=True)
            qml = ui / "ImportMain.qml"
            qml.write_text("Item {}", encoding="utf-8")
            tests = root / "tests" / "ui"
            tests.mkdir(parents=True)
            (tests / "help_ui_tests.cpp").write_text("void unused() {}\n", encoding="utf-8")
            # The binary is newest, so nothing is stale.
            binary.touch()
            self.assertEqual(capture.newer_sources(binary, root), [])
            qml.touch()
            self.assertEqual(capture.newer_sources(binary, root), [qml])


if __name__ == "__main__":
    unittest.main()
