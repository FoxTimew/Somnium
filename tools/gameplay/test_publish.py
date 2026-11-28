"""Checks publication ordering, identity, symbols and preservation on missing build output.
No linker or running sandbox is required to exercise the external file contract.
"""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from publish import publish


class PublishTests(unittest.TestCase):
    def test_immutable_generation_and_noop(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary, symbols = root / "game.dll", root / "game.pdb"
            binary.write_bytes(b"one")
            symbols.write_bytes(b"symbols")
            output = root / "installed/game.json"
            publish(binary, symbols, output)
            first = output.read_bytes()
            stamp = output.stat().st_mtime_ns
            document = json.loads(first)
            old = output.parent / document["binary"]["path"]
            self.assertEqual(old.read_bytes(), b"one")
            self.assertEqual(document["binary"]["sha256"], hashlib.sha256(b"one").hexdigest())
            self.assertEqual((output.parent / document["symbols"]["path"]).name, "game.pdb")
            publish(binary, symbols, output)
            self.assertEqual(output.stat().st_mtime_ns, stamp)
            binary.write_bytes(b"two")
            publish(binary, symbols, output)
            self.assertNotEqual(first, output.read_bytes())
            self.assertEqual(old.read_bytes(), b"one")

    def test_missing_symbols_cannot_publish_partial_generation(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            binary = root / "game.so"
            binary.write_bytes(b"one")
            output = root / "installed/game.json"
            publish(binary, None, output)
            original = output.read_bytes()
            binary.write_bytes(b"two")
            with self.assertRaises(FileNotFoundError):
                publish(binary, root / "missing.pdb", output)
            self.assertEqual(output.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
