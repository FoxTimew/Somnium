"""Capture a replay-driven Vulkan window; X11 is queried but receives no input."""
import subprocess
import sys

def snapshot(pid, path):
    result = subprocess.run(["xdotool", "search", "--pid", str(pid), "--name", "Somnium"], capture_output=True, text=True, check=True, timeout=10)
    windows = result.stdout.splitlines()
    if not windows:
        raise RuntimeError("No sandbox X11 window")
    subprocess.run(["import", "-window", windows[0], "-depth", "8", "PNG32:" + str(path)], check=True, timeout=10)

if __name__ == "__main__":
    from capture import main
    sys.exit(main())
