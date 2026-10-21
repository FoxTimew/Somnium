# Exercises the real build graph with transitive includes, option changes and missing products.
# This uses the pinned CLI, never a mock compiler or a generated reference implementation.
import argparse
import json
from pathlib import Path
import subprocess
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    # Work is a dedicated test output directory; do not remove a caller-supplied tree.
    args.work.mkdir(parents=True, exist_ok=True)
    import tempfile
    with tempfile.TemporaryDirectory(prefix="shader contract ", dir=args.work) as folder:
        root = Path(folder)
        source = root / "source"
        source.mkdir()
        (source / "nested.hlsli").write_text("#define TEST_DEPTH 0.5\n")
        (source / "common.hlsli").write_text('#include "nested.hlsli"\n')
        (source / "probe.hlsl").write_text('#include "common.hlsli"\nfloat4 vsMain(uint id : SV_VertexID) : SV_Position { return float4(float(id) * SCALE, 0, TEST_DEPTH, 1); }\n')
        script = f'''cmake_minimum_required(VERSION 3.28)
project(ShaderContract NONE)
set(CMAKE_OSX_DEPLOYMENT_TARGET 11.0)
include("{(args.root / 'cmake/Shaders.cmake').as_posix()}")
add_custom_target(consumer ALL)
somnium_add_shaders(TARGET consumer NAME probe SOURCE probe.hlsl STAGE vertex ENTRYPOINT ${{ENTRY}} INCLUDE_DIRECTORY "${{CMAKE_CURRENT_SOURCE_DIR}}" DEFINES SCALE=${{SCALE}})
'''
        (source / "CMakeLists.txt").write_text(script)
        build = root / "build"

        def run(command, fail=False):
            result = subprocess.run(list(map(str, command)), capture_output=True, text=True)
            if (result.returncode != 0) != fail:
                raise RuntimeError(result.stdout + result.stderr)
            return result.stdout + result.stderr

        def configure(entry="vsMain", scale="1", compiler=args.compiler):
            return run(["cmake", "-S", source, "-B", build, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug",
                        f"-DSOMNIUM_SHADERCROSS_EXECUTABLE={compiler}", f"-DENTRY={entry}", f"-DSCALE={scale}"],
                       fail=entry != "vsMain" or compiler != args.compiler)

        def compile_all():
            return run(["cmake", "--build", build, "--", "-d", "explain"])

        configure()
        compile_all()
        manifest = build / "shaders/Debug/probe.vsMain.vert.json"
        initial = manifest.stat().st_mtime_ns
        no_op = compile_all()
        assert manifest.stat().st_mtime_ns == initial, "No-op rebuild changed outputs\n" + no_op
        time.sleep(1.05)  # Filesystem/Ninja timestamp granularity, not an engine wait.
        (source / "nested.hlsli").write_text("#define TEST_DEPTH 0.25\n")
        compile_all()
        assert manifest.stat().st_mtime_ns > initial, "Transitive include did not rebuild"
        previous = manifest.read_bytes()
        configure(scale="2")
        compile_all()
        assert manifest.read_bytes() != previous, "Changed compile definitions did not rebuild"
        (build / "shaders/Debug/probe.vsMain.vert.dxil").unlink()
        compile_all()
        assert (build / "shaders/Debug/probe.vsMain.vert.dxil").exists()
        (source / "new.hlsli").write_text("#define NEW_DEPTH 0.75\n")
        (source / "nested.hlsli").write_text('#include "new.hlsli"\n#define TEST_DEPTH NEW_DEPTH\n')
        compile_all()
        initial = manifest.stat().st_mtime_ns
        time.sleep(1.05)
        (source / "new.hlsli").write_text("#define NEW_DEPTH 0.625\n")
        compile_all()
        assert manifest.stat().st_mtime_ns > initial, "New transitive include not tracked"
        (source / "nested.hlsli").write_text("#define TEST_DEPTH 0.125\n")
        (source / "new.hlsli").unlink()
        compile_all()
        run(["cmake", "--install", build, "--prefix", root / "installed"])
        data = json.loads(manifest.read_text())
        for item in data["formats"].values():
            assert (root / "installed/bin/assets/shaders" / item["file"]).stat().st_size == item["bytes"]
        rejected_entry = configure(entry="main")
        assert "main is forbidden" in " ".join(rejected_entry.split()), rejected_entry
        missing_tool = configure(compiler=str(root / "absent"))
        assert "Bootstrap explicitly" in " ".join(missing_tool.split()), missing_tool
        configure()
        (source / "probe.hlsl").write_text("deliberate invalid HLSL\n")
        run(["cmake", "--build", build], fail=True)
        print("Shader build contract: no-op, transitive/add/remove includes, defines, products, install, names and failures OK")


if __name__ == "__main__":
    main()
