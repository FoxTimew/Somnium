# Verifies shipping's generated graph, options and compiled sources, not just dead stripping.
# Development iteration services and the standalone spike must never enter this build.
import argparse
import json
from pathlib import Path
import re
import subprocess

FORBIDDEN_TARGET = re.compile(
    r"\b(?:spike_\w+|somnium_(?:imgui|platform_imgui|gpu_imgui|debug_ui|iteration\w*|"
    r"editor\w*|file_watch\w*|shader_reload\w*|game_module\w*|module_loader\w*|engine_shared))\b")
ITERATION_OPTIONS = ("SOMNIUM_ENABLE_ITERATION", "SOMNIUM_ENABLE_SHADER_RELOAD",
                     "SOMNIUM_ENABLE_GAMEPLAY_RELOAD")
FORBIDDEN_SOURCE = re.compile(
    r"(?:imgui|/debug_ui/|/integration/|inspector|/hot_reload_spike/|"
    r"/(?:file_watch|module_loader|shader_reload|edit_session|editor_camera|scene_editor)[^/]*\.)")


def validate(targets, commands, cache):
    if FORBIDDEN_TARGET.search(targets):
        raise ValueError("Shipping contains a forbidden UI/iteration/spike target")
    options = dict(re.findall(r"^([^/#\n][^:\n]*):[^=\n]+=(.*)$", cache, re.MULTILINE))
    if options.get("SOMNIUM_DATA_MODE", "").strip() != "COOKED":
        raise ValueError("Shipping must use a single cooked data root")
    for name in ("SOMNIUM_ENABLE_DEBUG_UI", *ITERATION_OPTIONS):
        if options.get(name, "OFF").strip().upper() not in ("OFF", "FALSE", "0", "NO", "N", ""):
            raise ValueError(f"Shipping enables {name}")
    for command in commands:
        source = command["file"].replace("\\", "/").lower()
        flags = command.get("command", " ".join(command.get("arguments", [])))
        if "SOMNIUM_DEVELOPMENT_ASSETS" in flags:
            raise ValueError("Shipping contains the development source root")
        for name in ITERATION_OPTIONS:
            if re.search(r"(?:-D|/D)\s*" + name + r"(?:\b|=)", flags):
                raise ValueError(f"Shipping compiles with iteration definition {name}")
        if FORBIDDEN_SOURCE.search(source):
            raise ValueError(f"Shipping compiles an optional UI/iteration source: {source}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    targets = subprocess.run(["cmake", "--build", str(args.build), "--target", "help"],
                             check=True, capture_output=True, text=True).stdout
    commands = json.loads((args.build / "compile_commands.json").read_text(encoding="utf-8"))
    cache = (args.build / "CMakeCache.txt").read_text(encoding="utf-8")
    try:
        validate(targets, commands, cache)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    print("Shipping graph: no UI, inspector, iteration services, shared engine/game module or spike; cooked root only")


if __name__ == "__main__":
    main()
