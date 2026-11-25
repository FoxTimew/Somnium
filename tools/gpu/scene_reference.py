"""Independent double-precision ray/plane oracle for the serialized pivot scene.
Only explicit regeneration writes references; GPU captures are never an input.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import tempfile
from compare import read_png, write_png
from cube_reference import dot, normalize, decode, encode, fixture

ROOT = Path(__file__).resolve().parents[2]
SCENE = ROOT / "engine/runtime/tests/reference_scene.json"
REFERENCE = ROOT / "engine/runtime/tests/reference/actual.png"
HIERARCHY = SCENE.with_name("hierarchy.json")
CUBE = SCENE.with_name("fixture_geometry.json")
FIXTURE = SCENE.parent / "fixtures/hierarchy.glb"

def rotate(v, axis, angle):
    c, s = math.cos(angle), math.sin(angle)
    x, y, z = v
    if axis == "y": return [c*x+s*z, y, -s*x+c*z]
    return [c*x-s*y, s*x+c*y, z]

def oracle(config, pose):
    document = json.loads(HIERARCHY.read_text())
    cube = json.loads(CUBE.read_text())
    entities = {e["id"]: {k.removeprefix("somnium."):v["data"] for k,v in e["components"].items()}
                for e in document["entities"]}
    child, pivot = entities["child"], entities[config["pivot"]]
    camera = entities[document["activeCamera"]]
    # The oracle deliberately supports the declared closed-form scene, not arbitrary ECS math.
    assert child["Hierarchy"]["parent"] == config["pivot"]
    assert pivot["Transform"]["rotationDegrees"] == [0,0,0]
    assert pivot["Transform"]["scale"] == [1,1,1]
    assert child["Transform"]["rotationDegrees"][:2] == [0,0]
    assert camera["Transform"]["rotationDegrees"] == [0,0,0]
    assert config["next_rotation_degrees"] == [0,90,0]
    assert entities["light"]["DirectionalLight"]["direction"] == [0,0,1]
    assert document["settings"]["clearColor"] == [0,0,0,1]
    theta = math.radians(config["next_rotation_degrees"][1]) * pose["alpha"]
    phi = math.radians(child["Transform"]["rotationDegrees"][2])
    center = [a+b for a,b in zip(pivot["Transform"]["position"],rotate(child["Transform"]["position"],"y",theta))]
    scale = [s*cube["cube"]["scale"] for s in child["Transform"]["scale"]]
    def inverse(v): return [c/s for c,s in zip(rotate(rotate(v,"y",-theta),"z",-phi),scale)]
    def normal(n): return normalize(rotate(rotate([c/s for c,s in zip(n,scale)],"z",phi),"y",theta))
    origin = inverse([a-b for a,b in zip(camera["Transform"]["position"],center)])
    tangent = math.tan(math.radians(camera["Camera"]["verticalFovDegrees"])/2)
    colors = {}
    for face in cube["faces"]:
        strength = max(normal(face["normal"])[2],0) + document["settings"]["ambient"]
        texel = cube["atlas"]["colors"][face["tile"]]
        colors[face["name"]] = bytes([encode(decode(c)*f*strength) for c,f in
            zip(texel[:3],cube["cube"]["base_color_factor"][:3])] + [255])
    strength=max(normal([0,0,1])[2],0)+document["settings"]["ambient"]
    colors["witness"]=bytes([encode(f*strength) for f in cube["depth_witness"]["base_color_factor"][:3]]+[255])
    pixels, labels = bytearray(), []
    for y in range(config["height"]):
        for x in range(config["width"]):
            u=(2*(x+.5)/config["width"]-1)*tangent*config["width"]/config["height"]
            v=(1-2*(y+.5)/config["height"])*tangent
            ray=inverse([u,v,-1])
            hits=[]
            for face in cube["faces"]:
                denominator=dot(face["normal"],ray)
                if denominator >= -1e-12: continue
                distance=(cube["cube"]["half_extent"]-dot(face["normal"],origin))/denominator
                point=[a+distance*b for a,b in zip(origin,ray)]
                if camera["Camera"]["near"] <= distance <= camera["Camera"]["far"] and all(abs(c)<=cube["cube"]["half_extent"]+1e-10 for c in point):
                    hits.append((distance,face["name"]))
            witness=cube["depth_witness"]
            if ray[2] < -1e-12:
                distance=(witness["z"]-origin[2])/ray[2]
                point=[a+distance*b for a,b in zip(origin,ray)]
                if distance>0 and abs(point[0])<witness["half_extent"] and abs(point[1])<witness["half_extent"]:
                    hits.append((distance,"witness"))
            label=min(hits)[1] if hits else "background"
            if pose["alpha"] < 1: assert label != "witness", "Witness must be occluded in the first two poses"
            labels.append(label)
            pixels.extend(colors.get(label,bytes([0,0,0,255])))
    for probe in pose["probes"]:
        x,y=probe["x"],probe["y"]
        i=y*config["width"]+x
        assert list(pixels[4*i:4*i+4])==probe["rgba"], probe
        assert all(labels[(y+dy)*config["width"]+x+dx]==probe["face"]
                   for dy in range(-3,4) for dx in range(-3,4)), probe
    return bytes(pixels), labels

def main():
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify",action="store_true")
    mode.add_argument("--confirm-regeneration",action="store_true")
    args=parser.parse_args()
    config=json.loads(SCENE.read_text())
    files=[SCENE,HIERARCHY,CUBE,Path(__file__),ROOT/"tools/gpu/cube_reference.py",
           *[ROOT/"assets/shaders"/name for name in ["model.vert.hlsl","model.frag.hlsl","color.hlsli","conventions.hlsli"]]]
    with tempfile.TemporaryDirectory() as folder:
        glb=fixture(json.loads(CUBE.read_text()),Path(folder))
    if args.verify: assert FIXTURE.read_bytes()==glb, "Scene fixture differs"
    else:
        FIXTURE.parent.mkdir(parents=True,exist_ok=True)
        FIXTURE.write_bytes(glb)
    provenance={"method":"Independent closed-form Ry(pivot) * Rz(child), inverse rays/planes and IEC sRGB; never a GPU capture",
                "inputs":{p.relative_to(ROOT).as_posix():hashlib.sha256(p.read_bytes().replace(b"\r\n",b"\n")).hexdigest() for p in files},
                "fixture_sha256":hashlib.sha256(glb).hexdigest(),"images":{}}
    with tempfile.TemporaryDirectory() as temporary:
        for pose in config["poses"]:
            assert pose["probes"], "Every pose requires exact probes"
            pixels,_=oracle(config,pose)
            path=Path(temporary)/(pose["name"]+".png")
            write_png(path,config["width"],config["height"],pixels)
            provenance["images"][path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
            destination=REFERENCE.parent/path.name
            if args.verify:
                assert read_png(destination)==(config["width"],config["height"],pixels), "Analytic reference differs"
            else:
                destination.parent.mkdir(parents=True,exist_ok=True)
                destination.write_bytes(path.read_bytes())
    path=REFERENCE.with_name("provenance.json")
    if args.verify: assert json.loads(path.read_text())==provenance, "Scene provenance differs"
    else: path.write_text(json.dumps(provenance,indent=2)+"\n",encoding="utf-8",newline="\n")
    print("Scene hierarchy, three analytic poses, exact probes and provenance verified")

if __name__=="__main__": main()
