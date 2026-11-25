#!/usr/bin/env python3
"""Exercise focus-denial boundaries without opening a window or injecting input."""
import unittest
from unittest.mock import patch

import capture_windows as capture


class ActivationTests(unittest.TestCase):
    def setUp(self):
        self.mocks = {}
        for name in ["foreground", "get_foreground", "position", "extent", "cursor", "window_at", "send_input"]:
            control = patch.object(capture, name)
            self.mocks[name] = control.start()
            self.addCleanup(control.stop)
        sleep = patch.object(capture.time, "sleep")
        sleep.start()
        self.addCleanup(sleep.stop)
        self.mocks["extent"].return_value = (1280, 720, capture.w.POINT(10, 20))
        self.mocks["send_input"].return_value = 2

    def test_already_foreground_never_clicks(self):
        self.mocks["get_foreground"].return_value = 42
        self.assertEqual(capture.activate(42), "SetForegroundWindow")
        self.mocks["position"].assert_not_called()
        self.mocks["send_input"].assert_not_called()

    def test_covered_window_refuses_input_and_restores_order(self):
        self.mocks["get_foreground"].return_value = 7
        self.mocks["window_at"].return_value = 7
        with self.assertRaisesRegex(RuntimeError, "covered; refusing input"):
            capture.activate(42)
        self.mocks["send_input"].assert_not_called()
        self.assertEqual(self.mocks["position"].call_count, 2)

    def test_focus_denial_uses_verified_client_click(self):
        self.mocks["get_foreground"].side_effect = [7, 42]
        self.mocks["window_at"].return_value = 42
        self.assertEqual(capture.activate(42), "verified client click")
        self.mocks["cursor"].assert_called_once_with(970, 560)
        self.assertEqual(self.mocks["send_input"].call_args.args[0], 2)
        self.assertEqual(self.mocks["position"].call_count, 2)


if __name__ == "__main__":
    unittest.main()
