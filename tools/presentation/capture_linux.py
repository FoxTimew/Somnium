"""Capture actual Vulkan presentation and drive orbit/zoom through X11 input.
Run under Xvfb with xdotool and ImageMagick; captures are pixels from the window.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gpu"))
from compare import read_png
from run import native_validation_error

parser = argparse.ArgumentParser()
parser.add_argument("--executable", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
executable, output = args.executable.resolve(), args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
environment = dict(os.environ, SDL_ASSERT="abort", SDL_LOGGING="*=verbose")
result = {"origin": "GitHub Actions Linux runner" if os.getenv("CI") else "local Linux (Docker/Xvfb)",
          "backend": "vulkan", "commit": os.getenv("GITHUB_SHA") or subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
          "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
          "captures": [], "actions": [], "status": "échec"}
binary_suffixes = ["glb", "bin", "png", "jpg", "jpeg", "tga", "dds", "ktx2", "wav", "ogg", "ttf"]
result["code_worktree_dirty"] = bool(subprocess.check_output(
    ["git", "status", "--porcelain", "--untracked-files=all", "--", "engine", "game", "sandbox", "assets", "tools", "cmake", ".github",
     *[f":(exclude,glob)**/*.{suffix}" for suffix in binary_suffixes]], text=True).strip())
result["source_model_sha256"] = hashlib.sha256((Path(__file__).resolve().parents[2] / "assets/models/orbit-cube.glb").read_bytes()).hexdigest()

def run(*command, check=True):
    return subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=20, check=check)

with (output / "validation.log").open("w", encoding="utf-8") as log:
    process = subprocess.Popen([str(executable), "--backend", "vulkan"], cwd=output, env=environment,
                               stdout=log, stderr=subprocess.STDOUT)
    try:
        window = None
        for _ in range(100):
            if process.poll() is not None:
                raise RuntimeError(f"Sandbox exited before capture: {process.returncode}")
            found = run("xdotool", "search", "--pid", process.pid, "--name", "Somnium", check=False)
            if found.returncode == 0 and found.stdout.strip():
                window = found.stdout.splitlines()[0]
                break
            time.sleep(.1)
        if not window:
            raise RuntimeError("No sandbox window")
        run("xdotool", "windowfocus", "--sync", window)
        for _ in range(300):
            if process.poll() is not None:
                raise RuntimeError("Sandbox exited before its first presented model frame")
            if "First model frame presented" in (output / "validation.log").read_text(encoding="utf-8", errors="replace"):
                break
            time.sleep(.1)
        else:
            raise RuntimeError("Timed out waiting for the first presented model frame")
        time.sleep(.2)

        def capture(name):
            path = output / f"{name}.png"
            run("import", "-window", window, "-depth", "8", "PNG32:" + str(path))
            result["captures"].append(path.name)

        capture("model-overlay")
        run("xdotool", "mousemove", "--window", window, "700", "400")
        run("xdotool", "mousedown", "3")
        for step in range(1, 13):
            run("xdotool", "mousemove", "--window", window, 700 + step * 5, 400 - step * 2)
            time.sleep(.05)
        run("xdotool", "mouseup", "3")
        result["actions"].append("Right-button drag: +60,-24 client pixels")
        time.sleep(1)
        capture("orbit")
        run("xdotool", "click", "--repeat", "2", "--delay", "150", "4")
        result["actions"].append("Wheel: +2 units")
        time.sleep(1)
        capture("zoom")
        run("xdotool", "key", "r")
        result["actions"].append("Reset: physical R key")
        time.sleep(1)
        capture("reset")

        images = [read_png(output / f"{name}.png") for name in ["model-overlay", "orbit", "zoom", "reset"]]
        width, height, first = images[0]
        if any(image[:2] != (width, height) for image in images):
            raise RuntimeError("Unexpected capture extent change")
        # Ignore overlay timing in the top-left; compare the model's central/lower region.
        region = [4 * (y * width + x) for y in range(height // 3, height * 9 // 10)
                  for x in range(width // 3, width * 9 // 10)]
        def different(a, b):
            return sum(a[i:i+3] != b[i:i+3] for i in region)
        result["model_region_different_pixels"] = {"orbit": different(first, images[1][2]),
            "zoom": different(images[1][2], images[2][2]), "reset": different(first, images[3][2])}
        counts = result["model_region_different_pixels"]
        if counts["orbit"] < 500 or counts["zoom"] < 500 or counts["reset"] != 0:
            raise RuntimeError("Orbit/zoom must change model pixels and reset must restore them exactly")
        run("xdotool", "windowsize", "--sync", window, "960", "600")
        result["actions"].append("Resize: 960x600, recreating the depth target")
        time.sleep(.5)
        capture("resize")
        result["resized_extent"] = read_png(output / "resize.png")[:2]
        if result["resized_extent"] != (960, 600):
            raise RuntimeError("Resize did not reach the requested drawable extent")
        result["status"] = "validé"
    except (RuntimeError, subprocess.SubprocessError, ValueError) as error:
        result["reason"] = str(error)
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGINT)
        try:
            result["exit_code"] = process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            result["exit_code"] = -1

log = (output / "validation.log").read_text(encoding="utf-8")
result["validation_errors"] = native_validation_error(log)
result["validation_enabled"] = "Validation layers enabled" in log
if result["exit_code"] != 0 or result["validation_errors"] or not result["validation_enabled"]:
    result["status"] = "échec"
(output / "result.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
print(json.dumps(result, indent=2, ensure_ascii=False))
sys.exit(0 if result["status"] == "validé" else 1)
