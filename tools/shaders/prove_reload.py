# Exercises one live renderer process while complete offline programs are atomically replaced.
# Pixel-aligned geometry and white/red Lambert probes provide an independent exact oracle.
import argparse
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import threading

from publish import publish, sha
from reflection import canonical_bytes
from reload_reference import expected, verify

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/gpu"))
from compare import write_png
from run import native_validation_error


def run(args, report):
    verify()
    executable = args.executable.resolve()
    source = executable.parent / "assets/shaders"
    destination = args.output.resolve()
    data = destination / "data/shaders"
    data.mkdir(parents=True, exist_ok=True)
    output = data / "model.program.json"
    initial = {"vertex": source / "modelVertex.vsMain.vert.json",
               "lambert": source / "modelFragment.psMain.frag.json", "ambient": source / "modelAmbient.psMain.frag.json"}
    red = dict(initial, lambert=source / "reloadRed.psMain.frag.json", ambient=source / "reloadRedAmbient.psMain.frag.json")
    incompatible = dict(red, lambert=source / "reloadLayout.psMain.frag.json")
    baseline = publish("model", initial, output)
    scene_path = ROOT / "engine/renderer/tests/reload_scene.json"
    scene = json.loads(scene_path.read_text())
    shutil.copyfile(scene_path, destination / scene_path.name)
    report.update(binary_sha256=sha(executable.read_bytes()), scene_sha256=sha(scene_path.read_bytes()),
                  backend=args.backend, validation_enabled=True, source_commit=os.getenv("GITHUB_SHA"), events=[])
    messages = queue.Queue()
    with (destination / "validation.log").open("w", encoding="utf-8") as log:
        process = subprocess.Popen([str(executable), args.backend, str(data.parent), str(destination), str(scene_path), args.format],
                                   stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=log, text=True,
                                   encoding="utf-8", errors="replace", bufsize=1)

        def receive():
            for line in process.stdout:
                messages.put(line)
            messages.put(None)

        thread = threading.Thread(target=receive, daemon=True)
        thread.start()

        def response():
            line = messages.get(timeout=120)
            while line is not None and not line.startswith("{"):
                report.setdefault("engine_log", []).append(line.rstrip())
                line = messages.get(timeout=120)
            if line is None:
                raise RuntimeError("Renderer terminated before its proof response; see validation.log")
            value = json.loads(line)
            report["events"].append(value)
            return value

        def command(op, **kwargs):
            process.stdin.write(json.dumps(dict(op=op, **kwargs)) + "\n")
            process.stdin.flush()
            return response()

        def render(name, color):
            value = command("render", name=name)
            if value != {"event": "render", "name": name, "variants": 4}:
                raise RuntimeError("Incomplete render response")
            reference = expected(scene, color)
            write_png(destination / (name + "-expected.png"), scene["width"], scene["height"], reference)
            for variant in range(4):
                path = destination / f"{name}-{variant}.rgba"
                actual = path.read_bytes()
                write_png(path.with_suffix(".png"), scene["width"], scene["height"], actual)
                if actual != reference:
                    raise RuntimeError(f"Analytical image differs: {name}, lighting/winding {variant}")

        try:
            ready = response()
            if ready.get("event") != "ready" or ready.get("generation") != baseline["generation"]:
                raise RuntimeError("Renderer did not load the initial publication")
            report["device"] = ready["device"]
            render("initial", scene["initial_rgba"])
            replacement = publish("model", red, output)
            if replacement["layout_sha256"] != baseline["layout_sha256"]:
                raise RuntimeError("Color variant unexpectedly changes the structural layout")
            value = command("watch")
            if value["diagnostic"] or value["generation"] != replacement["generation"]:
                raise RuntimeError("Compatible program did not replace every dependent pipeline")
            render("red", scene["replacement_rgba"])

            # A hashed but malformed ambient artifact fails after Lambert candidates exist.
            staging = destination / "invalid-stage"
            staging.mkdir(exist_ok=True)
            broken = json.loads(initial["ambient"].read_text())
            for descriptor in [*broken["formats"].values(), broken["reflection"], broken["layout"]]:
                shutil.copyfile(source / descriptor["file"], staging / descriptor["file"])
            for descriptor in broken["formats"].values():
                blob = bytes(32)
                (staging / descriptor["file"]).write_bytes(blob)
                descriptor.update(bytes=len(blob), sha256=sha(blob))
            broken_path = staging / initial["ambient"].name
            broken_path.write_bytes(canonical_bytes(broken))
            publish("model", dict(red, ambient=broken_path), output)
            value = command("reload")
            if value["ok"] or value["restart_required"] or value["generation"] != replacement["generation"]:
                raise RuntimeError("Invalid artifact changed the active generation")
            if not command("watch")["diagnostic"]:
                raise RuntimeError("Rejected artifact has no runtime/UI diagnostic")
            command("watch")  # Repeated observations must not duplicate the error log.
            render("invalid-preserved", scene["replacement_rgba"])

            layout = publish("model", incompatible, output)
            if layout["layout_sha256"] == baseline["layout_sha256"]:
                raise RuntimeError("Type change was not reflected in the layout signature")
            if json.loads(initial["lambert"].read_text())["counts"] != json.loads(incompatible["lambert"].read_text())["counts"]:
                raise RuntimeError("Incompatible layout proof must preserve resource counts")
            value = command("reload")
            if value["ok"] or not value["restart_required"] or value["generation"] != replacement["generation"]:
                raise RuntimeError("Layout change did not produce the dedicated restart diagnostic")
            if "redémarrage requis" not in command("watch")["diagnostic"]:
                raise RuntimeError("Restart requirement missing from runtime/UI diagnostic")
            command("watch")
            render("layout-preserved", scene["replacement_rgba"])
            publish("model", initial, output)
            value = command("watch")
            if value["diagnostic"] or value["generation"] != baseline["generation"]:
                raise RuntimeError("Valid program did not recover after rejected candidates")
            render("recovered", scene["initial_rgba"])
            if command("reload")["changed"]:
                raise RuntimeError("Unchanged program rebuilt pipelines")
            process.stdin.write('{"op":"quit"}\n')
            process.stdin.flush()
            process.stdin.close()
            if process.wait(timeout=30) != 0:
                raise RuntimeError("Renderer reported a validation or cleanup error")
            if sum("[warning]" in line for line in report.get("engine_log", [])) != 2:
                raise RuntimeError("Reload diagnostics were lost or repeated for the same publication")
        finally:
            if process.poll() is None:
                process.kill()
                process.wait(timeout=30)
            thread.join(timeout=5)
    validation = (destination / "validation.log").read_text(encoding="utf-8")
    if native_validation_error(validation):
        raise RuntimeError("Native validation reported an error")
    report.update(status="validé", images=20, pixel_differences=0,
                  program_generations={"initial": baseline["generation"], "red": replacement["generation"]})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--backend", choices=["direct3d12", "vulkan", "metal"], required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    report = {"version": 1, "status": "échec"}
    try:
        report["formats"] = {}
        for format_name in ["unorm", "srgb"]:
            options = argparse.Namespace(**vars(args))
            options.output = args.output / format_name
            options.format = format_name
            options.output.mkdir(parents=True, exist_ok=True)
            result = {}
            report["formats"][format_name] = result
            run(options, result)
        report.update(status="validé", images=40, pixel_differences=0)
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (args.output / "result.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
