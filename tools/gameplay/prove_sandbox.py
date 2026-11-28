"""Publishes another built game while one sandbox remains alive and captures its composed frame.
Publication is external; no OS input injection or engine-side compilation is involved.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))
from tools.presentation.capture import convert_frame
from tools.gpu.run import native_validation_error
from tools.gameplay.publish import replace


def stage(source: Path, publication: Path, target: Path) -> bytes:
    content = publication.read_bytes()
    document = json.loads(content)
    for role in ("binary", "symbols"):
        if role not in document:
            continue
        relative = Path(document[role]["path"])
        path = (source / relative).resolve()
        if not path.is_relative_to(source.resolve()):
            raise ValueError("Module publication escapes its root")
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != document[role]["sha256"]:
            raise ValueError("Module publication checksum mismatch")
        destination = target / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    return content


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--backend", choices=["direct3d12", "vulkan"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    executable, output = arguments.executable.resolve(), arguments.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    modules = output / "modules"
    modules.mkdir(exist_ok=True)
    source = executable.parent / "modules"
    initial = stage(source, source / "game.json", modules)
    replacement = stage(source, source / "v2.json", modules)
    (modules / "game.json").write_bytes(initial)
    prefix = output / "reloaded"
    for extension in (".json", ".rgba", ".png"):
        prefix.with_suffix(extension).unlink(missing_ok=True)
    log = output / "sandbox.log"
    command = [str(executable), "--backend", arguments.backend, "--watch-gameplay", "on",
               "--module-root", str(modules), "--replay", "replays/courtyard.json",
               "--capture-step", "540", "--capture-output", str(prefix)]
    writable = []
    with log.open("w", encoding="utf-8") as stream:
        process = subprocess.Popen(command, stdout=stream, stderr=subprocess.STDOUT,
                                   env=dict(os.environ, SDL_ASSERT="abort"))
        try:
            deadline = time.monotonic() + 45
            while "First model frame presented" not in log.read_text(encoding="utf-8", errors="replace"):
                if process.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError("Sandbox did not present its first frame")
                time.sleep(.05)
            # Exercise write access to linker outputs while their shadow copy is loaded.
            # Rewriting the same byte preserves the published build and its checksums.
            for role in ("binary", "symbols"):
                entry = json.loads(initial).get(role)
                if entry:
                    linker_output = executable.parent / Path(entry["path"]).name
                    with linker_output.open("r+b") as binary:
                        first = binary.read(1)
                        if not first:
                            raise RuntimeError("Empty linker output")
                        binary.seek(0)
                        binary.write(first)
                        binary.flush()
                    writable.append(role)
            # Let the reference replay open the door before changing movement speed.
            # The exact exchange tick is recorded by runtime; the CPU proof asserts tick 300.
            time.sleep(6)
            temporary = modules / "game.next.json"
            temporary.write_bytes(replacement)
            replace(temporary, modules / "game.json")
            if process.wait(timeout=60):
                raise RuntimeError("Sandbox failed after module publication")
        finally:
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=10)
    text = log.read_text(encoding="utf-8", errors="replace")
    exchanges = re.findall(r"Gameplay reloaded at tick (\d+)", text)
    if len(exchanges) != 1 or native_validation_error(text):
        raise RuntimeError("Expected exactly one accepted exchange without GPU validation errors")
    metadata = convert_frame(prefix, 540)
    result = {"version": 1, "status": "validated", "backend": arguments.backend,
              "single_process": True, "exchange_tick": int(exchanges[0]),
              "linker_outputs_writable": writable,
              "image": "reloaded.png", "metadata": metadata,
              "sha256": hashlib.sha256(prefix.with_suffix(".png").read_bytes()).hexdigest()}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    main()
