"""Enforce read-only OS capture and shared replay delivery on every host."""
from pathlib import Path
import unittest
import sys
import struct
import tempfile
import zlib
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gpu"))
from compare import read_png
from window_visibility import overlaps, require_unobscured

ROOT = Path(__file__).resolve().parent

class CaptureContractTests(unittest.TestCase):
    def test_topmost_overlay_rejected_even_if_sandbox_is_foreground(self):
        with self.assertRaisesRegex(RuntimeError, "obscured"):
            require_unobscured((0, 0, 1280, 720), [((848, 0, 1280, 285), True, False, False)])

    def test_hidden_minimized_cloaked_or_nonoverlapping_windows_are_harmless(self):
        rect = (20, 20, 80, 80)
        require_unobscured((0, 0, 100, 100), [
            (rect, False, False, False), (rect, True, True, False),
            (rect, True, False, True), ((100, 0, 200, 100), True, False, False)])
        self.assertFalse(overlaps((0, 0, 100, 100), (10, 100, 20, 110)))
        self.assertTrue(overlaps((0, 0, 100, 100), (99, 99, 101, 101)))

    def test_no_system_input_or_focus_mutation(self):
        forbidden = ["Send" + "Input", "Set" + "CursorPos", "Set" + "ForegroundWindow", "post" + "ToPid", "AXUIElementSet" + "AttributeValue", '"mouse' + 'down"', '"mouse' + 'move"', '"window' + 'focus"']
        for name in ["capture.py", "capture_windows.py", "capture_linux.py", "capture_macos.py", "macos_window.swift"]:
            source = (ROOT / name).read_text(encoding="utf-8")
            for token in forbidden:
                self.assertNotIn(token, source, name)

    def test_rgb_screenshot_reader_does_not_relax_reference_contract(self):
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
        png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 1, 8, 2, 0, 0, 0))
        png += chunk(b"IDAT", zlib.compress(bytes([1, 20, 30, 40, 5, 5, 5]))) + chunk(b"IEND", b"")
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "screenshot.png"
            path.write_bytes(png)
            with self.assertRaises(ValueError):
                read_png(path)
            self.assertEqual(read_png(path, allow_rgb=True), (2, 1, bytes([20, 30, 40, 255, 25, 35, 45, 255])))

    def test_shared_driver_requires_replay_and_exact_step_count(self):
        source = (ROOT / "capture.py").read_text(encoding="utf-8")
        for token in ['"--replay"', '"--capture-step"', 'CAPTURE_READY', 'fixed_steps=']:
            self.assertIn(token, source)

if __name__ == "__main__":
    unittest.main()
