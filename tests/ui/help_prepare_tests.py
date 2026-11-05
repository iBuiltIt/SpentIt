#!/usr/bin/env python3
"""The Help preparer accepts the sample bundle and rejects links that leave it."""

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("prepare_help", ROOT / "tools" / "prepare_help.py")
prepare_help = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(prepare_help)


def topic(text: str) -> str:
    return "---\nid: sample-extra\ntitle: Extra\norder: 9\n---\n\n# Extra\n\n" + text + "\n"


class PrepareHelpTests(unittest.TestCase):
    def test_sample_bundle_rewrites_internal_links(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory)
            prepare_help.build(ROOT / "manual", output)
            manifest = (output / "manifest.json").read_text(encoding="utf-8")
            self.assertIn("sample-start", manifest)
            self.assertIn("help:sample-details#wide-table", manifest)
            self.assertIn("help-image:sample-mark.png", manifest)
            self.assertNotIn("https://", manifest)
            self.assertTrue((output / "images" / "sample-mark.png").is_file())

    def rejected(self, body: str):
        with tempfile.TemporaryDirectory() as directory:
            manual = Path(directory) / "manual"
            shutil.copytree(ROOT / "manual", manual)
            (manual / "sample" / "extra.md").write_text(topic(body), encoding="utf-8")
            with self.assertRaises(prepare_help.PrepareError):
                prepare_help.build(manual, Path(directory) / "out")

    def test_external_url_is_rejected(self):
        self.rejected("See [the web](https://example.com).")

    def test_file_url_is_rejected(self):
        self.rejected("See [a file](file:///tmp/note.txt).")

    def test_absolute_path_is_rejected(self):
        self.rejected("See [a path](/tmp/note.md).")

    def test_parent_escape_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manual = root / "manual"
            shutil.copytree(ROOT / "manual", manual)
            outside = root / "outside.md"
            outside.write_text("# Outside\n", encoding="utf-8")
            (manual / "sample" / "extra.md").write_text(topic("See [outside](../../outside.md)."), encoding="utf-8")
            with self.assertRaises(prepare_help.PrepareError):
                prepare_help.build(manual, root / "out")

    def test_unknown_section_is_rejected(self):
        self.rejected("See [missing](details.md#not-a-section).")


if __name__ == "__main__":
    unittest.main()
