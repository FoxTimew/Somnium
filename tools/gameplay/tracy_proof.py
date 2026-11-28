"""Connects Tracy before the real module exchange and verifies zones from both generations.
Every trace/log stays on the runner; a missing collector or connection is a hard failure.
"""
from __future__ import annotations
import argparse
import csv
import json
from pathlib import Path
import subprocess


def prove(capture: Path, exporter: Path, host: Path, modules: Path, assets: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    trace = output / "gameplay.tracy"
    with (output / "capture.log").open("w", encoding="utf-8") as capture_log, \
            (output / "host.log").open("w", encoding="utf-8") as host_log:
        collector = subprocess.Popen([str(capture), "-a", "127.0.0.1", "-o", str(trace), "-f"],
                                     stdout=capture_log, stderr=subprocess.STDOUT)
        try:
            subprocess.run([str(host), str(modules), str(assets), str(output / "module.json"), "--tracy"],
                           check=True, stdout=host_log, stderr=subprocess.STDOUT, timeout=150)
            collector.wait(timeout=30)
            if collector.returncode:
                raise RuntimeError("Tracy collector failed; see capture.log")
        finally:
            if collector.poll() is None:
                collector.terminate()
                collector.wait(timeout=10)
    if not trace.is_file() or trace.stat().st_size == 0:
        raise RuntimeError("Tracy produced no trace")
    with (output / "zones.csv").open("w", encoding="utf-8", newline="") as stream:
        subprocess.run([str(exporter), str(trace)], check=True, stdout=stream, timeout=60)
    with (output / "zones.csv").open(encoding="utf-8", newline="") as stream:
        rows = list(csv.reader(stream))
    cells = {cell for row in rows for cell in row}
    for zone in ("game.fixed_update", "game.v2.fixed_update"):
        if zone not in cells:
            raise RuntimeError(f"Captured trace lacks {zone}")
    module = json.loads((output / "module.json").read_text(encoding="utf-8"))
    if not module["tracy_connected"] or module["retained_images"] != 64:
        raise RuntimeError("The real module host did not complete its connected proof")
    result = {"version": 1, "status": "validated", "trace_bytes": trace.stat().st_size,
              "zones": ["game.fixed_update", "game.v2.fixed_update"], "retained_images": 64}
    (output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("capture", "exporter", "host", "modules", "assets", "output"):
        parser.add_argument("--" + name, required=True, type=Path)
    args = parser.parse_args()
    prove(**vars(args))
