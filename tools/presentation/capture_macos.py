# Captures the real Metal swapchain/overlay on a macOS runner, separately from D5.
# A CI desktop capture is explicitly labelled CI and never substitutes for local hardware silently.
import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time

parser = argparse.ArgumentParser()
parser.add_argument("--executable", type=Path, required=True)
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
output = args.output.resolve()
output.mkdir(parents=True, exist_ok=True)
executable = args.executable.resolve()
executable.chmod(executable.stat().st_mode | 0o111)
helper = output / "macos-window"
subprocess.run(["swiftc", str(Path(__file__).with_name("macos_window.swift")), "-o", str(helper)], check=True)
environment = dict(os.environ, SDL_ASSERT="abort", SDL_LOGGING="*=verbose", MTL_DEBUG_LAYER="1",
                   MTL_SHADER_VALIDATION="1", MTL_SHADER_VALIDATION_REPORT_TO_STDERR="1")
result = {"origin": "GitHub Actions macOS runner" if os.getenv("CI") else "local macOS",
          "backend": "metal", "commit": os.getenv("GITHUB_SHA"), "status": "non validé",
          "binary_sha256": hashlib.sha256(executable.read_bytes()).hexdigest(), "captures": [], "actions": {}}

def run(*command):
    return subprocess.run([str(value) for value in command], capture_output=True, text=True, timeout=20)

with (output / "validation.log").open("w", encoding="utf-8") as log:
    process = subprocess.Popen([str(executable), "--backend", "metal"], cwd=output, env=environment,
                               stdout=log, stderr=subprocess.STDOUT)
    try:
        window = None
        for _ in range(50):
            if process.poll() is not None:
                raise RuntimeError(f"Sandbox exited before capture: {process.returncode}")
            query = run(helper, process.pid, "inspect")
            windows = json.loads(query.stdout) if query.returncode == 0 else []
            if windows:
                window = max(windows, key=lambda item: item.get("kCGWindowBounds", {}).get("Width", 0))
                break
            time.sleep(0.2)
        if not window:
            raise RuntimeError("No sandbox window available on the runner desktop")
        result["window"] = window
        time.sleep(1)

        def capture(name):
            path = output / f"{name}.png"
            command = run("screencapture", "-x", "-o", "-l", window["kCGWindowNumber"], path)
            if command.returncode or not path.exists():
                raise RuntimeError("Screen capture unavailable: " + command.stderr.strip())
            result["captures"].append(path.name)

        capture("triangle-overlay")
        for operation in ["resize", "minimize", "restore"]:
            action = run(helper, process.pid, operation)
            result["actions"][operation] = {"exit_code": action.returncode, "diagnostic": action.stderr.strip()}
            time.sleep(1)
            if action.returncode == 0:
                state = run(helper, process.pid, "inspect")
                result["actions"][operation]["windows"] = json.loads(state.stdout)
                if operation == "minimize":
                    observed = result["actions"][operation]["windows"]
                    result["actions"][operation]["observed_minimized"] = not any(item.get("kCGWindowIsOnscreen", False) for item in observed)
                    screenshot = run("screencapture", "-x", output / "minimized-desktop.png")
                    if screenshot.returncode == 0:
                        result["captures"].append("minimized-desktop.png")
                else:
                    capture(operation)
        complete = all(value["exit_code"] == 0 for value in result["actions"].values())
        complete = complete and result["actions"].get("minimize", {}).get("observed_minimized", False)
        result["status"] = "validé" if complete else "non validé"
    except (RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        result["reason"] = str(error)
    finally:
        if process.poll() is None:
            process.send_signal(signal.SIGINT) # SDL translates SIGINT into its normal quit event.
        try:
            result["exit_code"] = process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            result["exit_code"] = -1
            result["reason"] = "Sandbox failed to close after SDL quit event"

log = (output / "validation.log").read_text(encoding="utf-8")
errors = any(marker in log for marker in ["Validation Error", "Texture usage flags mismatch", "Assertion failure", "VUID-", "SYNC-HAZARD"])
if result["exit_code"] != 0 or errors:
    result["status"] = "échec"
result["validation_errors"] = errors
result["validation_enabled"] = "Metal API Validation Enabled" in log and "Metal GPU Validation Enabled" in log
if result["status"] == "validé" and not result["validation_enabled"]:
    result["status"] = "non validé"
    result["reason"] = "Metal validation markers missing from the presentation log"
(output / "result.json").write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
if os.getenv("GITHUB_STEP_SUMMARY"):
    with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
        summary.write(f"\nPrésentation Metal (capture **CI**) : **{result['status']}**. Voir presentation-metal/result.json.\n")
print(json.dumps(result, indent=2, ensure_ascii=False))
if result["status"] == "échec":
    sys.exit(1)
if result["status"] != "validé":
    print("::notice title=Présentation Metal non validée::Capture ou contrôle de fenêtre indisponible ; preuve locale encore requise.")
