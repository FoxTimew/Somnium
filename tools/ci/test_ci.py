#!/usr/bin/env python3
"""Regression checks for destructive retention boundaries and CI trust guards.
These tests run without network, GitHub or a graphics device.
"""
import json
import os
from pathlib import Path
import tempfile
import time
import unittest

import run


class InfrastructureTests(unittest.TestCase):
    def test_retention_preserves_current_unowned_recent_and_cache(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            current = root / "runs/current"
            for name in ["old", "current", "recent", "foreign"]:
                path = root / "runs" / name
                path.mkdir(parents=True)
                marker = path / "run.json"
                marker.write_text(json.dumps({"owner": "other" if name == "foreign" else "somnium-ci"}))
                if name != "recent":
                    os.utime(marker, (0, 0))
            cache = root / "cache/keep"
            cache.mkdir(parents=True)
            run.clean_runs(root, current, 14)
            self.assertFalse((root / "runs/old").exists())
            for path in [current, root / "runs/recent", root / "runs/foreign", cache]:
                self.assertTrue(path.exists())

    @unittest.skipIf(os.name == "nt", "Creating symlinks requires a privilege on Windows")
    def test_retention_does_not_follow_symlinks(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            outside = root / "outside"
            outside.mkdir()
            (outside / "run.json").write_text('{"owner":"somnium-ci"}')
            os.utime(outside / "run.json", (0, 0))
            (root / "runs").mkdir()
            (root / "runs/escape").symlink_to(outside, target_is_directory=True)
            run.clean_runs(root, root / "runs/current", 14)
            self.assertTrue((outside / "run.json").exists())

    def test_workflow_guards_and_storage_contract(self):
        files = sorted((run.ROOT / ".github/workflows").glob("*.yml"))
        self.assertEqual([p.name for p in files], ["ci.yml", "macos-manual.yml"])
        guard = "if: github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name == github.repository"
        for path in files:
            text = path.read_text(encoding="utf-8")
            self.assertNotIn("pull_request_target", text)
            for forbidden in ["actions/cache@", "actions/upload-artifact@", "actions/download-artifact@", "contents: write"]:
                self.assertNotIn(forbidden, text)
            self.assertIn("cancel-in-progress: true", text)
            self.assertIn("persist-credentials: false", text)
            self.assertEqual(text.count("runs-on:"), text.count(guard))
            self.assertEqual(text.count("runs-on:"), text.count("timeout-minutes:"))
        optional = files[1].read_text(encoding="utf-8")
        self.assertIn("  workflow_dispatch:", optional)
        self.assertNotIn("  push:", optional)
        self.assertNotIn("  pull_request:", optional)


if __name__ == "__main__":
    unittest.main()
