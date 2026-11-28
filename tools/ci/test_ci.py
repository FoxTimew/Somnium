#!/usr/bin/env python3
"""Regression checks for destructive retention boundaries and CI trust guards.
These tests run without network, GitHub or a graphics device.
"""
import json
import hashlib
import os
import shutil
from pathlib import Path
import tempfile
import time
import unittest
from unittest.mock import patch
import urllib.error

import run
import provision
import storage


class StorageTests(unittest.TestCase):
    @staticmethod
    def build(root, name, age=0, owner="somnium-ci"):
        directory = root / "runs" / name
        build = directory / "build" / "debug"
        build.mkdir(parents=True)
        (directory / "run.json").write_text(json.dumps({"owner": owner}))
        stamp = time.time() - age * 86400
        os.utime(directory / "run.json", (stamp, stamp))
        (build / "object.o").write_bytes(b"x" * 1000)
        (build / "Testing").mkdir()
        (build / "Testing/LastTest.log").write_text("retained test journal")
        (build / "iteration-benchmark.json").write_text('{"samples":200}')
        (directory / "capture.png").write_bytes(b"retained image")
        return directory

    def test_age_purges_builds_but_preserves_current_worktrees_and_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            current = self.build(root, "current", age=10)
            old = self.build(root, "old", age=3)
            recent = self.build(root, "recent")
            foreign = self.build(root, "foreign", age=10, owner="other")
            misplaced = self.build(root, "worktree", age=10)
            (misplaced / "build/debug/.git").write_text("gitdir: user-owned")
            checkout = root / "checkouts/work/build"
            checkout.mkdir(parents=True)
            storage.clean_builds(root, current, summary=lambda _: None)
            self.assertFalse((old / "build").exists())
            self.assertEqual((old / "capture.png").read_bytes(), b"retained image")
            archives = list((old / "logs/retired-builds").glob("build-*/debug"))
            self.assertEqual(len(archives), 1)
            archive = archives[0]
            self.assertEqual((archive / "Testing/LastTest.log").read_text(), "retained test journal")
            self.assertTrue((archive / "iteration-benchmark.json").is_file())
            for path in [current / "build", recent / "build", foreign / "build", misplaced / "build", checkout]:
                self.assertTrue(path.exists())
            self.assertEqual(storage.clean_builds(root, current, summary=lambda _: None), 0)
            os.utime(misplaced / "run.json", (0, 0))
            run.clean_runs(root, current, 14)
            self.assertTrue((misplaced / "build/debug/.git").is_file())

    def test_quota_removes_oldest_even_within_evidence_retention(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            old = self.build(root, "first", age=1)
            new = self.build(root, "second")
            storage.clean_builds(root, root / "runs/current", quota=1500, summary=lambda _: None)
            self.assertFalse((old / "build").exists())
            self.assertTrue((new / "build").is_dir())

    def test_free_space_pressure_and_failure_name_directory_and_cleanup(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            old = self.build(root, "old")
            usage = shutil.disk_usage(root)._replace(free=0)
            with patch.object(storage.shutil, "disk_usage", return_value=usage):
                storage.clean_builds(root, root / "runs/current", minimum_free=1, summary=lambda _: None)
                self.assertFalse((old / "build").exists())
                with self.assertRaisesRegex(RuntimeError, "cleanup --root") as error:
                    storage.require_space(root / "build/debug", f'cleanup --root "{root}"')
                self.assertIn(str((root / "build/debug").resolve()), str(error.exception))
                self.assertIn("12 GiB", str(error.exception))

    def test_busy_run_is_never_purged(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            busy = self.build(root, "busy", age=4)
            with storage.exclusive(busy / "active.lock"):
                storage.clean_builds(root, root / "runs/current", quota=0, summary=lambda _: None)
            self.assertTrue((busy / "build").is_dir())

    def test_archival_failure_preserves_build_and_evidence(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            old = self.build(root, "old", age=4)
            with patch.object(storage.shutil, "copy2", side_effect=OSError("archive unavailable")):
                with self.assertRaisesRegex(OSError, "archive unavailable"):
                    storage.clean_builds(root, root / "runs/current", summary=lambda _: None)
            self.assertTrue((old / "build/debug/Testing/LastTest.log").is_file())

    def test_build_and_doctor_stop_before_any_process_on_low_space(self):
        runner = object.__new__(run.Runner)
        runner.build_dir = lambda preset: Path("ci/runs/current/build") / preset
        runner.check_space = lambda path: storage.require_space(path, "cleanup --root ci")
        with patch.object(storage.shutil, "disk_usage", return_value=shutil.disk_usage(Path.cwd())._replace(free=0)):
            with self.assertRaisesRegex(RuntimeError, "before build"):
                runner.build("debug")
            runner.run = Path("ci/runs/current")
            with self.assertRaisesRegex(RuntimeError, "before build"):
                runner.doctor()

    @unittest.skipIf(os.name == "nt", "Creating symlinks requires a privilege on Windows")
    def test_root_alias_is_normalized_but_build_links_never_escape(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder) / "physical"
            current = self.build(root, "current", age=4)
            alias = Path(folder) / "alias"
            alias.symlink_to(root, target_is_directory=True)
            outside = Path(folder) / "outside"
            outside.mkdir()
            (outside / "keep.log").write_text("untouched")
            old = self.build(root, "old", age=4)
            (old / "build/debug/nested").symlink_to(outside, target_is_directory=True)
            escaped = self.build(root, "escape", age=4)
            shutil.rmtree(escaped / "build")
            (escaped / "build").symlink_to(outside, target_is_directory=True)
            storage.clean_builds(alias, alias / "runs/current", quota=0, summary=lambda _: None)
            self.assertTrue((current / "build").is_dir())
            self.assertTrue((escaped / "build").is_symlink())
            self.assertEqual((outside / "keep.log").read_text(), "untouched")


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

    def test_windows_pipeline_requires_both_engine_captures(self):
        runner = object.__new__(run.Runner)
        for name in ["doctor", "format", "cache_server", "compiler_cache", "shader_toolchain", "hot_reload_spike",
                     "configure", "build", "test", "gpu", "call", "summary"]:
            setattr(runner, name, lambda *args, **kwargs: None)
        captures = []
        runner.captures = captures.append
        iteration = []
        runner.iteration_proof = iteration.append
        shaders = []
        runner.shader_reload_proof = shaders.append
        gameplay = []
        runner.gameplay_proof = gameplay.append
        presets = []
        runner.configure = presets.append
        runner.iteration_profiling = lambda: self.fail("Profiling iteration must be manual")
        with patch.object(run, "SYSTEM", "Windows"):
            runner.pipeline()
        self.assertEqual(captures, ["direct3d12", "vulkan"])
        self.assertEqual(iteration, ["direct3d12", "vulkan"])
        self.assertEqual(shaders, ["direct3d12", "vulkan"])
        self.assertEqual(gameplay, ["direct3d12", "vulkan"])
        self.assertIn("iteration", presets)
        self.assertNotIn("iteration-profiling", presets)
        workflow = (run.ROOT / ".github/workflows/ci.yml").read_text(encoding="utf-8")
        self.assertNotIn("windows-presentation:", workflow)


if __name__ == "__main__":
    unittest.main()
