#!/usr/bin/env python3
"""Regression checks for destructive retention boundaries and CI trust guards.
These tests run without network, GitHub or a graphics device.
"""
import json
import hashlib
import os
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch
import urllib.error

import run
import provision


class DownloadTests(unittest.TestCase):
    def test_lunarg_403_retries_and_reuses_verified_cache(self):
        payload = b"verified SDK archive"
        url = "https://sdk.lunarg.com/sdk/download/1.4/linux/sdk.tar.xz"
        pin = {"url": url, "sha256": hashlib.sha256(payload).hexdigest()}

        def curl(command, **kwargs):
            self.assertEqual(command[-1], url + "?Human=true")
            self.assertTrue(kwargs["check"])
            self.assertIn("--fail", command)
            Path(command[command.index("--output") + 1]).write_bytes(payload)

        with tempfile.TemporaryDirectory() as folder:
            error = urllib.error.HTTPError(url, 403, "Forbidden", {}, None)
            with patch.object(provision.urllib.request, "urlretrieve", side_effect=error) as request, \
                    patch.object(provision.shutil, "which", return_value="curl"), \
                    patch.object(provision.subprocess, "run", side_effect=curl) as retry:
                result = provision.download(pin, Path(folder))
                self.assertEqual(result.read_bytes(), payload)
                self.assertEqual(provision.download(pin, Path(folder)), result)
                self.assertEqual(request.call_count, 1)
                self.assertEqual(retry.call_count, 1)

    def test_other_hosts_and_http_errors_are_not_retried(self):
        for url, status in [("https://example.com/sdk/download/sdk.zip", 403),
                            ("https://sdk.lunarg.com/sdk/download/sdk.zip", 404)]:
            with self.subTest(url=url, status=status), tempfile.TemporaryDirectory() as folder:
                error = urllib.error.HTTPError(url, status, "Rejected", {}, None)
                with patch.object(provision.urllib.request, "urlretrieve", side_effect=error) as request:
                    with self.assertRaises(urllib.error.HTTPError):
                        provision.download({"url": url, "sha256": "unused"}, Path(folder))
                    self.assertEqual(request.call_count, 1)

    def test_corrupt_download_does_not_enter_cache(self):
        pin = {"url": "https://example.com/sdk.zip", "sha256": hashlib.sha256(b"expected").hexdigest()}

        def retrieve(address, destination):
            Path(destination).write_bytes(b"corrupt")

        with tempfile.TemporaryDirectory() as folder:
            with patch.object(provision.urllib.request, "urlretrieve", side_effect=retrieve):
                with self.assertRaisesRegex(RuntimeError, "Checksum mismatch"):
                    provision.download(pin, Path(folder))
            self.assertFalse((Path(folder) / (pin["sha256"] + "-sdk.zip")).exists())


class InfrastructureTests(unittest.TestCase):
    def assert_current_survives_alias(self, root, current):
        for name in ["current", "old"]:
            directory = root / "runs" / name
            directory.mkdir(parents=True)
            marker = directory / "run.json"
            marker.write_text('{"owner":"somnium-ci"}')
            os.utime(marker, (0, 0))
        run.clean_runs(root, current, 14)
        self.assertTrue((root / "runs/current/run.json").is_file())
        self.assertFalse((root / "runs/old").exists())

    def test_retention_normalizes_current_path(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            self.assert_current_survives_alias(root, root / "runs/../runs/current")

    @unittest.skipIf(os.name == "nt", "Creating symlinks requires a privilege on Windows")
    def test_retention_preserves_current_through_temporary_directory_alias(self):
        # Model macOS /var -> /private/var without depending on the host TMPDIR.
        with tempfile.TemporaryDirectory() as folder:
            physical = Path(folder).resolve() / "physical"
            physical.mkdir()
            alias = Path(folder) / "temporary-alias"
            alias.symlink_to(physical, target_is_directory=True)
            self.assert_current_survives_alias(alias, alias / "runs/current")

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
        guard = "github.event_name != 'pull_request' || github.event.pull_request.head.repo.full_name == github.repository"
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

    def test_windows_pipeline_never_injects_desktop_input(self):
        # Run orchestration with inert build operations; any accidental desktop
        # capture in the required Windows pipeline is an immediate test failure.
        runner = object.__new__(run.Runner)
        for name in ["doctor", "format", "cache_server", "compiler_cache", "shader_toolchain",
                     "configure", "build", "test", "gpu", "call", "summary"]:
            setattr(runner, name, lambda *args, **kwargs: None)
        runner.captures = lambda *_: self.fail("Required Windows pipeline injected desktop input")
        with patch.object(run, "SYSTEM", "Windows"):
            runner.pipeline()
        workflow = (run.ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        required, manual = workflow.split("  windows-presentation:", 1)
        self.assertNotIn("windows.ps1 captures", required)
        self.assertIn("github.event_name == 'workflow_dispatch' && inputs.presentation", manual)
        self.assertIn("needs: windows", manual)
        self.assertIn("windows.ps1 captures --backend direct3d12", manual)


if __name__ == "__main__":
    unittest.main()
