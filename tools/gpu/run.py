"""Run D5 and preserve captures, metrics and native validation logs.
Missing devices are reported separately from rendering or comparison failures.
"""
import argparse
import hashlib
import json
import math
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys

from compare import compare, probes, read_png, write_png

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "engine/gpu/tests/reference_scene.json"
REFERENCE = ROOT / "engine/gpu/tests/reference/gpu_conventions.png"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def native_validation_error(log):
    return re.search(r"Validation Error|SYNC-HAZARD|VUID-|failed assertion|Texture usage flags mismatch|Metal.*[Vv]alidation.*[Ee]rror", log) is not None


def load_scene():
    scene = json.loads(SCENE.read_text())
    for key in ["width", "height"]:
        if type(scene[key]) is not int or not 1 <= scene[key] <= 4096:
            raise ValueError("Invalid image size")
    if not 0 <= scene["channel_tolerance"] <= 255 or not 0 <= scene["max_different_fraction"] <= 1:
        raise ValueError("Invalid comparison thresholds")
    if not scene["vertices"] or not scene["draws"] or not scene["probes"]:
        raise ValueError("Empty test scene")
    for item in scene["vertices"]:
        for key, size in [("position", 3), ("color", 4), ("uv", 2)]:
            if len(item[key]) != size or not all(math.isfinite(v) for v in item[key]):
                raise ValueError("Invalid vertex")
    for draw in scene["draws"]:
        if draw["first"] < 0 or draw["count"] <= 0 or draw["count"] % 3 or draw["first"] + draw["count"] > len(scene["vertices"]):
            raise ValueError("Invalid draw range")
    for probe in scene["probes"]:
        if not (0 <= probe["x"] < scene["width"] and 0 <= probe["y"] < scene["height"]):
            raise ValueError("Probe outside image")
        if len(probe["rgba"]) != 4 or any(type(v) is not int or not 0 <= v <= 255 for v in probe["rgba"]):
            raise ValueError("Invalid probe color")
    texture = scene["texture"]
    if not 1 <= texture["width"] <= 4096 or not 1 <= texture["height"] <= 4096 or len(texture["rgba"]) != texture["width"] * texture["height"] * 4:
        raise ValueError("Invalid texture size")
    if any(type(v) is not int or not 0 <= v <= 255 for v in texture["rgba"]):
        raise ValueError("Invalid texture texel")
    return scene


def verify_shaders(scene, folder):
    identities = {}
    for stage, name in scene["shaders"].items():
        manifest = json.loads((folder / name).read_text())
        entry = {"vertex": "vsMain", "fragment": "psMain"}[stage]
        if manifest["version"] != 1 or manifest["stage"] != stage or manifest["entrypoint"] != entry:
            raise ValueError("Invalid shader manifest")
        for kind in ["spirv", "dxil", "msl"]:
            artifact = manifest["formats"][kind]
            if Path(artifact["file"]).name != artifact["file"] or artifact["entrypoint"] != entry:
                raise ValueError("Invalid shader artifact path/entrypoint")
            path = folder / artifact["file"]
            if path.stat().st_size != artifact["bytes"] or digest(path) != artifact["sha256"]:
                raise ValueError("Shader artifact integrity mismatch")
        identities[name] = digest(folder / name)
    return identities


def accept_capture(args, scene):
    if os.environ.get("CI") or not args.confirm_reference_update:
        raise ValueError("Reference update requires --confirm-reference-update outside CI")
    capture = Path(args.accept_capture)
    report = json.loads((capture / "result.json").read_text())
    if report.get("scene_sha256") != digest(SCENE) or not report.get("capture_complete") or not report.get("validation_enabled"):
        raise ValueError("Capture provenance, scene or validation is incomplete")
    width, height, rgba = read_png(capture / "actual.png")
    if (width, height) != (scene["width"], scene["height"]) or any(p["actual"] != p["rgba"] for p in probes(scene, rgba)):
        raise ValueError("Exact witness centers failed; cannot accept capture")
    if report["image_sha256"] != digest(capture / "actual.png"):
        raise ValueError("Capture was modified")
    REFERENCE.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(capture / "actual.png", REFERENCE)
    (REFERENCE.parent / "provenance.json").write_text(json.dumps({key: report[key] for key in
        ["backend", "device", "scene_sha256", "image_sha256", "shader_manifests"]}, indent=2) + "\n")
    print(f"Reference explicitly accepted: {REFERENCE}")
    return 0


