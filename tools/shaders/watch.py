# Watches source/include changes and invokes only the existing offline CMake generation.
# The engine consumes immutable publications and this separate atomic diagnostic file.
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import time

from publish import atomic_write
from reflection import canonical_bytes


def dependencies(config):
    paths = {Path(path) for path in config["sources"]}
    for depfile in config["depfiles"]:
        path = Path(depfile)
        if not path.is_file():
            continue
        _, separator, tail = path.read_text(encoding="utf-8").partition(": ")
        if not separator:
            raise ValueError(f"Invalid normalized depfile: {path}")
        for value in tail.rstrip("\n").split(" \\\n "):
            paths.add(Path(re.sub(r"\\([ :#\\])", r"\1", value).replace("$$", "$")))
    # A missing/new include cannot yet appear in the previous successful depfile.
    for directory in config["include_roots"]:
        paths.update(path for path in Path(directory).rglob("*")
                     if path.is_file() and path.suffix.lower() in {".hlsl", ".hlsli"})
    return sorted(paths)


def fingerprint(config):
    digest = hashlib.sha256()
    for path in dependencies(config):
        digest.update(str(path).encode("utf-8") + b"\0")
        try:
            digest.update(path.read_bytes())
        except FileNotFoundError:
            digest.update(b"missing")
        digest.update(b"\0")
    return digest.hexdigest()


def build(config, build_directory, configuration):
    command = [config["cmake"], "--build", str(build_directory), "--config", configuration,
               "--target", config["target"], "--parallel", "2"]
    try:
        completed = subprocess.run(command, capture_output=True, text=True, encoding="utf-8",
                                   errors="replace", timeout=600)
        output = completed.stdout + completed.stderr
        ok = completed.returncode == 0
    except (OSError, subprocess.TimeoutExpired) as error:
        output, ok = str(error), False
    print(output, flush=True)
    status = {"version": 1, "ok": ok, "message": "" if ok else output[-8192:]}
    destination = Path(config["manifest"]).with_name(config["program"] + ".status.json")
    atomic_write(destination, canonical_bytes(status))
    return ok


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--config", default="Debug")
    parser.add_argument("--program", default="model")
    parser.add_argument("--once", action="store_true")
    args = parser.parse_args()
    if not re.fullmatch(r"[a-z][A-Za-z0-9_]*", args.program):
        parser.error("Invalid program name")
    config = json.loads((args.build / "shader-watch" / args.config / (args.program + ".json")).read_text())
    if config.get("version") != 1 or config.get("program") != args.program:
        parser.error("Unsupported generated watch configuration")
    if args.once:
        raise SystemExit(0 if build(config, args.build, args.config) else 1)
    previous = None
    while True:
        current = fingerprint(config)
        if current != previous:
            time.sleep(0.25)
            settled = fingerprint(config)
            if settled != current:
                continue
            build(config, args.build, args.config)
            # Compare against the pre-build fingerprint, so edits during compilation retry.
            previous = current
        time.sleep(0.25)


if __name__ == "__main__":
    main()
