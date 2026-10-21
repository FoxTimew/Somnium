# Obtains and proves the offline compiler without configuring any engine module.
# Every subprocess is timed and logged; cache hits still execute all smoke tests.
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
LOCK = json.loads((HERE / "lock.json").read_text(encoding="utf-8"))
CACHE = ROOT / ".cache/shader-toolchain"
REPORTS = CACHE / "reports"
SYSTEM = platform.system()
EXT = ".exe" if SYSTEM == "Windows" else ""
RESULT = {"os": SYSTEM, "arch": platform.machine(), "steps": [], "sources": LOCK["sources"]}


def run(name, command, *, cwd=None, env=None, expect_success=True):
    REPORTS.mkdir(parents=True, exist_ok=True)
    logfile = REPORTS / f"{name}.log"
    print(f"[{name}] {' '.join(map(str, command))}", flush=True)
    started = time.monotonic()
    with logfile.open("w", encoding="utf-8") as stream:
        process = subprocess.run(list(map(str, command)), cwd=cwd, env=env,
                                 stdout=stream, stderr=subprocess.STDOUT, text=True)
    RESULT["steps"].append({"name": name, "seconds": round(time.monotonic() - started, 3),
                            "returncode": process.returncode})
    print(f"[{name}] exit={process.returncode}, {RESULT['steps'][-1]['seconds']}s", flush=True)
    output = logfile.read_text(encoding="utf-8", errors="replace")
    if expect_success and process.returncode:
        print(output[-16000:], flush=True)
        raise RuntimeError(f"{name} failed; see {logfile}")
    return process.returncode, output


def obtain_source(name):
    pin = LOCK["sources"][name]
    destination = CACHE / "sources" / f"{name}-{pin['revision']}"
    if not (destination / ".git").exists():
        destination.mkdir(parents=True, exist_ok=True)
        run(f"{name}-init", ["git", "init", destination])
        run(f"{name}-remote", ["git", "remote", "add", "origin", pin["repository"]], cwd=destination)
        run(f"{name}-fetch", ["git", "fetch", "--depth", "1", "origin", pin["revision"]], cwd=destination)
        run(f"{name}-checkout", ["git", "checkout", "--detach", "FETCH_HEAD"], cwd=destination)
    _, revision = run(f"{name}-revision", ["git", "rev-parse", "HEAD"], cwd=destination)
    if revision.strip() != pin["revision"]:
        raise RuntimeError(f"Unpinned {name} checkout: {revision}")
    if name == "dxc":
        run("dxc-submodules", ["git", "submodule", "update", "--init", "--recursive", "--depth", "1"], cwd=destination)
        run("dxc-submodule-revisions", ["git", "submodule", "status", "--recursive"], cwd=destination)
    return destination


def download(pin):
    destination = CACHE / "downloads" / pin["url"].split("/")[-1]
    destination.parent.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    if not destination.exists():
        urllib.request.urlretrieve(pin["url"], destination)
    with destination.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != pin["sha256"]:
        raise RuntimeError(f"SHA256 mismatch for {destination}: {digest}")
    RESULT["steps"].append({"name": destination.name, "seconds": round(time.monotonic() - started, 3),
                            "sha256": digest, "bytes": destination.stat().st_size})
    return destination


