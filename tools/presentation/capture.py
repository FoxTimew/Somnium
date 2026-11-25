"""Run identical fixed-step replay captures on all desktop platforms.
Only pixels are read from the OS. The sandbox freezes at the requested tick and exits itself.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/gpu"))
from compare import read_png
from run import native_validation_error


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--backend", choices=["direct3d12", "vulkan", "metal"])
    parser.add_argument("--scenario", type=Path, default=Path(__file__).with_name("orbit.json"))
    args = parser.parse_args()
    system = platform.system()
    backend = args.backend or {"Windows": "direct3d12", "Linux": "vulkan", "Darwin": "metal"}[system]
    output, executable = args.output.resolve(), args.executable.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scenario = json.loads(args.scenario.read_text(encoding="utf-8"))
    if scenario["version"] != 1:
        raise ValueError("Unsupported presentation scenario version")
    replay = ROOT / "assets" / scenario["replay"]
    report = {"origin": ("CI " if os.getenv("CI") else "local ") + system, "backend": backend,
              "source_commit": os.getenv("GITHUB_SHA") or subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
              "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
              "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
              "replay_sha256": hashlib.sha256(replay.read_bytes()).hexdigest(),
              "scenario_sha256": hashlib.sha256(args.scenario.read_bytes()).hexdigest(),
              "input_source": "platform fixed-step replay (no OS input)", "status": "échec", "captures": []}
    environment = dict(os.environ, SDL_ASSERT="abort", SDL_LOGGING="*=verbose")
    if system == "Darwin":
        import capture_macos
        helper = capture_macos.prepare(output)
        snapshot = lambda pid, path: capture_macos.snapshot(pid, path, helper)
        environment.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1", MTL_SHADER_VALIDATION_REPORT_TO_STDERR="1")
    elif system == "Windows":
        from capture_windows import snapshot
    else:
        from capture_linux import snapshot
    combined = []
    try:
        for case in scenario["cases"]:
            name, steps = case["name"], case["steps"]
            log_path = output / (name + ".log")
            with log_path.open("w", encoding="utf-8") as log:
                command = [str(executable), "--backend", backend, "--replay", scenario["replay"], "--capture-step", str(steps)]
                process = subprocess.Popen(command, cwd=output, env=environment, stdout=log, stderr=subprocess.STDOUT)
                try:
                    deadline = time.monotonic() + 30 + steps / 30
                    while time.monotonic() < deadline:
                        text = log_path.read_text(encoding="utf-8", errors="replace")
                        if f"CAPTURE_READY steps={steps}\n" in text:
                            break
                        if process.poll() is not None:
                            raise RuntimeError(f"{name}: exited before capture ({process.returncode})")
                        time.sleep(.05)
                    else:
                        raise RuntimeError(f"{name}: replay capture timed out")
                    time.sleep(.15) # Let the compositor display the submitted stable frame.
                    path = output / (name + ".png")
                    snapshot(process.pid, path)
                    if process.wait(timeout=15) != 0:
                        raise RuntimeError(f"{name}: sandbox failed on shutdown")
                    text = log_path.read_text(encoding="utf-8", errors="replace")
                    combined.append(text)
                    if native_validation_error(text):
                        raise RuntimeError(f"{name}: native validation diagnostic")
                    enabled = ("Validation layers enabled" in text if backend != "metal" else
                               "Metal API Validation Enabled" in text and "Metal GPU Validation Enabled" in text)
                    if not enabled:
                        raise RuntimeError(f"{name}: validation markers missing")
                    if system == "Windows" and "6600M" not in text:
                        raise RuntimeError(f"{name}: expected Radeon RX 6600M")
                    if f"fixed_steps={steps} " not in text:
                        raise RuntimeError(f"{name}: simulation continued past the capture boundary")
                    report["captures"].append({"name": path.name, "steps": steps, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=10)
        differences = []
        for comparison in scenario["comparisons"]:
            width, height, a = read_png(output / (comparison["a"] + ".png"), allow_rgb=True)
            bw, bh, b = read_png(output / (comparison["b"] + ".png"), allow_rgb=True)
            if (width, height) != (bw, bh):
                raise RuntimeError("Capture dimensions differ")
            # Exclude timing overlay; inspect the central/lower model region.
            count = sum(a[i:i+3] != b[i:i+3] for y in range(height // 3, height * 9 // 10)
                        for x in range(width // 3, width * 9 // 10) for i in [4 * (y * width + x)])
            differences.append(dict(comparison, different_pixels=count))
            if count < comparison.get("minimum", 0) or count > comparison.get("maximum", width * height):
                raise RuntimeError(f"Replay image comparison failed: {differences[-1]}")
        report["comparisons"] = differences
        report["status"] = "validé"
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        report["reason"] = str(error)
    (output / "validation.log").write_text("\n".join(combined), encoding="utf-8")
    (output / "result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if os.getenv("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
            summary.write(f"\nPrésentation {backend}, rejeu sans injection : **{report['status']}** ({output}).\n")
    return 0 if report["status"] == "validé" else 1


if __name__ == "__main__":
    sys.exit(main())
