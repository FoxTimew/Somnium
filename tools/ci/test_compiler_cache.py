#!/usr/bin/env python3
"""Prove that Ninja observes header edits through the real compiler cache.
This catches localized MSVC /showIncludes corruption on both misses and hits.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--work", type=Path, required=True)
    args = parser.parse_args()
    args.work.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.work, prefix="compiler-cache-") as folder:
        source = Path(folder)
        (source / "CMakeLists.txt").write_text('''cmake_minimum_required(VERSION 3.28)
project(CacheDependencyProbe LANGUAGES CXX)
add_executable(probe probe.cpp)
''')
        (source / "probe.cpp").write_text('#include "value.hpp"\nint main() { return value; }\n')
        header = source / "value.hpp"
        header.write_text("constexpr int value = 1;\n")
        build = source / "build"
        subprocess.run([args.cmake, "-S", str(source), "-B", str(build), "-G", "Ninja",
                        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_CXX_COMPILER_LAUNCHER=sccache"], check=True)
        for value in [1, 2, 1]:
            time.sleep(1.05)
            header.write_text(f"constexpr int value = {value};\n")
            subprocess.run([args.cmake, "--build", str(build)], check=True)
            executable = build / ("probe.exe" if (build / "probe.exe").exists() else "probe")
            actual = subprocess.run([str(executable)]).returncode
            if actual != value:
                raise RuntimeError(f"Ninja missed a header edit through sccache: expected {value}, got {actual}")
        print("Compiler-cache header invalidation: passed (1 -> 2 -> 1)")


if __name__ == "__main__":
    main()
