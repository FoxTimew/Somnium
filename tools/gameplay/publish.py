"""Publishes immutable module bytes and symbols before atomically switching the manifest.
The linker output remains writable while a sandbox executes its own shadow copy.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import tempfile
import time


def replace(source: Path, target: Path) -> None:
    deadline = time.monotonic() + 2
    while True:
        try:
            os.replace(source, target)
            return
        except PermissionError:
            if os.name != "nt" or time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


def publish(binary: Path, symbols: Path | None, output: Path) -> None:
    files = {"binary": (binary.name, binary.read_bytes())}
    if symbols:
        files["symbols"] = (symbols.name, symbols.read_bytes())
    hashes = {key: hashlib.sha256(data).hexdigest() for key, (_, data) in files.items()}
    generation = hashlib.sha256(json.dumps(hashes, sort_keys=True).encode()).hexdigest()
    root = output.parent.resolve()
    root.mkdir(parents=True, exist_ok=True)
    directory = root / "generations" / generation
    directory.mkdir(parents=True, exist_ok=True)
    manifest: dict = {"version": 1}
    for key, (name, data) in files.items():
        target = directory / name
        if not target.exists() or target.read_bytes() != data:
            with tempfile.NamedTemporaryFile(dir=directory, delete=False) as stream:
                temporary = Path(stream.name)
                stream.write(data)
            try:
                replace(temporary, target)
            finally:
                temporary.unlink(missing_ok=True)
        manifest[key] = {"path": target.relative_to(root).as_posix(), "sha256": hashes[key]}
    encoded = (json.dumps(manifest, sort_keys=True, indent=2) + "\n").encode()
    if output.exists() and output.read_bytes() == encoded:
        return
    with tempfile.NamedTemporaryFile(dir=root, delete=False) as stream:
        temporary = Path(stream.name)
        stream.write(encoded)
    try:
        replace(temporary, output)
    finally:
        temporary.unlink(missing_ok=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--symbols", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    publish(arguments.binary, arguments.symbols, arguments.output)
