# Derives the reload oracle analytically, independently of GPU output or shader execution.
# Regeneration is explicit and forbidden in CI; existing D5/cube/scene references are untouched.
import argparse
import hashlib
import json
import os
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/gpu"))
from compare import read_png, write_png

SCENE = ROOT / "engine/renderer/tests/reload_scene.json"
REFERENCE = SCENE.parent / "reference"


def expected(scene, color):
    left, top, right, bottom = scene["rectangle_pixels"]
    return bytes(channel for y in range(scene["height"]) for x in range(scene["width"])
                 for channel in (color if left <= x < right and top <= y < bottom else scene["clear_rgba"]))


def verify(regenerate=False):
    if regenerate and os.getenv("CI"):
        raise ValueError("Reference regeneration is forbidden in CI")
    scene = json.loads(SCENE.read_text())
    provenance = {"version": 1, "generator": "analytic pixel-aligned rectangle, axis-aligned Lambert = 1",
                  "scene_sha256": hashlib.sha256(SCENE.read_bytes()).hexdigest(), "images": {}}
    for name, color in [("white", "initial_rgba"), ("red", "replacement_rgba")]:
        path = REFERENCE / f"shader_reload_{name}.png"
        pixels = expected(scene, scene[color])
        if regenerate:
            write_png(path, scene["width"], scene["height"], pixels)
        width, height, actual = read_png(path)
        if (width, height) != (scene["width"], scene["height"]) or actual != pixels:
            raise ValueError(f"Analytical reference differs: {path}")
        provenance["images"][path.name] = hashlib.sha256(path.read_bytes()).hexdigest()
    path = REFERENCE / "shader_reload_provenance.json"
    if regenerate:
        path.write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
    if json.loads(path.read_text()) != provenance:
        raise ValueError("Reload reference provenance differs")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify", action="store_true")
    mode.add_argument("--regenerate", action="store_true")
    verify(parser.parse_args().regenerate)
