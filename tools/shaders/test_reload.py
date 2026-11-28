# Exercises structural signatures and atomic program publication without a GPU.
# Real compiler and rendering proofs complement these malformed-publication checks.
import copy
import errno
import json
from pathlib import Path
import tempfile
import threading
import unittest
from concurrent.futures import ThreadPoolExecutor
from unittest.mock import patch

from publish import atomic_write, publish, read_publication, sha, sharing_violation
from reflection import canonical_bytes, digest, normalize
from watch import build, dependencies, fingerprint


class ReflectionTests(unittest.TestCase):
    def setUp(self):
        self.counts = {"samplers": 0, "uniform_buffers": 1, "storage_buffers": 0, "storage_textures": 0}
        self.layout = {"entryPoints": [{"name": "vsMain", "mode": "vert"}],
                       "types": {"_12": {"name": "Frame", "members": [
                           {"name": "position", "type": "vec4", "offset": 0},
                           {"name": "world", "type": "mat4", "offset": 16, "matrix_stride": 16}]}},
                       "ubos": [{"type": "_12", "name": "instance", "set": 1, "binding": 0, "block_size": 80}],
                       "inputs": [{"name": "position", "type": "vec3", "location": 0}]}

    def signature(self, value):
        return digest(normalize(value, stage="vertex", entrypoint="vsMain", counts=self.counts))

    def test_type_ids_names_and_unused_types_do_not_change_signature(self):
        changed = copy.deepcopy(self.layout)
        changed["types"]["_900"] = changed["types"].pop("_12")
        changed["types"]["_900"]["name"] = "OtherName"
        changed["types"]["_900"]["members"][0]["name"] = "renamed"
        changed["types"]["_777"] = {"name": "unused"}
        changed["ubos"][0]["type"] = "_900"
        self.assertEqual(self.signature(changed), self.signature(self.layout))

    def test_equal_resource_counts_do_not_hide_layout_changes(self):
        for field, value in [("type", "ivec4"), ("offset", 4), ("array", [2]), ("array_stride", 32)]:
            with self.subTest(field=field):
                changed = copy.deepcopy(self.layout)
                changed["types"]["_12"]["members"][0][field] = value
                self.assertNotEqual(self.signature(changed), self.signature(self.layout))
        for category, field, value in [("ubos", "binding", 1), ("ubos", "set", 0),
                                       ("inputs", "location", 1), ("inputs", "flat", True)]:
            changed = copy.deepcopy(self.layout)
            changed[category][0][field] = value
            self.assertNotEqual(self.signature(changed), self.signature(self.layout))

    def test_invalid_type_references_are_explicit_errors(self):
        for value in ["_missing", [], {}]:
            changed = copy.deepcopy(self.layout)
            changed["ubos"][0]["type"] = value
            with self.assertRaises(ValueError):
                self.signature(changed)


