#!/usr/bin/env python3
"""Install verified portable tools under one explicit runner root.
Machine packages are handled separately; CI jobs never elevate privileges.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
import urllib.request
import zipfile

HERE = Path(__file__).resolve().parent
LOCK = json.loads((HERE / "toolchain.json").read_text(encoding="utf-8"))


def download(pin, folder):
    folder.mkdir(parents=True, exist_ok=True)
    path = folder / (pin["sha256"] + "-" + pin["url"].split("/")[-1])
    if not path.exists():
        temporary = path.with_suffix(path.suffix + ".partial")
        urllib.request.urlretrieve(pin["url"], temporary)
        temporary.replace(path)
    with path.open("rb") as stream:
        digest = hashlib.file_digest(stream, "sha256").hexdigest()
    if digest != pin["sha256"]:
        raise RuntimeError(f"Checksum mismatch: {path}; remove this download explicitly before retrying")
    return path


def extract(archive, destination):
    destination.mkdir(parents=True, exist_ok=True)
    if archive.suffix == ".zip":
        with zipfile.ZipFile(archive) as source:
            for item in source.infolist():
                if not (destination / item.filename).resolve().is_relative_to(destination.resolve()):
                    raise RuntimeError("Archive escapes destination")
            source.extractall(destination)
    else:
        with tarfile.open(archive) as source:
            source.extractall(destination, filter="data")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    root = args.root.resolve()
    system = platform.system()
    pins = LOCK[system]
    folder = root / "tools"
    downloads = folder / "downloads"
    python = folder / "python" / ("python.exe" if system == "Windows" else "bin/python3")
    if not python.exists():
        extract(download(pins["python"], downloads), folder)
    actual = subprocess.check_output([str(python), "--version"], text=True).strip()
    if actual != "Python " + LOCK["python"]:
        raise RuntimeError(f"Unexpected Python: {actual}")
    packages = [f"cmake=={LOCK['cmake']}", f"ninja=={LOCK['ninja']}",
                f"clang-format=={LOCK['clang_format']}"]
    subprocess.run([str(python), "-m", "pip", "install", "--disable-pip-version-check", *packages], check=True)
    binary = folder / "bin"
    binary.mkdir(parents=True, exist_ok=True)
    for package, executable in [("sccache", "sccache"), ("git_lfs", "git-lfs")]:
        suffix = ".exe" if system == "Windows" else ""
        unpacked = folder / pins[package]["sha256"]
        if not (unpacked / "complete").exists():
            extract(download(pins[package], downloads), unpacked)
            (unpacked / "complete").touch()
        candidates = list(unpacked.rglob(executable + suffix))
        if len(candidates) != 1:
            raise RuntimeError(f"Ambiguous {package} executable")
        target = binary / (executable + suffix)
        with candidates[0].open("rb") as stream:
            expected = hashlib.file_digest(stream, "sha256").digest()
        actual = None
        if target.is_file():
            with target.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").digest()
        if actual != expected:
            shutil.copy2(candidates[0], target)
        target.chmod(0o755)
    if system == "Linux":
        sdk = folder / "vulkan" / LOCK["vulkan_sdk"]
        if not (sdk / "x86_64/lib/libVkLayer_khronos_validation.so").exists():
            extract(download(pins["vulkan"], downloads), folder / "vulkan")
    print(f"Portable toolchain installed: {root}")


if __name__ == "__main__":
    main()
