"""Generate Somnium's original CC0 courtyard models, with explicit regeneration.
Boxes are authored analytically; no downloaded model, reference image or GPU result is used.
"""
import argparse
import json
from pathlib import Path
import struct
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/gpu"))
from compare import write_png

# Each face has cross(u,v) == outward normal, matching CCW front faces.
FACES = [([0,0,1],[1,0,0],[0,1,0]), ([0,0,-1],[-1,0,0],[0,1,0]),
         ([1,0,0],[0,0,-1],[0,1,0]), ([-1,0,0],[0,0,1],[0,1,0]),
         ([0,1,0],[1,0,0],[0,0,-1]), ([0,-1,0],[1,0,0],[0,0,1])]


def model(parts, checker=False):
    data, views, accessors, primitives, materials = bytearray(), [], [], [], []
    def append(payload, stride=None):
        data.extend(b"\0" * ((-len(data)) % 4))
        view = dict(buffer=0, byteOffset=len(data), byteLength=len(payload))
        if stride:
            view["byteStride"] = stride
        views.append(view)
        data.extend(payload)
        return len(views)-1
    for center, size, color in parts:
        vertices, indices = [], []
        for normal, u, v in FACES:
            first = len(vertices)
            for (a,b), uv in zip([(-1,-1),(1,-1),(1,1),(-1,1)],[(0,1),(1,1),(1,0),(0,0)]):
                position = [center[i] + .5 * size[i] * (normal[i]+a*u[i]+b*v[i]) for i in range(3)]
                vertices.append(position + normal + [t * (12 if checker else 1) for t in uv])
            indices.extend(first+i for i in [0,1,2,0,2,3])
        view = append(b"".join(struct.pack("<8f",*vertex) for vertex in vertices),32)
        first = len(accessors)
        for offset, kind in [(0,"VEC3"),(12,"VEC3"),(24,"VEC2")]:
            accessors.append(dict(bufferView=view,byteOffset=offset,componentType=5126,count=len(vertices),type=kind))
        accessors[first].update(min=[min(vertex[i] for vertex in vertices) for i in range(3)],max=[max(vertex[i] for vertex in vertices) for i in range(3)])
        index_view = append(struct.pack("<"+"H"*len(indices),*indices))
        accessors.append(dict(bufferView=index_view,componentType=5123,count=len(indices),type="SCALAR"))
        primitives.append(dict(attributes=dict(POSITION=first,NORMAL=first+1,TEXCOORD_0=first+2),indices=first+3,material=len(materials),mode=4))
        material = dict(pbrMetallicRoughness=dict(baseColorFactor=color))
        if checker:
            material["pbrMetallicRoughness"]["baseColorTexture"] = dict(index=0)
        materials.append(material)
    result = dict(asset=dict(version="2.0",generator="Somnium original CC0 courtyard boxes"),
                  bufferViews=views,accessors=accessors,materials=materials,
                  meshes=[dict(primitives=primitives)],nodes=[dict(mesh=0)],scenes=[dict(nodes=[0])],scene=0)
    if checker:
        with tempfile.TemporaryDirectory() as folder:
            png = Path(folder)/"checker.png"
            write_png(png,2,2,bytes([185,194,205,255,128,142,160,255,128,142,160,255,185,194,205,255]))
            image_view=append(png.read_bytes())
        result.update(images=[dict(bufferView=image_view,mimeType="image/png")],textures=[dict(source=0,sampler=0)],
                      samplers=[dict(magFilter=9728,minFilter=9728,wrapS=10497,wrapT=10497)])
    result["buffers"]=[dict(byteLength=len(data))]
    text=json.dumps(result,separators=(",",":")).encode()
    text+=b" "*((-len(text))%4); data.extend(b"\0"*((-len(data))%4))
    return struct.pack("<III",0x46546c67,2,28+len(text)+len(data))+struct.pack("<II",len(text),0x4e4f534a)+text+struct.pack("<II",len(data),0x004e4942)+data


def generated():
    box = lambda color: [([0,0,0],[1,1,1],color)]
    character = [([0,.95,0],[.52,.65,.3],[.035,.42,.55,1]),
        ([0,1.5,0],[.4,.4,.38],[.95,.65,.14,1]),
        ([-.17,.32,0],[.2,.64,.25],[.06,.1,.22,1]),([.17,.32,0],[.2,.64,.25],[.06,.1,.22,1]),
        ([-.38,.95,0],[.18,.64,.2],[.035,.42,.55,1]),([.38,.95,0],[.18,.64,.2],[.035,.42,.55,1]),
        ([-.09,1.54,-.195],[.065,.065,.025],[.015,.02,.03,1]),([.09,1.54,-.195],[.065,.065,.025],[.015,.02,.03,1])]
    return {"courtyard-ground.glb":model(box([1,1,1,1]),True),
            "courtyard-wall.glb":model(box([.5,.58,.67,1])),
            "courtyard-door.glb":model(box([.62,.19,.055,1])),
            "courtyard-character.glb":model(character)}


def main():
    parser=argparse.ArgumentParser()
    mode=parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--verify",action="store_true")
    mode.add_argument("--confirm-regeneration",action="store_true")
    args=parser.parse_args()
    for name,data in generated().items():
        path=ROOT/"assets/models"/name
        if args.verify:
            if path.read_bytes()!=data:
                raise RuntimeError(f"Original asset differs: {name}; explicit regeneration required")
        else:
            path.write_bytes(data)
    print("Original CC0 courtyard models verified")

if __name__=="__main__":
    main()
