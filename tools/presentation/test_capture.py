"""Exercise engine readback transport and reject stale/incomplete/UI-free captures."""
import json
from pathlib import Path
import tempfile
import unittest
from capture import convert_frame, check_ui, read_png


class CaptureContractTests(unittest.TestCase):
    def test_frame_transport_and_invalid_metadata(self):
        with tempfile.TemporaryDirectory() as folder:
            prefix = Path(folder) / "frame"
            metadata = dict(version=1, width=2, height=1, format="RGBA8", encoding="sRGB",
                            steps=45, debug_ui=True, source="engine-composed-frame")
            pixels = bytes([12, 34, 56, 255, 90, 80, 70, 255])
            prefix.with_suffix(".rgba").write_bytes(pixels)
            prefix.with_suffix(".json").write_text(json.dumps(metadata), encoding="utf-8")
            self.assertEqual(convert_frame(prefix, 45), metadata)
            self.assertEqual(read_png(prefix.with_suffix(".png")), (2, 1, pixels))
            for key, value in [("steps", 44), ("debug_ui", False), ("source", "desktop"),
                               ("format", "BGRA8"), ("encoding", "linear"), ("width", 0),
                               ("height", 1.5), ("width", True), ("version", 2)]:
                with self.subTest(key=key):
                    prefix.with_suffix(".json").write_text(json.dumps(dict(metadata, **{key: value})), encoding="utf-8")
                    with self.assertRaises(ValueError):
                        convert_frame(prefix, 45)
            prefix.with_suffix(".json").write_text(json.dumps(metadata), encoding="utf-8")
            prefix.with_suffix(".rgba").write_bytes(pixels[:-1])
            with self.assertRaisesRegex(ValueError, "byte count"):
                convert_frame(prefix, 45)

    def test_ui_witness_detects_scene_without_overlay(self):
        probe = dict(name="inspector", region=[.5, 0, 1, 1], minimum_channel=230, minimum_pixels=1)
        self.assertEqual(check_ui(2, 1, bytes([0, 0, 0, 255, 240, 240, 240, 255]), [probe]),
                         [dict(name="inspector", pixels=1)])
        with self.assertRaisesRegex(ValueError, "Missing UI witness"):
            check_ui(2, 1, bytes([240, 240, 240, 255, 0, 0, 0, 255]), [probe])

    def test_no_desktop_or_system_input_dependency(self):
        source = Path(__file__).with_name("capture.py").read_text(encoding="utf-8")
        for token in ["WinDLL", "screencapture", "xdotool", "swiftc", "snapshot(", "SetForegroundWindow", "SendInput"]:
            self.assertNotIn(token, source)
        for token in ['"--replay"', '"--capture-step"', '"--capture-output"', 'fixed_steps=']:
            self.assertIn(token, source)


if __name__ == "__main__":
    unittest.main()
