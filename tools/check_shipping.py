# Verifies the generated shipping build graph and compilation units exclude every UI target.
# Run after configure, independently of whether the executable happens to link those objects.
import argparse
import json
from pathlib import Path
import re
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("build", type=Path)
args = parser.parse_args()
targets = subprocess.run(["cmake", "--build", str(args.build), "--target", "help"],
                         check=True, capture_output=True, text=True).stdout
if re.search(r"somnium_(?:imgui|platform_imgui|gpu_imgui|debug_ui)\b", targets):
    raise SystemExit("Shipping contains a forbidden UI target")
commands = json.loads((args.build / "compile_commands.json").read_text(encoding="utf-8"))
if "SOMNIUM_DATA_MODE:STRING=COOKED" not in (args.build / "CMakeCache.txt").read_text(encoding="utf-8"):
    raise SystemExit("Shipping must use a single cooked data root")
for command in commands:
    source = Path(command["file"]).as_posix().lower()
    if "SOMNIUM_DEVELOPMENT_ASSETS" in command.get("command", " ".join(command.get("arguments", []))):
        raise SystemExit("Shipping contains the development source root")
    if "imgui" in source or "/debug_ui/" in source or "/integration/" in source:
        raise SystemExit(f"Shipping compiles an optional UI source: {source}")
print("Shipping graph: no ImGui, platform/gpu integration or debug_ui target/source")
