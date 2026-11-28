# Publishes a complete shader program last, after immutable generation files exist.
# A failed or concurrent stage build cannot replace the previous active manifest.
import argparse
import errno
import hashlib
import json
import os
from pathlib import Path
import re
import tempfile
import time

from reflection import canonical_bytes


def sha(data):
    return hashlib.sha256(data).hexdigest()


def sharing_violation(error):
    # Python's CRT-backed open reports EACCES without winerror for the same race.
    native = getattr(error, "winerror", None)
    return os.name == "nt" and (native in {5, 32, 33} or
                               (native is None and error.errno == errno.EACCES))


def read_publication(path):
    # Windows can briefly deny opening the old directory entry while another
    # publisher replaces it. Retry opening, never reuse a partially read document.
    deadline = time.monotonic() + 2
    while True:
        try:
            return path.read_bytes()
        except PermissionError as error:
            if not sharing_violation(error) or time.monotonic() >= deadline:
                raise
            time.sleep(0.01)


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        if read_publication(path) == data:
            return
    except FileNotFoundError:
        pass
    with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".publish-", delete=False) as stream:
        temporary = Path(stream.name)
        try:
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        except BaseException:
            stream.close()
            temporary.unlink(missing_ok=True)
            raise
    try:
        deadline = time.monotonic() + 2
        while True:
            try:
                os.replace(temporary, path)
                break
            except PermissionError as error:
                # Windows readers may briefly omit FILE_SHARE_DELETE. Never fall back
                # to truncating the live file: retry its atomic replacement, bounded.
                if not sharing_violation(error) or time.monotonic() >= deadline:
                    raise
                time.sleep(0.01)
    finally:
        temporary.unlink(missing_ok=True)


def stage_files(path):
    manifest = path.read_bytes()
    data = json.loads(manifest)
    if data.get("version") != 1 or set(data.get("formats", {})) != {"spirv", "dxil", "msl"}:
        raise ValueError("Incomplete stage formats")
    if data.get("entrypoint") != {"vertex": "vsMain", "fragment": "psMain"}.get(data.get("stage")):
        raise ValueError("Invalid stage entry point")
    result = {path.name: manifest}
    for descriptor in [*data["formats"].values(), data["reflection"], data["layout"]]:
        name = descriptor["file"]
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_.-]+", name) or name in {".", ".."}:
            raise ValueError("Unsafe stage artifact path")
        content = (path.parent / name).read_bytes()
        if len(content) != descriptor["bytes"] or sha(content) != descriptor["sha256"]:
            raise ValueError(f"Incomplete or changed artifact: {name}")
        if name in result:
            raise ValueError("Duplicate stage artifact name")
        result[name] = content
    return data, result


def publish(name, stages, destination):
    if not re.fullmatch(r"[a-z][A-Za-z0-9_]*", name) or "vertex" not in stages or len(stages) < 2:
        raise ValueError("Program requires a name, vertex and fragment stages")
    files, records, layouts = {}, {}, {}
    for role, path in sorted(stages.items()):
        if not re.fullmatch(r"[a-z][A-Za-z0-9_]*", role):
            raise ValueError("Invalid program stage role")
        stage, products = stage_files(path)
        if stage["stage"] != ("vertex" if role == "vertex" else "fragment"):
            raise ValueError("Program stage role differs from reflection")
        for filename, content in products.items():
            if filename in files and files[filename] != content:
                raise ValueError("Colliding program artifact names")
            files[filename] = content
        records[role] = path.name
        layouts[role] = stage["layout"]["sha256"]
    descriptors = {filename: {"bytes": len(data), "sha256": sha(data)} for filename, data in sorted(files.items())}
    generation = sha(canonical_bytes({"program": name, "stages": records, "files": descriptors}))
    relative = Path(name + ".generations") / generation
    target = destination.parent / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists():
        if not target.is_dir() or any((target / filename).read_bytes() != data for filename, data in files.items()):
            raise ValueError("Immutable generation was modified")
    else:
        with tempfile.TemporaryDirectory(dir=target.parent, prefix=".generation-") as temporary:
            prepared = Path(temporary) / "complete"
            prepared.mkdir()
            for filename, data in files.items():
                (prepared / filename).write_bytes(data)
            # Renaming a complete directory avoids exposing a partially staged generation.
            try:
                os.rename(prepared, target)
            except OSError:
                if not target.is_dir() or any((target / filename).read_bytes() != data for filename, data in files.items()):
                    raise ValueError("Concurrent generation publication differs")
    layout = sha("".join(f"{role}:{value}\n" for role, value in sorted(layouts.items())).encode("ascii"))
    document = {"version": 1, "program": name, "generation": generation, "layout_sha256": layout,
                "stages": {role: (relative / filename).as_posix() for role, filename in records.items()},
                "files": {(relative / filename).as_posix(): value for filename, value in descriptors.items()}}
    atomic_write(destination, canonical_bytes(document))
    atomic_write(destination.with_name(name + ".status.json"),
                 canonical_bytes({"version": 1, "ok": True, "message": ""}))
    return document


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--name", required=True)
    parser.add_argument("--stage", action="append", required=True, help="role=stage-manifest.json")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    stages = {}
    for value in args.stage:
        role, separator, path = value.partition("=")
        if not separator or role in stages:
            parser.error("Each stage requires a unique role=manifest")
        stages[role] = Path(path)
    result = publish(args.name, stages, args.output)
    print(f"Published {args.name}: {result['generation']}")


if __name__ == "__main__":
    main()
