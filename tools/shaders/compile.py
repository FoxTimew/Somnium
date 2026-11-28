# Compiles one shader variant and publishes complete, relocatable artifacts atomically.
# DXC supplies the transitive include depfile; shadercross reflection owns resource counts.
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
from reflection import canonical_bytes, normalize


def normalize_depfile(depfile, target):
    # DXC v1.9.2609 dxcompilerobj.cpp:882-893 emits one *unescaped* dependency
    # per continued line. Escape that pinned format before Ninja/CMake consume it.
    raw = depfile.read_text(encoding="utf-8")
    _, separator, dependencies = raw.partition(": ")
    if not separator:
        raise ValueError("Unexpected DXC depfile format")

    def escape(path):
        value = Path(path).resolve().as_posix()
        return value.replace("$", "$$").replace("#", "\\#").replace(" ", "\\ ").replace(":", "\\:")

    paths = dependencies.removesuffix("\n").split(" \\\n ")
    if not paths or any(not Path(path).is_file() for path in paths):
        raise ValueError("DXC dependency does not exist")
    depfile.write_text(escape(target) + ": " + " \\\n ".join(map(escape, paths)) + "\n", encoding="utf-8")


def compile_shader(args):
    expected = {"vertex": "vsMain", "fragment": "psMain"}[args.stage]
    if args.entrypoint != expected:
        raise ValueError(f"Entrypoint must be {expected}; main is forbidden")
    output = Path(args.output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    library = str(Path(args.shadercross).resolve().parent.parent / "lib")
    for key in ["LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"]:
        env[key] = library + os.pathsep + env.get(key, "")

    def run(command):
        subprocess.run(list(map(str, command)), env=env, check=True)

    defines = ["-D" + item for item in args.define]
    with tempfile.TemporaryDirectory(prefix="shader-", dir=output.parent) as folder:
        temp = Path(folder)
        common = [args.shadercross, args.source, "-s", "HLSL", "-t", args.stage,
                  "-e", args.entrypoint, "-I", args.include, *defines]
        formats = {}
        for kind, extension in [("SPIRV", "spv"), ("DXIL", "dxil"), ("MSL", "msl")]:
            path = temp / (output.name + "." + extension)
            options = ["--msl-version", args.msl_version] if kind == "MSL" else []
            run([*common, "-d", kind, *options, "-o", path])
            data = path.read_bytes()
            if not data or (kind == "SPIRV" and data[:4] != b"\x03\x02\x23\x07") or (kind == "DXIL" and data[:4] != b"DXBC"):
                raise ValueError(f"Invalid {kind} output")
            if kind == "MSL" and not re.search(r"\b" + args.stage + r"\s+\w+\s+" + re.escape(args.entrypoint) + r"\s*\(", data.decode("utf-8")):
                raise ValueError(f"Generated MSL must define {args.entrypoint} with unchanged name")
            formats[kind.lower()] = {"file": path.name, "entrypoint": args.entrypoint,
                                    "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        reflection = temp / "reflection.json"
        run([args.shadercross, temp / (output.name + ".spv"), "-s", "SPIRV", "-d", "JSON",
             "-t", args.stage, "-e", args.entrypoint, "-o", reflection])
        reflected = json.loads(reflection.read_text(encoding="utf-8"))
        counts = {name: reflected[name] for name in ["samplers", "storage_textures", "storage_buffers", "uniform_buffers"]}
        limits = {"samplers": 16, "storage_textures": 8, "storage_buffers": 8, "uniform_buffers": 4}
        if any(type(value) is not int or value < 0 or value > limits[name] for name, value in counts.items()):
            raise ValueError(f"Invalid reflected resource counts: {counts}")
        structural = temp / (output.name + ".reflect.json")
        run([args.spirv_cross, temp / (output.name + ".spv"), "--reflect", "--output", structural])
        layout = temp / (output.name + ".layout.json")
        layout.write_bytes(canonical_bytes(normalize(json.loads(structural.read_text(encoding="utf-8")),
                           stage=args.stage, entrypoint=args.entrypoint, counts=counts)))

        def descriptor(path):
            data = path.read_bytes()
            return {"file": path.name, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}

        manifest = {"version": 1, "stage": args.stage, "entrypoint": args.entrypoint,
                    "counts": counts, "formats": formats, "msl_version": args.msl_version,
                    "macos_deployment_target": args.deployment_target,
                    "reflection": descriptor(structural), "layout": descriptor(layout)}
        (temp / (output.name + ".json")).write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
        depfile = temp / "includes.d"
        profile = "vs_6_0" if args.stage == "vertex" else "ps_6_0"
        run([args.dxc, args.source, "-T", profile, "-E", args.entrypoint, "-I", args.include,
             *defines, "-M", "-MF", depfile, "-Fo", str(output) + ".spv"])
        normalize_depfile(depfile, str(output) + ".spv")
        for extension in ["spv", "dxil", "msl", "reflect.json", "layout.json", "json"]:
            os.replace(temp / (output.name + "." + extension), str(output) + "." + extension)
        os.replace(depfile, str(output) + ".d")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    for name in ["shadercross", "dxc", "spirv-cross", "source", "include", "output", "entrypoint", "msl-version", "deployment-target"]:
        parser.add_argument("--" + name, required=True)
    parser.add_argument("--stage", choices=["vertex", "fragment"], required=True)
    parser.add_argument("--define", action="append", default=[])
    compile_shader(parser.parse_args())
