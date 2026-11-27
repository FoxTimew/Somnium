"""Prove editing, persistence and Stop restoration through composed engine frames.
Only source JSON is staged in the run directory; models remain in the repository roots.
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
sys.path.insert(0, str(ROOT))
from tools.presentation.capture import convert_frame, check_ui
from tools.gpu.compare import read_png
from tools.gpu.run import native_validation_error


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def differences(a, b, region):
    width, height, pixels = read_png(a)
    bw, bh, other = read_png(b)
    if (width, height) != (bw, bh):
        raise RuntimeError("Iteration frame extents differ")
    left, top, right, bottom = region
    return sum(pixels[i:i+4] != other[i:i+4]
               for y in range(int(top * height), int(bottom * height))
               for x in range(int(left * width), int(right * width))
               for i in [4 * (y * width + x)])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--backend", choices=["direct3d12", "vulkan", "metal"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--scenario", type=Path, default=Path(__file__).with_name("scenario.json"))
    args = parser.parse_args()
    executable, output = args.executable.resolve(), args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    scenario = json.loads(args.scenario.read_text(encoding="utf-8"))
    if scenario["version"] != 1:
        raise ValueError("Unsupported iteration scenario")
    source = output / "source"
    (source / "scenes").mkdir(parents=True, exist_ok=True)
    original = (ROOT / "assets/scenes/courtyard.json").read_bytes()
    scene_path = source / "scenes/courtyard.json"
    write_json(source / "replays/iteration-input.json", scenario["input"])
    report = {
        "version": 1, "status": "échec", "backend": args.backend,
        "origin": ("CI " if os.getenv("CI") else "local ") + platform.system(),
        "source_commit": os.getenv("GITHUB_SHA") or subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
        "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
        "scenario_sha256": hashlib.sha256(args.scenario.read_bytes().replace(b"\r\n", b"\n")).hexdigest(),
        "source": "engine-composed-frame; fixed-step editor commands and neutral input; no OS injection",
        "captures": []}
    environment = dict(os.environ, SDL_ASSERT="abort", SDL_LOGGING="*=verbose")
    traces = {}

    def capture(name, steps, commands):
        write_json(source / "replays/iteration-editor.json", {"version": 1, "commands": commands})
        prefix = output / name
        for suffix in [".json", ".rgba", ".png", ".editor.json"]:
            prefix.with_suffix(suffix).unlink(missing_ok=True)
        command = [str(executable), "--mode", "edit", "--backend", args.backend,
                   "--source-root", str(source), "--scene", "scenes/courtyard.json",
                   "--editor-replay", "replays/iteration-editor.json",
                   "--replay", "replays/iteration-input.json",
                   "--capture-step", str(steps), "--capture-output", str(prefix),
                   "--iteration-report", str(prefix.with_suffix(".editor.json"))]
        log_path = prefix.with_suffix(".log")
        with log_path.open("w", encoding="utf-8") as log:
            subprocess.run(command, cwd=output, env=environment, stdout=log,
                           stderr=subprocess.STDOUT, timeout=45 + steps / 30, check=True)
        text = log_path.read_text(encoding="utf-8", errors="replace")
        if native_validation_error(text):
            raise RuntimeError(f"{name}: native validation diagnostic")
        if args.backend != "metal" and "Validation layers enabled" not in text:
            raise RuntimeError(f"{name}: native validation was not enabled")
        if platform.system() == "Windows" and "6600M" not in text:
            raise RuntimeError(f"{name}: Radeon device was not used")
        metadata = convert_frame(prefix, steps)
        width, height, pixels = read_png(prefix.with_suffix(".png"))
        witnesses = check_ui(width, height, pixels, scenario["ui_probes"])
        trace = json.loads(prefix.with_suffix(".editor.json").read_text(encoding="utf-8"))
        expected = [entry for entry in commands if entry["step"] < steps]
        if len(trace["events"]) != len(expected) or trace["steps"] != steps:
            raise RuntimeError(f"{name}: editor command count or fixed step differs")
        for entry, event in zip(expected, trace["events"]):
            if event["command"] != entry["command"] or event["step"] != entry["step"]:
                raise RuntimeError(f"{name}: editor replay order differs")
            if event["accepted"] == bool(entry.get("errorContains")):
                raise RuntimeError(f"{name}: command outcome differs")
        traces[name] = trace
        report["captures"].append({"name": name + ".png", "metadata": metadata, "ui_probes": witnesses,
                                    "canonical_sha256": hashlib.sha256(trace["final"].encode()).hexdigest(),
                                    "sha256": hashlib.sha256(prefix.with_suffix(".png").read_bytes()).hexdigest()})

    try:
        for case in scenario["cases"]:
            scene_path.write_bytes(original)
            capture(case["name"], case["steps"], scenario["commands"])
        edited = traces["edited-saved"]["final"]
        stopped = traces["stopped-restored"]
        if stopped["final"] != edited or stopped["playing"] or stopped["dirty"]:
            raise RuntimeError("Stop did not restore the exact saved editing World")
        events = stopped["events"]
        successful_saves = [event["canonical"] for event in events
                            if event["command"]["op"] == "save" and event["accepted"]]
        if len(successful_saves) != 2 or any(value != edited for value in successful_saves):
            raise RuntimeError("Consecutive saves did not preserve canonical bytes")
        if scene_path.read_bytes() != edited.encode("utf-8"):
            raise RuntimeError("Published source differs from the canonical editor snapshot")
        # A new process reads the published file, with the same independently owned tool camera.
        capture("reloaded", 2, [
            {"step": 0, "command": {"op": "select", "id": "iteration-pivot", "component": "game.AabbCollider"}},
            {"step": 1, "command": {"op": "save"}}])
        if traces["reloaded"]["initial"] != edited or traces["reloaded"]["final"] != edited:
            raise RuntimeError("Restarted process did not load the identical canonical scene")
        if scene_path.read_bytes() != edited.encode("utf-8"):
            raise RuntimeError("Restarted save changed canonical bytes")
        comparisons = []
        for a, b, minimum, maximum in [
            ("initial", "edited-saved", scenario["minimum_edited_pixels"], None),
            ("edited-saved", "stopped-restored", 0, 0),
            ("edited-saved", "reloaded", 0, 0)]:
            count = differences(output / (a + ".png"), output / (b + ".png"), scenario["scene_region"])
            comparisons.append({"a": a, "b": b, "different_pixels": count})
            if count < minimum or (maximum is not None and count > maximum):
                raise RuntimeError(f"Iteration image comparison failed: {comparisons[-1]}")
        report["comparisons"] = comparisons
        report["saved_sha256"] = hashlib.sha256(scene_path.read_bytes()).hexdigest()
        report["status"] = "validé"
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        report["reason"] = str(error)
        for log in output.glob("*.log"):
            print(log.name + "\n" + "\n".join(log.read_text(encoding="utf-8", errors="replace").splitlines()[-25:]), file=sys.stderr)
    write_json(output / "result.json", report)
    print(json.dumps(report, indent=2, ensure_ascii=False))
    return 0 if report["status"] == "validé" else 1


if __name__ == "__main__":
    sys.exit(main())