def run(args, scene, output, report):
    executable = Path(args.executable).resolve()
    report["binary_sha256"] = digest(executable)
    if os.environ.get("GITHUB_SHA"):
        report["source_commit"] = os.environ["GITHUB_SHA"]
        report["ci_run"] = os.environ.get("GITHUB_RUN_ID")
    shaders = executable.parent / "assets/shaders"
    report["shader_manifests"] = verify_shaders(scene, shaders)
    report["scene_sha256"] = digest(SCENE)
    shutil.copyfile(SCENE, output / SCENE.name)
    shutil.copytree(shaders, output / "shaders", dirs_exist_ok=True)
    if REFERENCE.exists():
        shutil.copyfile(REFERENCE, output / "expected.png")
    env = os.environ.copy()
    if args.backend == "metal":
        env["MTL_DEBUG_LAYER"] = "1"
        env["MTL_SHADER_VALIDATION"] = "0" if args.metal_api_only_diagnostic else "1"
        env["MTL_SHADER_VALIDATION_REPORT_TO_STDERR"] = "1"
    process = subprocess.run([str(executable), args.backend, str(SCENE), str(output), str(shaders)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, env=env, timeout=120)
    log = process.stdout.decode("utf-8", errors="replace")
    (output / "validation.log").write_text(log, encoding="utf-8")
    print(log, flush=True)
    report["exit_code"] = process.returncode
    raw = output / "actual.rgba"
    if raw.exists():
        rgba = raw.read_bytes()
        write_png(output / "actual.png", scene["width"], scene["height"], rgba)
        report["image_sha256"] = digest(output / "actual.png")
        report["probes"] = probes(scene, rgba)
        if REFERENCE.exists():
            width, height, expected = read_png(REFERENCE)
            if (width, height) != (scene["width"], scene["height"]):
                raise ValueError("Reference dimensions mismatch")
            comparison, diff = compare(scene, rgba, expected)
            report["comparison"] = comparison
            write_png(output / "diff.png", width, height, diff)
    if process.returncode == 77 and "DEVICE_UNAVAILABLE:" in log:
        report.update(status="non validé", reason="Device unavailable; local capture and validation log required")
        return 0 if args.allow_unavailable else 1
    if process.returncode:
        raise RuntimeError("GPU execution or validation failed")
    if native_validation_error(log):
        raise RuntimeError("Native validation diagnostic in stderr")
    report["capture_complete"] = "D5_CAPTURE_COMPLETE" in log
    if not report["capture_complete"]:
        raise RuntimeError("Missing capture completion marker")
    device = json.loads((output / "device.json").read_text())
    if device["backend"] != args.backend or not device["debug"]:
        raise RuntimeError("Unexpected backend or validation disabled")
    report["device"] = device
    # SDL explicitly logs full validation activation for Vulkan and D3D12.
    # Metal must report both API and shader validation, not just an environment request.
    report["validation_enabled"] = ("Validation layers enabled" in log if args.backend != "metal"
                                     else "Metal API Validation Enabled" in log and "Metal GPU Validation Enabled" in log
                                     and not args.metal_api_only_diagnostic)
    if not REFERENCE.exists():
        raise RuntimeError("Reference missing; explicitly accept a validated capture outside CI")
    if not report.get("comparison", {}).get("passed"):
        raise RuntimeError("D5 image/probe comparison failed")
    if not report["validation_enabled"]:
        report.update(status="non validé", reason="Rendered correctly; full validation activation not proven")
        return 0 if args.allow_unavailable else 1
    report.update(status="validé", reason="Image, exact witnesses, resource lifetime and validation passed")
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--executable")
    parser.add_argument("--backend", choices=["vulkan", "direct3d12", "metal"])
    parser.add_argument("--output", type=Path)
    parser.add_argument("--allow-unavailable", action="store_true")
    parser.add_argument("--metal-api-only-diagnostic", action="store_true",
                        help="Diagnose Metal without shader instrumentation; never counts as validated D5")
    parser.add_argument("--accept-capture")
    parser.add_argument("--confirm-reference-update", action="store_true")
    args = parser.parse_args()
    if args.metal_api_only_diagnostic and args.backend != "metal":
        parser.error("--metal-api-only-diagnostic requires --backend metal")
    scene = load_scene()
    if args.accept_capture:
        return accept_capture(args, scene)
    if not args.executable or not args.backend or not args.output:
        parser.error("--executable, --backend and --output are required")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    for name in ["actual.rgba", "actual.png", "expected.png", "diff.png", "device.json", "result.json", "validation.log"]:
        (output / name).unlink(missing_ok=True)
    report = dict(backend=args.backend, status="échec")
    try:
        code = run(args, scene, output, report)
    except (ValueError, KeyError, OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        report["reason"] = str(error)
        code = 1
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2), flush=True)
    if path := os.environ.get("GITHUB_OUTPUT"):
        with open(path, "a", encoding="utf-8") as stream:
            stream.write("validation=" + report["status"] + "\n")
    if path := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(path, "a", encoding="utf-8") as stream:
            stream.write(f"## D5 {args.backend} : {report['status']}\n\n{report['reason']}\n")
    return code


if __name__ == "__main__":
    sys.exit(main())