class PublicationTests(unittest.TestCase):
    def test_windows_open_and_replace_report_different_permission_error_shapes(self):
        crt = PermissionError(errno.EACCES, "CRT open")
        native = PermissionError(errno.EACCES, "Win32 replace")
        native.winerror = 32
        with patch("publish.os.name", "nt"):
            self.assertTrue(sharing_violation(crt))
            self.assertTrue(sharing_violation(native))
            self.assertFalse(sharing_violation(PermissionError(errno.EPERM, "unrelated")))
        with patch("publish.os.name", "posix"):
            self.assertFalse(sharing_violation(crt))

    def test_transient_read_denial_retries_without_rewriting_identical_publication(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "program.json"
            path.write_bytes(b"complete")
            stamp = path.stat().st_mtime_ns
            with patch("publish.sharing_violation", return_value=True), \
                    patch("publish.time.sleep"), \
                    patch.object(Path, "read_bytes", side_effect=[PermissionError(), b"complete"]):
                atomic_write(path, b"complete")
            self.assertEqual(path.read_bytes(), b"complete")
            self.assertEqual(path.stat().st_mtime_ns, stamp)
            with patch("publish.sharing_violation", return_value=True), \
                    patch("publish.time.monotonic", side_effect=[0, 3]), \
                    patch.object(Path, "read_bytes", side_effect=PermissionError()):
                with self.assertRaises(PermissionError):
                    atomic_write(path, b"replacement")
            self.assertEqual(path.read_bytes(), b"complete")
            self.assertEqual(list(path.parent.iterdir()), [path])
            with patch("publish.sharing_violation", return_value=False), \
                    patch.object(Path, "read_bytes", side_effect=PermissionError()):
                with self.assertRaises(PermissionError):
                    read_publication(path)

    def stage(self, directory, name, stage):
        files = {}
        for kind in ["spirv", "dxil", "msl", "reflection", "layout"]:
            path = directory / f"{name}.{kind}"
            data = f"{name}:{kind}".encode()
            path.write_bytes(data)
            files[kind] = {"file": path.name, "bytes": len(data), "sha256": sha(data)}
        manifest = directory / f"{name}.json"
        manifest.write_bytes(canonical_bytes({"version": 1, "stage": stage,
            "entrypoint": "vsMain" if stage == "vertex" else "psMain",
            "formats": {kind: files[kind] for kind in ["spirv", "dxil", "msl"]},
            "reflection": files["reflection"], "layout": files["layout"]}))
        return manifest

    def test_generations_remain_readable_and_noop_preserves_publication_timestamp(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stages = {"vertex": self.stage(root, "vertex", "vertex"),
                      "lambert": self.stage(root, "fragment", "fragment")}
            output = root / "installed/model.json"
            initial = publish("model", stages, output)
            stamp = output.stat().st_mtime_ns
            self.assertEqual(publish("model", stages, output), initial)
            self.assertEqual(output.stat().st_mtime_ns, stamp)
            stages["lambert"] = self.stage(root, "replacement", "fragment")
            newer = publish("model", stages, output)
            self.assertNotEqual(initial["generation"], newer["generation"])
            for relative, descriptor in initial["files"].items():
                self.assertEqual(sha((output.parent / relative).read_bytes()), descriptor["sha256"])

    def test_missing_corrupt_and_unsafe_artifacts_never_replace_last_good_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stages = {"vertex": self.stage(root, "vertex", "vertex"),
                      "lambert": self.stage(root, "fragment", "fragment")}
            output = root / "installed/model.json"
            publish("model", stages, output)
            original = output.read_bytes()
            (root / "fragment.msl").write_bytes(b"incomplete")
            with self.assertRaisesRegex(ValueError, "Incomplete"):
                publish("model", stages, output)
            self.assertEqual(output.read_bytes(), original)
            (root / "fragment.msl").unlink()
            with self.assertRaises(FileNotFoundError):
                publish("model", stages, output)
            self.assertEqual(output.read_bytes(), original)
            self.stage(root, "fragment", "fragment")
            data = json.loads(stages["lambert"].read_text())
            data["layout"]["file"] = "../outside"
            stages["lambert"].write_text(json.dumps(data))
            with self.assertRaisesRegex(ValueError, "Unsafe"):
                publish("model", stages, output)
            self.assertEqual(output.read_bytes(), original)

    def test_concurrent_complete_publications_are_readable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stages = {"vertex": self.stage(root, "vertex", "vertex"),
                      "lambert": self.stage(root, "fragment", "fragment")}
            destination = root / "out/model.json"
            with ThreadPoolExecutor(max_workers=4) as pool:
                values = list(pool.map(lambda _: publish("model", stages, destination), range(8)))
            self.assertTrue(all(value == values[0] for value in values))
            self.assertEqual(json.loads(destination.read_text()), values[0])

    def test_readers_never_observe_mixed_programs_during_replacement(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            vertex = self.stage(root, "vertex", "vertex")
            variants = [{"vertex": vertex, "lambert": self.stage(root, f"fragment{i}", "fragment")}
                        for i in range(3)]
            destination = root / "out/model.json"
            publish("model", variants[0], destination)
            stop = threading.Event()

            def observe():
                count = 0
                while True:
                    document = json.loads(read_publication(destination))
                    for path, descriptor in document["files"].items():
                        content = (destination.parent / path).read_bytes()
                        self.assertEqual(len(content), descriptor["bytes"])
                        self.assertEqual(sha(content), descriptor["sha256"])
                    count += 1
                    if stop.wait(0.001):
                        return count

            with ThreadPoolExecutor(max_workers=3) as pool:
                reader = pool.submit(observe)
                try:
                    list(pool.map(lambda i: publish("model", variants[i % 3], destination), range(24)))
                finally:
                    stop.set()
                self.assertGreater(reader.result(), 0)


class WatchTests(unittest.TestCase):
    def test_includes_and_failed_build_leave_publication_intact(self):
        with tempfile.TemporaryDirectory(prefix="shader watch ") as directory:
            root = Path(directory)
            source = root / "scene.hlsl"
            include = root / "common #1.hlsli"
            source.write_text("include")
            include.write_text("first")
            depfile = root / "scene.d"
            escaped = include.as_posix().replace(":", "\\:").replace(" ", "\\ ").replace("#", "\\#")
            depfile.write_text("out: " + escaped + "\n")
            manifest = root / "model.json"
            manifest.write_bytes(b"last-good")
            config = {"sources": [str(source)], "include_roots": [str(root)],
                      "depfiles": [str(depfile)], "cmake": "cmake", "target": "program",
                      "manifest": str(manifest), "program": "model"}
            self.assertIn(include, dependencies(config))
            before = fingerprint(config)
            include.write_text("other")
            self.assertNotEqual(before, fingerprint(config))
            with patch("watch.subprocess.run") as call:
                call.return_value.returncode = 1
                call.return_value.stdout = "compiler refused"
                call.return_value.stderr = ""
                self.assertFalse(build(config, root, "Debug"))
            self.assertEqual(manifest.read_bytes(), b"last-good")
            status = json.loads((root / "model.status.json").read_text())
            self.assertFalse(status["ok"])
            self.assertIn("compiler refused", status["message"])


if __name__ == "__main__":
    unittest.main()
