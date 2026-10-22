"""Construct the glTF fixture and analytic ray/plane oracle without a GPU capture.
The same versioned scene supplies geometry, colors, camera, probes and error limits.
"""
import argparse
import hashlib
import io
import json
import math
from pathlib import Path
import struct
import tempfile
from compare import read_png, write_png

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "engine/renderer/tests/reference_scene.json"
FIXTURE = ROOT / "engine/assets/tests/fixtures/cube.glb"
REFERENCE = ROOT / "engine/renderer/tests/reference/cube.png"
PROVENANCE = REFERENCE.with_name("provenance.json")

def dot(a, b):
    return sum(x * y for x, y in zip(a, b))

def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]

def normalize(v):
    length = math.sqrt(dot(v, v))
    return [x / length for x in v]

def decode(v):
    c = v / 255
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

def encode(v):
    c = 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055
    return math.floor(min(1, max(0, c)) * 255 + 0.5)

def oracle(scene, mirrored=False):
    camera = scene["camera"]
    eye = camera["eye"]
    forward = normalize([b-a for a,b in zip(eye, camera["target"])])
    right = normalize(cross(forward, camera["up"]))
    up = cross(right, forward)
    tangent = math.tan(camera["vertical_fov"] / 2)
    light = normalize(scene["light"])
    size = scene["cube"]["half_extent"] * scene["cube"]["scale"]
    clear = bytes(round(v * 255) for v in scene["clear_rgba"])
    colors = {}
    scale = scene["mirror_scale"] if mirrored else [1,1,1]
    assert scale in [[1,1,1],[-1,1,1]], "This oracle proves the explicit X reflection"
    def normal(face): return [n*s for n,s in zip(face["normal"],scale)]
    for face in scene["faces"]:
        lambert = max(dot(normal(face), light), 0)
        color = scene["atlas"]["colors"][face["tile"]]
        colors[face["name"]] = bytes([encode(decode(c) * f * lambert)
            for c, f in zip(color[:3], scene["cube"]["base_color_factor"][:3])] + [255])
    pixels, labels = bytearray(), []
    witness_pixels = []
    for y in range(scene["height"]):
        for x in range(scene["width"]):
            u = (2 * (x + .5) / scene["width"] - 1) * tangent * scene["width"] / scene["height"]
            v = (1 - 2 * (y + .5) / scene["height"]) * tangent
            ray = [f + u*r + v*t for f,r,t in zip(forward,right,up)]
            hits = []
            for face in scene["faces"]:
                n = normal(face)
                denominator = dot(n, ray)
                if denominator >= 0: continue  # back-face culling
                distance = (size - dot(n, eye)) / denominator
                point = [a + distance*b for a,b in zip(eye,ray)]
                if camera["near"] <= distance <= camera["far"] and all(abs(c) <= size + 1e-10 for c in point):
                    hits.append((distance, face["name"]))
            witness = scene["depth_witness"]
            t = (witness["z"] * scene["cube"]["scale"] - eye[2]) / ray[2]
            p = [a+t*b for a,b in zip(eye,ray)]
            if abs(p[0]) < witness["half_extent"] * scene["cube"]["scale"] and abs(p[1]) < witness["half_extent"] * scene["cube"]["scale"]:
                witness_pixels.append((x,y))
                hits.append((t, "witness"))
            label = min(hits)[1] if hits else "background"
            if label == "witness": raise ValueError("Depth witness must be completely hidden")
            labels.append(label)
            pixels.extend(colors.get(label, clear))
    for probe in scene["probes"]:
        x,y = probe["x"],probe["y"]
        index = y * scene["width"] + x
        assert list(pixels[index*4:index*4+4]) == probe["rgba"], (probe, list(pixels[index*4:index*4+4]))
        face = "left" if mirrored and probe["face"] == "right" else probe["face"]
        assert all(labels[(y+dy)*scene["width"]+x+dx] == face for dy in range(-4,5) for dx in range(-4,5)), probe
        if probe["name"] == "occluded_late_draw": assert (x,y) in witness_pixels, probe
    assert set(labels) == {"background", "front", "left" if mirrored else "right", "top"}
    return bytes(pixels)

