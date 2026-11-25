"""Exercise the shipping single-root contract in a temporary package, outside the build.
This explicit test staging is not an asset cooker or an automatic source-asset copy.
"""
import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument("--executable", type=Path, required=True)
parser.add_argument("--model", type=Path, required=True)
parser.add_argument("--scene", type=Path, required=True)
args = parser.parse_args()
source = args.executable.resolve()
with tempfile.TemporaryDirectory(prefix="somnium-package-") as temporary:
    root = Path(temporary)
    package = root / "bin"
    package.mkdir()
    executable = package / source.name
    shutil.copy2(source, executable)
    shutil.copytree(source.parent / "assets/shaders", package / "assets/shaders")
    cwd = root / "unrelated-working-directory"
    cwd.mkdir()
    def launch():
        return subprocess.run([str(executable), "--frames", "12"], cwd=cwd,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, timeout=30)
    missing = launch()
    print(missing.stdout)
    assert missing.returncode == 1 and "scenes/orbit.json" in missing.stdout, "Shipping must not fall back to repository scenes"
    scenes = package / "assets/scenes"
    scenes.mkdir()
    shutil.copy2(args.scene, scenes / "orbit.json")
    missing_model = launch()
    print(missing_model.stdout)
    assert missing_model.returncode == 1 and "models/orbit-cube.glb" in missing_model.stdout, "Shipping must not fall back to repository models"
    models = package / "assets/models"
    models.mkdir()
    shutil.copy2(args.model, models / "orbit-cube.glb")
    inputs = package / "assets/input"
    inputs.mkdir()
    shutil.copy2(args.scene.parent.parent / "input/orbit.json", inputs / "orbit.json")
    complete = launch()
    print(complete.stdout)
    assert complete.returncode == 0 and "frames=12" in complete.stdout, "Explicitly staged package must run from an unrelated cwd"
