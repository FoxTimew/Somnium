#!/usr/bin/env python3
"""Shared local/CI operations with persistent disk caches and per-run evidence.
Only summaries use GitHub environment variables; no operation requires GitHub.
"""
import argparse
import base64
import contextlib
import ctypes
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import time
import uuid

ROOT = Path(__file__).resolve().parents[2]
PINS = json.loads((ROOT / "tools/ci/toolchain.json").read_text(encoding="utf-8"))
SYSTEM = platform.system()
PRESETS = ["debug", "release", "shipping", "profiling", "asan-ubsan", "iteration", "iteration-profiling"]


def key(*files, context=""):
    digest = hashlib.sha256()
    for name in files:
        digest.update((ROOT / name).read_bytes().replace(b"\r\n", b"\n"))
    digest.update((SYSTEM + platform.machine()).encode())
    digest.update(context.encode())
    return digest.hexdigest()[:24]


@contextlib.contextmanager
def exclusive(path):
    """An OS lock is released even when concurrency cancellation kills the process."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as stream:
        stream.write(b"0")
        stream.flush()
        stream.seek(0)
        if SYSTEM == "Windows":
            import msvcrt
            try:
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            except OSError as error:
                raise RuntimeError(f"Another local/CI operation holds {path}") from error
        else:
            import fcntl
            fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        yield


def clean_runs(root, current, days):
    runs = (root / "runs").resolve()
    # Compare physical paths on both sides, including macOS temporary aliases.
    current = current.resolve()
    if not runs.is_dir():
        return
    cutoff = time.time() - days * 86400
    for path in runs.iterdir():
        # Never follow symlinks/junctions; only direct run directories with our marker.
        if path.is_symlink() or path.is_junction() or not path.is_dir():
            continue
        resolved = path.resolve()
        marker = resolved / "run.json"
        if resolved == current or resolved.parent != runs or not marker.is_file():
            continue
        data = json.loads(marker.read_text(encoding="utf-8"))
        if data.get("owner") != "somnium-ci" or marker.stat().st_mtime >= cutoff:
            continue
        with exclusive(resolved / "active.lock"):
            # Validate again immediately before the recursive removal on Windows.
            if resolved.parent != runs or resolved.is_symlink() or resolved.is_junction():
                raise RuntimeError("Unsafe retention path")
        shutil.rmtree(resolved)


class Runner:
    def __init__(self, args):
        self.args = args
        self.root = args.root.resolve()
        if SYSTEM == "Linux" and (str(self.root).startswith("/mnt/") or str(ROOT).startswith("/mnt/")):
            raise RuntimeError("WSL checkout and CI root must be on the Linux filesystem, never /mnt/c")
        self.run = self.root / "runs" / args.run_id
        self.run.mkdir(parents=True, exist_ok=True)
        self.logs = self.run / "logs"
        self.logs.mkdir(exist_ok=True)
        self.records = []
        self.env = dict(os.environ)
        # Cached MSVC stdout contains the localized /showIncludes prefix. Objects
        # from the old French cache must never enter an English Ninja build.
        tool_key = key("tools/ci/toolchain.json", context="VSLANG=1033" if SYSTEM == "Windows" else "")
        self.env["CPM_SOURCE_CACHE"] = str(self.root / "cache/cpm" / key("cmake/CPM.cmake", "cmake/Dependencies.cmake"))
        self.env["SCCACHE_DIR"] = str(self.root / "cache/sccache" / tool_key)
        self.env["SCCACHE_CACHE_SIZE"] = "20G"
        self.env["SCCACHE_SERVER_PORT"] = {"Windows": "43260", "Linux": "43261", "Darwin": "43262"}[SYSTEM]
        self.env["PYTHONUTF8"] = "1"
        # Stable /showIncludes language is required by Ninja and sccache on French Windows.
        self.env["VSLANG"] = "1033"
        self.env["SDL_ASSERT"] = "abort"
        self.env["SDL_LOGGING"] = "*=verbose"
        if SYSTEM == "Linux":
            self.env.update(CC="clang-18", CXX="clang++-18")
        self.shader_cache = self.root / "cache/shaders" / key("tools/shader_toolchain/lock.json", "tools/shader_toolchain/build.py", "tools/shader_toolchain/CMakeLists.txt", "tools/ci/toolchain.json")
        self.provider = "source" if SYSTEM == "Darwin" else "official"
        self.suffix = ".exe" if SYSTEM == "Windows" else ""
        self.compiler = self.shader_cache / "install" / self.provider / "bin" / ("shadercross" + self.suffix)
        self.cmake = shutil.which("cmake")
        if not self.cmake:
            raise RuntimeError("Native CMake missing; run provisioning")
        self.ctest = Path(self.cmake).with_name("ctest" + self.suffix)
        self.tidy_tool = "clang-tidy-18" if SYSTEM == "Linux" else "clang-tidy"

    def call(self, label, command, *, env=None, window=False):
        if window and SYSTEM == "Linux":
            command = ["xvfb-run", "-a", "-s", "-screen 0 1600x1000x24", *command]
        command = list(map(str, command))
        print(f"[{label}] {' '.join(command)}", flush=True)
        start = time.monotonic()
        logfile = self.logs / (label + ".log")
        status = -1
        try:
            with logfile.open("w", encoding="utf-8") as log:
                process = subprocess.Popen(command, cwd=ROOT, env=env or self.env,
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace")
                for line in process.stdout:
                    print(line, end="", flush=True)
                    log.write(line)
                status = process.wait()
            if status:
                raise RuntimeError(f"{label} failed ({status}); log: {logfile}")
        finally:
            record = {"step": label, "seconds": round(time.monotonic() - start, 3), "exit_code": status, "log": str(logfile)}
            self.records.append(record)
            self.summary(f"{label}: {'OK' if status == 0 else 'FAILED'} — {record['seconds']} s — `{logfile}`")

    def summary(self, message):
        print(message, flush=True)
        with (self.run / "SUMMARY.md").open("a", encoding="utf-8") as stream:
            stream.write(message + "\n\n")
        if os.getenv("GITHUB_STEP_SUMMARY"):
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as stream:
                stream.write(message + "\n\n")

    def py(self, label, script, *arguments, **kwargs):
        self.call(label, [sys.executable, ROOT / script, *arguments], **kwargs)

    def build_dir(self, preset):
        return self.run / "build" / preset

    def doctor(self):
        if platform.python_version() != PINS["python"]:
            raise RuntimeError(f"Python {PINS['python']} required, got {platform.python_version()}")
        checks = [("cmake", [self.cmake, "--version"], PINS["cmake"]),
                  ("ninja", ["ninja", "--version"], PINS["ninja"].rsplit(".", 1)[0]),
                  ("format", ["clang-format", "--version"], PINS["clang_format"]),
                  ("sccache", ["sccache", "--version"], PINS["sccache"]),
                  ("lfs", ["git-lfs", "version"], PINS["git_lfs"])]
        if SYSTEM == "Linux":
            if os.geteuid() == 0:
                raise RuntimeError("Run CI as the unprivileged Linux user, never root")
            checks += [("clang", ["clang-18", "--version"], PINS["clang_linux"]),
                       ("tidy", [self.tidy_tool, "--version"], PINS["clang_linux"])]
        if SYSTEM == "Windows":
            if ctypes.windll.shell32.IsUserAnAdmin():
                raise RuntimeError("Run CI without elevation")
            compiler = Path(shutil.which("cl"))
            if not (compiler.parent / "1033/clui.dll").is_file():
                raise RuntimeError("MSVC English resources required for reliable sccache/Ninja dependencies; repeat provision_windows.ps1 -Machine")
            if not os.environ.get("VCToolsVersion", "").strip("/\\").startswith(PINS["msvc"]):
                raise RuntimeError("Pinned MSVC environment missing")
            session = ctypes.c_uint()
            if not ctypes.windll.kernel32.ProcessIdToSessionId(os.getpid(), ctypes.byref(session)):
                raise RuntimeError("Cannot inspect Windows session")
            self.summary(f"Windows session: {session.value}; window mode: {self.args.window_mode}")
            if self.args.window_mode == "interactive" and session.value == 0:
                raise RuntimeError("Session 0 cannot prove presentation; use the interactive scheduled task")
        for name, command, expected in checks:
            self.call("version-" + name, command)
            if expected not in (self.logs / ("version-" + name + ".log")).read_text(encoding="utf-8"):
                raise RuntimeError(f"Pinned {name} {expected} required")
        if SYSTEM == "Linux":
            self.call("platform-packages", ["dpkg-query", "-W", "clang-18", "clang-tidy-18", "libstdc++-13-dev", "mesa-vulkan-drivers", "xvfb"])
        self.summary(f"Host: {platform.platform()}; free disk: {shutil.disk_usage(self.root).free // (1024**3)} GiB")
        self.summary(f"Run evidence: `{self.run}`; shader cache: `{self.shader_cache}`")

    def cache_server(self):
        # A server retains its initial SCCACHE_DIR. Rebind under the root lock so
        # a changed tool hash cannot silently reuse the previous hash directory.
        stopped = subprocess.run(["sccache", "--stop-server"], env=self.env, capture_output=True)
        self.summary(f"sccache previous server stop status: {stopped.returncode}")
        self.call("sccache-start", ["sccache", "--start-server"])

    def format(self):
        self.py("format", "tools/format.py", "--check")
        self.py("architecture", "tools/check_architecture.py", ROOT)
        self.py("architecture-rules", "tools/test_architecture.py")
        self.py("shipping-rules", "tools/test_shipping.py")
        self.py("ci-contract", "tools/ci/test_ci.py")
        self.py("capture-replay-contract", "tools/presentation/test_capture.py")

    def compiler_cache(self):
        self.py("compiler-cache-dependencies", "tools/ci/test_compiler_cache.py", "--cmake", self.cmake, "--work", self.run)

    def shader_toolchain(self):
        self.py("shader-toolchain", "tools/shader_toolchain/build.py", "--provider", self.provider,
                "--jobs", self.args.jobs, "--cache-root", self.shader_cache,
                "--report-dir", self.run / "shader-toolchain")

    def hot_reload_spike(self):
        if SYSTEM not in ["Windows", "Linux"]:
            raise RuntimeError("The isolated reload spike is validated on Windows/Linux only")
        variants = ["plain", "profiling"] + (["sanitizers"] if SYSTEM == "Linux" else [])
        for variant in variants:
            build = self.run / "hot-reload-spike" / variant
            command = [self.cmake, "-S", ROOT / "tools/hot_reload_spike", "-B", build,
                       "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Debug",
                       f"-DSPIKE_PROFILING={'ON' if variant == 'profiling' else 'OFF'}",
                       f"-DSOMNIUM_ENABLE_SANITIZERS={'ON' if variant == 'sanitizers' else 'OFF'}",
                       "-DCMAKE_C_COMPILER_LAUNCHER=sccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache"]
            if SYSTEM == "Linux":
                command += ["-DCMAKE_C_COMPILER=clang-18", "-DCMAKE_CXX_COMPILER=clang++-18"]
            else:
                command += ["-DCMAKE_POLICY_DEFAULT_CMP0141=NEW", "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"]
            self.call(f"reload-{variant}-configure", command)
            self.call(f"reload-{variant}-build", [self.cmake, "--build", build, "--parallel", self.args.jobs])
            self.call(f"reload-{variant}-test", [self.ctest, "--test-dir", build, "--output-on-failure", "-V"],
                      env={**self.env, "ASAN_OPTIONS": "detect_leaks=1:halt_on_error=1",
                           "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
            if SYSTEM == "Linux" and variant == "plain":
                self.py("reload-tidy", "tools/run_clang_tidy.py", "--build", build,
                        "--tool", self.tidy_tool, "--jobs", self.args.jobs)

    def configure(self, preset):
        if not self.compiler.is_file():
            raise RuntimeError("Shader CLI missing; run the shader-toolchain command first")
        command = [self.cmake, "--preset", preset, "-B", self.build_dir(preset),
                   "-DSOMNIUM_WARNINGS_AS_ERRORS=ON", "-DSDL_WAYLAND=OFF",
                   f"-DSOMNIUM_SHADERCROSS_EXECUTABLE={self.compiler}",
                   f"-DPython3_EXECUTABLE={sys.executable}",
                   "-DCMAKE_C_COMPILER_LAUNCHER=sccache", "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache"]
        if SYSTEM == "Linux":
            command += ["-DCMAKE_C_COMPILER=clang-18", "-DCMAKE_CXX_COMPILER=clang++-18"]
        if SYSTEM == "Windows":
            command += ["-DCMAKE_POLICY_DEFAULT_CMP0141=NEW", "-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=Embedded"]
        if preset == "asan-ubsan":
            command += ["-DSDL_X11_SHARED=OFF", "-DSOMNIUM_ENABLE_ITERATION=ON"]
        self.call("configure-" + preset, command)
        if preset == "shipping":
            self.py("shipping-graph", "tools/check_shipping.py", self.build_dir(preset))

    def build(self, preset):
        self.call("build-" + preset, [self.cmake, "--build", self.build_dir(preset), "--parallel", self.args.jobs])
        if SYSTEM == "Windows":
            dependencies = subprocess.check_output(["ninja", "-C", self.build_dir(preset), "-t", "deps"],
                env=self.env, text=True, encoding="utf-8", errors="replace")
            objects = [(name, int(count)) for name, count in re.findall(r"^(.+\.obj): #deps (\d+)", dependencies, re.MULTILINE)
                       if name.replace("\\", "/").startswith(("engine/", "game/", "sandbox/", "tools/"))]
            missing = [name for name, count in objects if count == 0]
            if not objects or missing:
                raise RuntimeError(f"Ninja lost project header dependencies through MSVC/sccache: {missing or 'no objects found'}")
            self.summary(f"{preset}: Ninja header dependencies present for {len(objects)} project objects")

    def test(self, preset):
        command = [self.ctest, "--test-dir", self.build_dir(preset), "--output-on-failure", "--no-tests=error"]
        if SYSTEM == "Windows" and self.args.window_mode == "service":
            command += ["-LE", "window"]
            self.summary("Window tests: **non validé** (session 0/service); manual proof required")
        self.call("test-" + preset, command, window=True)

    def tidy(self):
        self.py("tidy", "tools/run_clang_tidy.py", "--build", self.build_dir("iteration"), "--tool", self.tidy_tool, "--jobs", self.args.jobs)

    def sanitizers(self):
        if SYSTEM != "Linux":
            raise RuntimeError("The required sanitizer job runs on Linux")
        self.configure("asan-ubsan")
        self.build("asan-ubsan")
        env = dict(self.env, ASAN_OPTIONS="detect_leaks=1:halt_on_error=1", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        self.py("sanitizers", "tools/run_sanitizers.py", "--build-dir", self.build_dir("asan-ubsan"), env=env)

    def gpu_environment(self, backend):
        env = dict(self.env)
        if backend == "vulkan":
            sdk = self.root / "tools/vulkan" / PINS["vulkan_sdk"]
            layers = list(sdk.rglob("VkLayer_khronos_validation.json"))
            if len(layers) != 1:
                raise RuntimeError(f"Pinned SDK validation layer missing or ambiguous: {sdk}")
            env.update(VK_LAYER_PATH=str(layers[0].parent), VK_INSTANCE_LAYERS="VK_LAYER_KHRONOS_validation",
                       VK_KHRONOS_VALIDATION_VALIDATE_SYNC="1", VK_LOADER_DEBUG="layer")
            if SYSTEM == "Linux":
                icds = list(Path("/usr/share/vulkan/icd.d").glob("lvp_icd*.json"))
                if len(icds) != 1:
                    raise RuntimeError("Exactly one lavapipe ICD required")
                env["VK_DRIVER_FILES"] = str(icds[0])
                env["LD_LIBRARY_PATH"] = str(sdk / "x86_64/lib") + os.pathsep + env.get("LD_LIBRARY_PATH", "")
        return env

    def gpu(self, backend, preset="debug"):
        env = self.gpu_environment(backend)
        for test, executable in [("d5", "somnium_gpu_offscreen"), ("cube", "somnium_renderer_offscreen"), ("scene", "somnium_scene_offscreen")]:
            output = self.run / (test + "-" + backend)
            arguments = ["--test", test, "--executable", self.build_dir(preset) / "bin" / (executable + self.suffix),
                         "--backend", backend, "--output", output]
            if SYSTEM == "Darwin":
                arguments += ["--allow-unavailable"]
            self.py(test + "-" + backend, "tools/gpu/run.py", *arguments, env=env, window=True)
            report = json.loads((output / "result.json").read_text(encoding="utf-8"))
            if SYSTEM == "Windows" and "6600M" not in report.get("device", {}).get("device", ""):
                raise RuntimeError("Windows GPU proof did not use Radeon RX 6600M")
            self.summary(f"{test} {backend}: **{report['status']}** — `{output}`")
            # Hosted macOS disks disappear after the job. Preserve only the three
            # small scene images in ordinary logs, without an artifact service.
            if SYSTEM == "Darwin" and test == "scene":
                for name in ["previous", "actual", "current"]:
                    path = output / (name + ".png")
                    if path.exists():
                        data = path.read_bytes()
                        if len(data) > 1024 * 1024:
                            raise RuntimeError("Scene log capture exceeds 1 MiB")
                        print("SOMNIUM_SCENE_IMAGE " + json.dumps({"name": path.name,
                              "sha256": hashlib.sha256(data).hexdigest(),
                              "base64": base64.b64encode(data).decode("ascii")}), flush=True)
        self.call("composition-" + backend, [self.build_dir(preset) / "bin" / ("somnium_gpu_capabilities" + self.suffix), backend,
                  self.run / ("composition-" + backend + ".json")], env=env, window=True)
        log = (self.logs / ("composition-" + backend + ".log")).read_text(encoding="utf-8")
        sys.path.insert(0, str(ROOT / "tools/gpu"))
        sys.path.insert(0, str(ROOT))
        from tools.gpu.run import native_validation_error
        if native_validation_error(log) or "COMPOSITION_PROBE_COMPLETE" not in log:
            raise RuntimeError("Composition probe missing or native validation diagnostic")

    def captures(self, backend, preset="debug"):
        output = self.run / ("presentation-" + backend)
        if SYSTEM == "Windows" and self.args.window_mode == "service":
            output.mkdir(exist_ok=True)
            (output / "result.json").write_text(json.dumps({"status": "non validé", "reason": "Session 0/service; manual proof required"}), encoding="utf-8")
            self.summary("Presentation: **non validé** (service mode)")
            return
        arguments = ["--executable", self.build_dir(preset) / "bin" / ("sandbox" + self.suffix),
                     "--output", output, "--backend", backend]
        try:
            self.py("capture-" + backend, "tools/presentation/capture.py", *arguments, env=self.gpu_environment(backend), window=True)
        finally:
            # Hosted disks are ephemeral. Retain produced frames even if comparison fails.
            if SYSTEM == "Darwin":
                for path in sorted(output.glob("*.png")):
                    data = path.read_bytes()
                    if len(data) > 2 * 1024 * 1024:
                        raise RuntimeError("Presentation log capture exceeds 2 MiB")
                    print("SOMNIUM_PRESENTATION_IMAGE " + json.dumps({"name": path.name,
                          "sha256": hashlib.sha256(data).hexdigest(),
                          "base64": base64.b64encode(data).decode("ascii")}), flush=True)

    def pipeline(self):
        # One job per OS: cheap checks first, GPU smoke before four full build variants.
        self.doctor()
        self.format()
        self.cache_server()
        self.compiler_cache()
        if SYSTEM in ["Windows", "Linux"]:
            self.hot_reload_spike()
        self.shader_toolchain()
        self.configure("debug")
        self.build("debug")
        self.test("debug")
        backends = {"Windows": ["direct3d12", "vulkan"], "Linux": ["vulkan"], "Darwin": ["metal"]}[SYSTEM]
        if SYSTEM == "Darwin" and os.getenv("SOMNIUM_MACOS_RUNNER") != "macos-15":
            backends = []
            self.summary("Metal: **non validé** on this build-only host; the independent macos-15 job probes Metal")
        for backend in backends:
            self.gpu(backend)
            # Engine readback is independent of desktop overlap/focus and injects no OS input.
            # A usable window/swapchain is still required, as for the existing window tests.
            self.captures(backend)
        self.configure("iteration")
        self.build("iteration")
        self.test("iteration")
        if SYSTEM == "Linux":
            self.tidy()
        for preset in ["release", "shipping", "profiling"]:
            self.configure(preset)
            self.build(preset)
            self.test(preset)
        if SYSTEM == "Linux":
            self.sanitizers()
        self.call("sccache-statistics", ["sccache", "--show-stats"])

    def iteration_profiling(self):
        self.doctor()
        self.cache_server()
        self.compiler_cache()
        self.shader_toolchain()
        self.configure("iteration-profiling")
        self.build("iteration-profiling")
        self.test("iteration-profiling")
        self.summary("Iteration + Tracy: CPU contracts and Play/Edit window tests; GPU validation remains in the required Debug jobs")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operation", choices=["doctor", "format", "compiler-cache", "shader-toolchain", "hot-reload-spike", "configure", "build", "test", "tidy", "sanitizers", "gpu", "captures", "pipeline", "iteration-profiling", "cleanup"])
    parser.add_argument("--root", type=Path, default=Path(os.environ.get("SOMNIUM_CI_ROOT", "C:/SomniumCI" if SYSTEM == "Windows" else str(Path.home() / "somnium-ci"))))
    parser.add_argument("--run-id", default=os.getenv("GITHUB_RUN_ID", "local-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S") + "-" + uuid.uuid4().hex[:6]) + "-" + os.getenv("GITHUB_RUN_ATTEMPT", "1"))
    parser.add_argument("--preset", choices=PRESETS, default="debug")
    parser.add_argument("--backend", choices=["vulkan", "direct3d12", "metal"], default="direct3d12" if SYSTEM == "Windows" else "metal" if SYSTEM == "Darwin" else "vulkan")
    parser.add_argument("--window-mode", choices=["interactive", "service"], default="interactive")
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,79}", args.run_id) or args.jobs < 1:
        parser.error("Unsafe run id or invalid job count")
    runner = Runner(args)
    started = time.monotonic()
    status = "failure"
    with exclusive(runner.root / "operation.lock"), exclusive(runner.run / "active.lock"):
        try:
            clean_runs(runner.root, runner.run, PINS["retention_days"])
            (runner.run / "run.json").write_text(json.dumps({"owner": "somnium-ci", "status": "running", "operation": args.operation}), encoding="utf-8")
            if args.operation in ["build", "sanitizers", "compiler-cache", "hot-reload-spike"]:
                runner.cache_server()
            if args.operation in ["configure", "build", "test"]:
                getattr(runner, args.operation)(args.preset)
            elif args.operation in ["gpu", "captures"]:
                getattr(runner, args.operation)(args.backend)
            elif args.operation != "cleanup":
                getattr(runner, args.operation.replace("-", "_"))()
            status = "success"
        finally:
            data = {"owner": "somnium-ci", "operation": args.operation, "status": status,
                    "source": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                    "source_dirty": bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT, text=True).strip()),
                    "os": SYSTEM, "records": runner.records, "elapsed_seconds": round(time.monotonic() - started, 3),
                    "finished": datetime.now(timezone.utc).isoformat()}
            (runner.run / "run.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
            runner.summary(f"**{status}**: {args.operation}. Evidence retained locally: `{runner.run}`")
            runner.summary(f"Total: {data['elapsed_seconds']:.3f} seconds; warm per-push objective: < 3600 seconds per runner")


if __name__ == "__main__":
    main()
