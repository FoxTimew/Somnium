"""Collect independent Metal diagnostics after a failed required D5 execution.
Their results cannot change the primary job's failure or validate a reference.
"""
import json
import os
from pathlib import Path
import subprocess
import sys

output = Path(".cache/gpu-validation/metal-diagnostic")
output.mkdir(parents=True, exist_ok=True)
native = output / "native-probe"
subprocess.run(["xcrun", "clang++", "-std=c++20", "-fobjc-arc", "-Wall", "-Wextra", "-Werror",
                "-mmacosx-version-min=11.0", "-framework", "Foundation", "-framework", "Metal",
                "tools/gpu/metal_validation_probe.mm", "-o", str(native)], check=True)
results = {}
for mode in ["full", "api-only"]:
    env = os.environ.copy()
    env.update(MTL_DEBUG_LAYER="1", MTL_SHADER_VALIDATION="1" if mode == "full" else "0",
               MTL_SHADER_VALIDATION_REPORT_TO_STDERR="1")
    process = subprocess.run([str(native.resolve())], env=env, stdout=subprocess.PIPE,
                             stderr=subprocess.STDOUT, timeout=120)
    log = process.stdout.decode("utf-8", errors="replace")
    (output / f"native-{mode}.log").write_text(log)
    results[mode] = {"exit_code": process.returncode, "exact": "NATIVE_TEXTURE_EXACT" in log}
    print(mode, process.returncode, log, flush=True)
env = os.environ.copy()
env.pop("GITHUB_OUTPUT", None)
env.pop("GITHUB_STEP_SUMMARY", None)
process = subprocess.run([sys.executable, "tools/gpu/run.py", "--backend", "metal",
    "--executable", ".cache/gpu-bin/somnium_gpu_offscreen", "--output", str(output / "api-only-d5"),
    "--metal-api-only-diagnostic"], env=env, timeout=150)
results["api-only-d5"] = {"exit_code": process.returncode, "status": "diagnostic, non validé"}
(output / "diagnosis.json").write_text(json.dumps(results, indent=2) + "\n")
