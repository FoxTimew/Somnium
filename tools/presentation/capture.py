"""Run identical fixed-step replay captures on all desktop platforms.
The engine reads its composed scene/UI target, copies it to the swapchain and exits.
No desktop pixels, window queries, focus changes or system input are involved.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/gpu"))
from compare import read_png, write_png
from run import native_validation_error


def convert_frame(prefix, steps):
    metadata = json.loads(prefix.with_suffix(".json").read_text(encoding="utf-8"))
    if (metadata.get("version") != 1 or metadata.get("format") != "RGBA8" or
            metadata.get("encoding") != "sRGB" or metadata.get("source") != "engine-composed-frame" or
            metadata.get("steps") != steps or metadata.get("debug_ui") is not True):
        raise ValueError("Invalid composed frame metadata (exact replay step and UI required)")
    width, height = metadata.get("width"), metadata.get("height")
    if any(type(value) is not int or value <= 0 or value > 16384 for value in (width, height)):
        raise ValueError("Invalid composed frame extent")
    write_png(prefix.with_suffix(".png"), width, height, prefix.with_suffix(".rgba").read_bytes())
    return metadata


def check_ui(width, height, rgba, probes):
    results = []
    for probe in probes:
        left, top, right, bottom = probe["region"]
        count = sum(min(rgba[i:i+3]) >= probe["minimum_channel"]
                    for y in range(int(height * top), int(height * bottom))
                    for x in range(int(width * left), int(width * right)) for i in [4 * (y * width + x)])
        if count < probe["minimum_pixels"]:
            raise ValueError(f"Missing UI witness {probe['name']}: {count} pixels")
        results.append({"name": probe["name"], "pixels": count})
    return results


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--backend", choices=["direct3d12", "vulkan", "metal"])
    parser.add_argument("--scenario", type=Path, default=Path(__file__).with_name("courtyard.json"))
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
              "replay_sha256": hashlib.sha256(replay.read_bytes().replace(b"\r\n", b"\n")).hexdigest(),
              "scenario_sha256": hashlib.sha256(args.scenario.read_bytes().replace(b"\r\n", b"\n")).hexdigest(),
              "input_source": "platform fixed-step replay (no OS input)",
              "capture_source": "engine-composed-frame (scene + UI, swapchain extent)",
              "status": "échec", "captures": []}
    environment = dict(os.environ, SDL_ASSERT="abort", SDL_LOGGING="*=verbose")
    if system == "Darwin":
        environment.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1", MTL_SHADER_VALIDATION_REPORT_TO_STDERR="1")
    combined = []
    try:
        for case in scenario["cases"]:
            name, steps = case["name"], case["steps"]
            log_path = output / (name + ".log")
            with log_path.open("w", encoding="utf-8") as log:
                prefix = output / name
                # A failed invocation must never consume files left by an earlier run.
                for suffix in [".rgba", ".json", ".png"]:
                    prefix.with_suffix(suffix).unlink(missing_ok=True)
                command = [str(executable), "--backend", backend, "--replay", scenario["replay"],
                           "--capture-step", str(steps), "--capture-output", str(prefix)]
                process = subprocess.Popen(command, cwd=output, env=environment, stdout=log, stderr=subprocess.STDOUT)
                try:
                    path = output / (name + ".png")
                    if process.wait(timeout=30 + steps / 30) != 0:
                        raise RuntimeError(f"{name}: sandbox failed on shutdown")
                    metadata = convert_frame(prefix, steps)
                    text = log_path.read_text(encoding="utf-8", errors="replace")
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
                    for marker in case.get("markers", []):
                        if marker not in text:
                            raise RuntimeError(f"{name}: missing gameplay evidence: {marker}")
                    width, height, rgba = read_png(path)
                    witnesses = check_ui(width, height, rgba, scenario.get("ui_probes", []))
                    report["captures"].append({"name": path.name, "steps": steps, "width": width, "height": height,
                                              "metadata": metadata, "ui_probes": witnesses,
                                              "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=10)
                    combined.append(log_path.read_text(encoding="utf-8", errors="replace"))
        differences = []
        for comparison in scenario["comparisons"]:
            width, height, a = read_png(output / (comparison["a"] + ".png"))
            bw, bh, b = read_png(output / (comparison["b"] + ".png"))
            if (width, height) != (bw, bh):
                raise RuntimeError("Capture dimensions differ")
            # The versioned central region compares poses; UI has separate witnesses.
            left, top, right, bottom = scenario.get("region", [.25, .3, .68, .9])
            count = sum(a[i:i+3] != b[i:i+3] for y in range(int(height * top), int(height * bottom))
                        for x in range(int(width * left), int(width * right)) for i in [4 * (y * width + x)])
            differences.append(dict(comparison, different_pixels=count))
            if count < comparison.get("minimum", 0) or count > comparison.get("maximum", width * height):
                raise RuntimeError(f"Replay image comparison failed: {differences[-1]}")
        report["comparisons"] = differences
        report["status"] = "validé"
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError) as error:
        report["reason"] = str(error)
        if combined:
            print("\n".join(combined[-1].splitlines()[-120:]), file=sys.stderr)
    (output / "validation.log").write_text("\n".join(combined), encoding="utf-8")
    (output / "result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2, ensure_ascii=False))
    if os.getenv("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
            summary.write(f"\nFrame moteur scène + UI, {backend}, rejeu sans injection : **{report['status']}** ({output}).\n")
    return 0 if report["status"] == "validé" else 1


if __name__ == "__main__":
    sys.exit(main())
