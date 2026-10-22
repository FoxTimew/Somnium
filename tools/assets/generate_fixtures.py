"""Generate small, deterministic CPU import fixtures, never captured from a renderer.
Binary fixtures live beside module tests and are tracked with Git LFS.
"""
from pathlib import Path
import argparse
import base64
import json
import math
import struct
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "gpu"))
from compare import write_png


def generate(folder):
    folder.mkdir(parents=True, exist_ok=True)
    png = folder / "texture.png"
    write_png(png, 2, 2, bytes([255, 0, 0, 255, 0, 255, 0, 255,
                              0, 0, 255, 255, 255, 255, 255, 255]))
    positions = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
    uvs = [(0, 0), (1, 0), (0, 1)]
    binary = b"".join(struct.pack("<8f", *position, 0, 0, 1, *uv) for position, uv in zip(positions, uvs))
    binary += struct.pack("<3H", 0, 1, 2) + b"\0\0" + struct.pack("<3I", 0, 1, 2)
    (folder / "mesh.bin").write_bytes(binary)
    primitive = {"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "indices": 3, "material": 0, "mode": 4}
    document = {
        "asset": {"version": "2.0", "generator": "Somnium analytic CPU fixture"},
        "buffers": [{"uri": "mesh.bin", "byteLength": len(binary)}],
        "bufferViews": [{"buffer": 0, "byteLength": 96, "byteStride": 32},
                        {"buffer": 0, "byteOffset": 96, "byteLength": 6},
                        {"buffer": 0, "byteOffset": 104, "byteLength": 12}],
        "accessors": [{"bufferView": 0, "byteOffset": offset, "componentType": 5126, "count": 3, "type": kind}
                      for offset, kind in [(0, "VEC3"), (12, "VEC3"), (24, "VEC2")]] +
                     [{"bufferView": 1, "componentType": 5123, "count": 3, "type": "SCALAR"},
                      {"bufferView": 2, "componentType": 5125, "count": 3, "type": "SCALAR"}],
        "images": [{"uri": "texture.png"}],
        "samplers": [{"minFilter": 9728, "magFilter": 9729, "wrapS": 33071, "wrapT": 10497}],
        "textures": [{"source": 0, "sampler": 0}],
        "materials": [{"pbrMetallicRoughness": {"baseColorFactor": [0.5, 0.25, 1, 1], "baseColorTexture": {"index": 0}}}],
        "meshes": [{"primitives": [primitive, dict(primitive, indices=4)]},
                   {"primitives": [{key: value for key, value in primitive.items() if key != "material"}]}],
        "nodes": [{"translation": [1, 2, 3], "rotation": [0, 0, math.sqrt(0.5), math.sqrt(0.5)], "children": [1]},
                  {"translation": [4, 0, 0], "scale": [2, 3, 4], "mesh": 0}, {"mesh": 1, "translation": [-2, 0, 0]}],
        "scenes": [{"nodes": [0, 2]}], "scene": 0,
    }
    document["accessors"][0].update(min=[0, 0, 0], max=[1, 1, 0])
    (folder / "model.gltf").write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")
    data_document = json.loads(json.dumps(document))
    data_document["buffers"][0]["uri"] = "data:application/octet-stream;base64," + base64.b64encode(binary).decode()
    data_document["images"][0]["uri"] = "data:image/png;base64," + base64.b64encode(png.read_bytes()).decode()
    (folder / "embedded.gltf").write_text(json.dumps(data_document, indent=2) + "\n", encoding="utf-8")
    embedded = binary + png.read_bytes()
    document["buffers"] = [{"byteLength": len(embedded)}]
    document["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": png.stat().st_size})
    document["images"] = [{"bufferView": 3, "mimeType": "image/png"}]
    text = json.dumps(document, separators=(",", ":")).encode()
    text += b" " * (-len(text) % 4)
    embedded += b"\0" * (-len(embedded) % 4)
    glb = struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(text) + 8 + len(embedded))
    glb += struct.pack("<II", len(text), 0x4E4F534A) + text
    glb += struct.pack("<II", len(embedded), 0x004E4942) + embedded
    (folder / "model.glb").write_bytes(glb)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--confirm-regeneration", action="store_true", required=True)
    parser.parse_args()
    generate(Path(__file__).resolve().parents[2] / "engine/assets/tests/fixtures")
