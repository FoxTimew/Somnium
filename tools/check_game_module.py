"""Checks the generated module link inputs and actual shared-library dependencies.
Header templates are permitted; engine implementation objects and archives are not.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import platform
import re
import subprocess


def validate(target: dict) -> None:
    if target.get("type") != "MODULE_LIBRARY":
        raise ValueError("Game must be a module library")
    for source in target.get("sources", []):
        name = source["path"].replace("\\", "/")
        if re.search(r"(?:^|/)engine/", name):
            raise ValueError("Game embeds an engine implementation source/object: " + name)
    fragments = " ".join(item["fragment"] for item in target.get("link", {}).get("commandFragments", []))
    if "somnium_engine_shared" not in fragments:
        raise ValueError("Game does not import the shared engine")
    for name in re.findall(r"[^\s\"']+", fragments.replace("\\", "/")):
        base = name.rsplit("/", 1)[-1]
        if re.search(r"(?:^|/)engine/.*\.(?:o|obj)(?:$|[;,])", name):
            raise ValueError("Game embeds an engine object: " + name)
        if re.match(r"(?:lib)?somnium_(?:core|platform|gpu|renderer|assets|scene|input|debug_ui|runtime)(?:\.|_imgui\.)", base):
            raise ValueError("Game embeds a logical engine archive: " + name)
    if "WHOLEARCHIVE" in fragments.upper() or "--whole-archive" in fragments:
        raise ValueError("Game must not aggregate engine archives")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    arguments = parser.parse_args()
    reply = arguments.build / ".cmake/api/v1/reply"
    index = json.loads(max(reply.glob("index-*.json"), key=lambda path: path.stat().st_mtime).read_text())
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text())
    checked = 0
    for config in model["configurations"]:
        for item in config["targets"]:
            if item["name"] != "somnium_game_module":
                continue
            target = json.loads((reply / item["jsonFile"]).read_text())
            validate(target)
            artifact = arguments.build / target["artifacts"][0]["path"]
            if artifact.suffix == ".dll":
                command = ["dumpbin", "/DEPENDENTS", str(artifact)]
            elif platform.system() == "Darwin":
                command = ["otool", "-L", str(artifact)]
            else:
                command = ["readelf", "-d", str(artifact)]
            dependencies = subprocess.check_output(command, text=True, encoding="utf-8", errors="replace")
            if "somnium_engine_shared" not in dependencies:
                raise ValueError("Module binary does not import the shared engine")
            checked += 1
    if checked == 0:
        raise ValueError("No game module in generated codemodel")
    print("Game module: generated sources/link inputs and binary import verified; no engine objects")


if __name__ == "__main__":
    main()
