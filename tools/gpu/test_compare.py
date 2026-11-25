"""Protect exact witnesses independently of the whole-image tolerance budget.
These CPU tests never claim to prove a rendered GPU image.
"""
import tempfile
from pathlib import Path
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from compare import compare, read_png, write_png
from run import accept_capture, native_validation_error


class ComparisonTests(unittest.TestCase):
    def test_native_errors_are_fatal_even_with_a_matching_image(self):
        self.assertTrue(native_validation_error('Texture usage flags mismatch executing fragment function: "psMain"'))
        self.assertTrue(native_validation_error('Validation Error: [ VUID-vkCmdDraw-None-08114 ]'))
        self.assertTrue(native_validation_error('D3D12 ERROR: ID3D12CommandList::DrawInstanced: invalid state'))
        self.assertTrue(native_validation_error('D3D12 CORRUPTION: invalid object'))
        self.assertFalse(native_validation_error('Metal API Validation Enabled\nMetal GPU Validation Enabled'))

    def setUp(self):
        self.scene = dict(width=100, height=10, channel_tolerance=2, max_different_fraction=.001,
                          probes=[dict(name="center", x=50, y=5, rgba=[0, 0, 0, 255])])
        self.image = bytes([0, 0, 0, 255] * 1000)

    def test_inclusive_channel_and_fraction_limits(self):
        actual = bytearray(self.image)
        actual[0] = 2
        self.assertEqual(compare(self.scene, actual, self.image)[0]["different_pixels"], 0)
        actual[0] = 3
        self.assertTrue(compare(self.scene, actual, self.image)[0]["passed"])
        actual[4] = 3
        self.assertFalse(compare(self.scene, actual, self.image)[0]["passed"])

    def test_exact_probe_overrides_tolerance(self):
        actual = bytearray(self.image)
        actual[(5 * 100 + 50) * 4] = 1
        report, _ = compare(self.scene, actual, self.image)
        self.assertEqual(report["different_pixels"], 0)
        self.assertFalse(report["passed"])
        self.assertFalse(compare(self.scene, actual, actual)[0]["passed"])

    def test_alpha_and_wrong_size(self):
        actual = bytearray(self.image)
        actual[3] = actual[7] = 0
        self.assertFalse(compare(self.scene, actual, self.image)[0]["passed"])
        with self.assertRaises(ValueError):
            compare(self.scene, actual[:-1], self.image)

    def test_png_round_trip_and_corruption(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "capture.png"
            write_png(path, 100, 10, self.image)
            self.assertEqual(read_png(path), (100, 10, self.image))
            corrupted = bytearray(path.read_bytes())
            corrupted[-5] ^= 1
            path.write_bytes(corrupted)
            with self.assertRaises(ValueError):
                read_png(path)

    def test_reference_updates_require_explicit_local_consent(self):
        args = SimpleNamespace(confirm_reference_update=True, accept_capture="unused")
        with patch.dict("os.environ", {"CI": "true"}):
            with self.assertRaisesRegex(ValueError, "outside CI"):
                accept_capture(args, {})
        args.confirm_reference_update = False
        with patch.dict("os.environ", {"CI": ""}):
            with self.assertRaisesRegex(ValueError, "confirm-reference-update"):
                accept_capture(args, {})


if __name__ == "__main__":
    unittest.main()
