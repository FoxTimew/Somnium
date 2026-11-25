"""Capture an already presented Metal window without posting OS events."""
import json
from pathlib import Path
import subprocess
import sys

def prepare(output):
    helper = output / "macos-window"
    subprocess.run(["swiftc", str(Path(__file__).with_name("macos_window.swift")), "-o", str(helper)], check=True, timeout=60)
    return helper

def snapshot(pid, path, helper):
    result = subprocess.run([str(helper), str(pid)], capture_output=True, text=True, check=True, timeout=10)
    windows = json.loads(result.stdout)
    if not windows:
        raise RuntimeError("No sandbox surface on the runner desktop")
    window = max(windows, key=lambda item: item.get("kCGWindowBounds", {}).get("Width", 0) * item.get("kCGWindowBounds", {}).get("Height", 0))
    subprocess.run(["screencapture", "-x", "-o", "-l", str(window["kCGWindowNumber"]), str(path)], check=True, timeout=10)

if __name__ == "__main__":
    from capture import main
    sys.exit(main())