def fixture(scene, directory):
    atlas = scene["atlas"]
    width, height = atlas["columns"]*atlas["cell_pixels"], atlas["rows"]*atlas["cell_pixels"]
    texels = bytes(c for y in range(height) for x in range(width)
        for c in atlas["colors"][(y//atlas["cell_pixels"])*atlas["columns"]+x//atlas["cell_pixels"]])
    png = directory / "atlas.png"
    write_png(png, width, height, texels)
    data = bytearray()
    views, accessors = [], []
    def append(payload, stride=None):
        while len(data)%4: data.append(0)
        index = len(views)
        views.append(dict(buffer=0, byteOffset=len(data), byteLength=len(payload)))
        if stride: views[-1]["byteStride"] = stride
        data.extend(payload)
        return index
    def quad(normal, center, u, v, extent, uv, bits, material):
        vertices = []
        for (a,b), tex in zip([(-1,-1),(1,-1),(1,1),(-1,1)], uv):
            vertices.append([center[i]+extent*(a*u[i]+b*v[i]) for i in range(3)] + normal + tex)
        view = append(b"".join(struct.pack("<8f", *vertex) for vertex in vertices), 32)
        first = len(accessors)
        for offset,kind in [(0,"VEC3"),(12,"VEC3"),(24,"VEC2")]:
            accessors.append(dict(bufferView=view, byteOffset=offset, componentType=5126, count=4, type=kind))
        accessors[first].update(min=[min(p[i] for p in vertices) for i in range(3)], max=[max(p[i] for p in vertices) for i in range(3)])
        index_view = append(struct.pack("<6" + ("H" if bits == 16 else "I"), 0,1,2,0,2,3))
        accessors.append(dict(bufferView=index_view, componentType=5123 if bits==16 else 5125, count=6, type="SCALAR"))
        return dict(attributes=dict(POSITION=first,NORMAL=first+1,TEXCOORD_0=first+2), indices=first+3, material=material, mode=4)
    primitives, materials = [], []
    for i, face in enumerate(scene["faces"]):
        col,row = face["tile"]%atlas["columns"], face["tile"]//atlas["columns"]
        u0,u1 = (col*atlas["cell_pixels"]+.5)/width, ((col+1)*atlas["cell_pixels"]-.5)/width
        v0,v1 = (row*atlas["cell_pixels"]+.5)/height, ((row+1)*atlas["cell_pixels"]-.5)/height
        half = scene["cube"]["half_extent"]
        primitives.append(quad(face["normal"], [n*half for n in face["normal"]], face["u"],face["v"], half,
            [[u0,v1],[u1,v1],[u1,v0],[u0,v0]],face["index_bits"],i))
        materials.append(dict(pbrMetallicRoughness=dict(baseColorFactor=scene["cube"]["base_color_factor"],baseColorTexture=dict(index=0))))
    witness = scene["depth_witness"]
    plane = quad([0,0,1],[0,0,witness["z"]],[1,0,0],[0,1,0],witness["half_extent"],[[0,0]]*4,32,len(materials))
    materials.append(dict(pbrMetallicRoughness=dict(baseColorFactor=witness["base_color_factor"])))
    image_view = append(png.read_bytes())
    model = dict(asset=dict(version="2.0",generator="Somnium analytic cube fixture"),
        buffers=[dict(byteLength=len(data))],bufferViews=views,accessors=accessors,
        images=[dict(bufferView=image_view,mimeType="image/png")], textures=[dict(source=0,sampler=0)],
        samplers=[dict(magFilter=9728,minFilter=9728,wrapS=33071,wrapT=33071)], materials=materials,
        meshes=[dict(primitives=primitives),dict(primitives=[plane])],
        nodes=[dict(scale=[scene["cube"]["scale"]]*3,children=[2,1]),dict(mesh=0),dict(mesh=1)],
        scenes=[dict(nodes=[0])],scene=0)
    text = json.dumps(model,separators=(",",":")).encode()
    text += b" "*((-len(text))%4)
    data.extend(b"\0"*((-len(data))%4))
    return struct.pack("<III",0x46546c67,2,12+8+len(text)+8+len(data)) + struct.pack("<II",len(text),0x4e4f534a) + text + struct.pack("<II",len(data),0x004e4942) + data

def digest(path): return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    parser = argparse.ArgumentParser()
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify", action="store_true")
    mode.add_argument("--confirm-regeneration", action="store_true")
    args = parser.parse_args()
    scene = json.loads(SCENE.read_text())
    pixels = oracle(scene)
    assert oracle(scene, mirrored=True) == pixels, "The reflected cube must have the same analytic image"
    with tempfile.TemporaryDirectory() as tmp:
        directory = Path(tmp)
        glb = fixture(scene, directory)
        path = directory / "cube.png"
        write_png(path,scene["width"],scene["height"],pixels)
        files = {SCENE, Path(__file__), ROOT/"assets/shaders/model.vert.hlsl", ROOT/"assets/shaders/model.frag.hlsl", ROOT/"assets/shaders/color.hlsli", ROOT/"assets/shaders/conventions.hlsli"}
        provenance = dict(method="Independent double precision ray/plane intersections; IEC 61966-2-1 sRGB, Lambert 0/1; never GPU capture",
            inputs={p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes().replace(b"\r\n",b"\n")).hexdigest() for p in sorted(files)},
            fixture_sha256=hashlib.sha256(glb).hexdigest(), image_sha256=digest(path))
        if args.verify:
            assert FIXTURE.read_bytes() == glb, "Cube fixture differs; explicit regeneration required"
            assert read_png(REFERENCE) == (scene["width"], scene["height"], pixels), "Analytic reference differs"
            assert json.loads(PROVENANCE.read_text()) == provenance, "Provenance differs"
        else:
            REFERENCE.parent.mkdir(parents=True,exist_ok=True)
            FIXTURE.write_bytes(glb)
            REFERENCE.write_bytes(path.read_bytes())
            PROVENANCE.write_text(json.dumps(provenance,indent=2)+"\n",encoding="utf-8",newline="\n")
    print("Cube analytic fixture, reference, exact probes and provenance verified")

if __name__ == "__main__": main()