def unpack(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as source:
            for item in source.infolist():
                if not (destination / item.filename).resolve().is_relative_to(destination.resolve()):
                    raise RuntimeError("Archive path outside destination")
            source.extractall(destination)
    else:
        with tarfile.open(archive) as source:
            source.extractall(destination, filter="data")


def obtain_dxc(provider, jobs):
    if provider == "official":
        if SYSTEM not in LOCK["official"]:
            raise RuntimeError(f"DXC {LOCK['sources']['dxc']['version']} has no official {SYSTEM} archive")
        destination = CACHE / "official-dxc"
        unpack(download(LOCK["official"][SYSTEM]), destination)
        return destination
    source = obtain_source("dxc")
    build = CACHE / "dxc-build"
    destination = CACHE / "source-dxc"
    run("dxc-configure", ["cmake", "-S", source, "-B", build, "-G", "Ninja",
        "-C", source / "cmake/caches/PredefinedParams.cmake", "-DCMAKE_BUILD_TYPE=Release",
        "-DHLSL_INCLUDE_TESTS=OFF", "-DLLVM_INCLUDE_TESTS=OFF", "-DSPIRV_BUILD_TESTS=OFF",
        "-DLLVM_INCLUDE_BENCHMARKS=OFF", "-DLLVM_ENABLE_ASSERTIONS=OFF",
        "-DLLVM_PARALLEL_LINK_JOBS=1"])
    run("dxc-build", ["cmake", "--build", build, "--target", "dxc", "dxcompiler", "dxildll",
                      "--parallel", str(jobs)])
    (destination / "include/dxc").mkdir(parents=True, exist_ok=True)
    for header in (source / "include/dxc").glob("*.h"):
        shutil.copy2(header, destination / "include/dxc" / header.name)
    for folder in ["bin", "lib"]:
        (destination / folder).mkdir(exist_ok=True)
        for pattern in ["dxc", "dxc.exe", "*dxcompiler*", "*dxil.*"]:
            for file in (build / folder).glob(pattern):
                if file.is_file():
                    shutil.copy2(file, destination / folder / file.name)
    return destination


def runtime_environment(prefix):
    env = os.environ.copy()
    env["PATH"] = str(prefix / "bin") + os.pathsep + env.get("PATH", "")
    for variable in ["LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH"]:
        env[variable] = str(prefix / "lib") + os.pathsep + env.get(variable, "")
    return env


def verify(prefix):
    compiler = prefix / "bin" / ("shadercross" + EXT)
    env = runtime_environment(prefix)
    run("consumer-configure", ["cmake", "-S", HERE / "verify", "-B", CACHE / "consumer",
                               f"-DSOMNIUM_SHADERCROSS_EXECUTABLE={compiler}"], env=env)
    status, message = run("consumer-missing-tool", ["cmake", "-S", HERE / "verify",
        "-B", CACHE / "consumer-missing", f"-DSOMNIUM_SHADERCROSS_EXECUTABLE={CACHE / 'absent-shadercross'}"],
        env=env, expect_success=False)
    if not status or "SOMNIUM_SHADERCROSS_EXECUTABLE is required" not in message:
        raise RuntimeError("Consumer did not fail explicitly for a missing offline compiler")
    run("dxc-version", [prefix / "bin" / ("dxc" + EXT), "--version"], env=env)
    for stage, short in [("vertex", "vert"), ("fragment", "frag")]:
        source = HERE / "fixtures" / f"probe.{short}.hlsl"
        base = [compiler, source, "-s", "HLSL", "-t", stage, "-e", "main", "-I", HERE / "fixtures"]
        for target in ["SPIRV", "DXIL", "MSL"]:
            output = REPORTS / f"probe.{short}.{target.lower()}"
            run(f"compile-{short}-{target}", [*base, "-d", target, "-o", output], env=env)
            data = output.read_bytes()
            if not data or (target == "SPIRV" and data[:4] != b'\x03\x02\x23\x07') or (target == "DXIL" and data[:4] != b'DXBC'):
                raise RuntimeError(f"Invalid {target} output")
            if target == "MSL" and (b"metal_stdlib" not in data or stage.encode() not in data):
                raise RuntimeError("Missing Metal shader declaration")
        metadata = REPORTS / f"probe.{short}.json"
        run(f"reflect-{short}", [compiler, REPORTS / f"probe.{short}.spirv", "-s", "SPIRV",
            "-d", "JSON", "-t", stage, "-e", "main", "-o", metadata], env=env)
        counts = json.loads(metadata.read_text())
        expected = {"samplers": int(stage == "fragment"), "storage_buffers": int(stage == "fragment"),
                    "storage_textures": 0, "uniform_buffers": 1}
        if any(counts.get(key) != value for key, value in expected.items()):
            raise RuntimeError(f"Unexpected reflection for {stage}: {counts}")
    invalid = REPORTS / "invalid.hlsl"
    invalid.write_text("This is deliberately invalid HLSL.\n", encoding="utf-8")
    status, _ = run("invalid-hlsl", [compiler, invalid, "-s", "HLSL", "-d", "SPIRV", "-t", "vertex",
        "-o", REPORTS / "invalid.spirv"], env=env, expect_success=False)
    if not status:
        raise RuntimeError("Invalid shader unexpectedly succeeded")


def build(provider, jobs):
    prefix = CACHE / "install" / provider
    marker = prefix / "verified-lock.json"
    RESULT["cache_hit"] = marker.exists()
    if marker.exists():
        if json.loads(marker.read_text()) != LOCK:
            raise RuntimeError("Installed toolchain cache does not match lock.json")
    else:
        dxc = obtain_dxc(provider, jobs)
        sources = {name: obtain_source(name) for name in ["sdl", "spirv_cross", "shadercross"]}
        arguments = [f"-DSOMNIUM_{name.upper()}_SOURCE={path}" for name, path in sources.items()]
        run("shadercross-configure", ["cmake", "-S", HERE, "-B", CACHE / "cli-build", "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}", f"-DSOMNIUM_DXC_ROOT={dxc}", *arguments])
        run("shadercross-build", ["cmake", "--build", CACHE / "cli-build", "--target", "shadercross", "--parallel", str(jobs)])
        run("shadercross-install", ["cmake", "--install", CACHE / "cli-build"])
        candidates = list((dxc / "bin").rglob("dxc" + EXT))
        executable = next((p for p in candidates if SYSTEM != "Windows" or p.parent.name == "x64"), None)
        if executable is None:
            raise RuntimeError("Pinned DXC CLI missing from package")
        shutil.copy2(executable, prefix / "bin" / executable.name)
    verify(prefix)
    marker.write_text(json.dumps(LOCK, indent=2) + "\n", encoding="utf-8")
    RESULT["prefix"] = str(prefix)


def sdk_probe():
    if SYSTEM != "Linux":
        raise RuntimeError("The SDK comparison probe requires Linux x86_64")
    unpack(download(LOCK["vulkan_sdk"]), CACHE / "vulkan-sdk")
    sdk = CACHE / "vulkan-sdk" / LOCK["vulkan_sdk"]["version"] / "x86_64"
    env = runtime_environment(sdk)
    run("sdk-dxc-version", [sdk / "bin/dxc", "--version"], env=env)
    capabilities = {}
    for target, extra in [("SPIRV", ["-spirv"]), ("DXIL", [])]:
        status, _ = run(f"sdk-{target}", [sdk / "bin/dxc", HERE / "fixtures/probe.vert.hlsl",
            "-T", "vs_6_0", "-E", "main", "-I", HERE / "fixtures", "-Fo", REPORTS / f"sdk.{target.lower()}", *extra],
            env=env, expect_success=False)
        capabilities[target] = status == 0
    capabilities["dxcompiler_libraries"] = [p.name for p in (sdk / "lib").glob("*dxcompiler*")]
    capabilities["dxil_libraries"] = [p.name for p in (sdk / "lib").glob("*dxil*")]
    RESULT["sdk_capabilities"] = capabilities
    print(json.dumps(capabilities, indent=2))
    # A measured incompatible candidate is a comparison result, never the selected toolchain.
    if not capabilities["SPIRV"]:
        raise RuntimeError("Vulkan SDK DXC cannot perform its documented SPIR-V compilation")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--provider", choices=["official", "source", "vulkan-sdk"], required=True)
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    RESULT["provider"] = args.provider
    REPORTS.mkdir(parents=True, exist_ok=True)
    started = time.monotonic()
    try:
        if args.provider == "vulkan-sdk":
            sdk_probe()
        else:
            build(args.provider, args.jobs)
        RESULT["status"] = "success"
    except Exception as error:
        RESULT["status"] = "failure"
        RESULT["error"] = str(error)
        raise
    finally:
        RESULT["total_seconds"] = round(time.monotonic() - started, 3)
        (REPORTS / "result.json").write_text(json.dumps(RESULT, indent=2) + "\n", encoding="utf-8")
        summary = os.getenv("GITHUB_STEP_SUMMARY")
        if summary:
            with open(summary, "a", encoding="utf-8") as output:
                output.write(f"### Toolchain spike: {SYSTEM} / {args.provider}\n\n```json\n{json.dumps(RESULT, indent=2)}\n```\n")


if __name__ == "__main__":
    main()
