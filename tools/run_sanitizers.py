# Runs the complete Linux sanitizer suite with one known Vulkan ICD.
# Keeping lavapipe loaded preserves driver symbols/global roots through LSan's exit scan.
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--build-dir", type=Path)
args = parser.parse_args()

icds = list(Path("/usr/share/vulkan/icd.d").glob("lvp_icd*.json"))
if len(icds) != 1:
    raise SystemExit("Sanitizers require exactly one installed lavapipe ICD")
library = Path(json.loads(icds[0].read_text())["ICD"]["library_path"])
if not library.is_absolute():
    library = Path("/usr/lib/x86_64-linux-gnu") / library
if not library.is_file():
    raise SystemExit(f"Lavapipe library unavailable: {library}")
print(f"Sanitizers: ICD={icds[0]}, retained driver={library}", flush=True)
ctest = ["ctest", "--test-dir", str(args.build_dir), "--output-on-failure", "--no-tests=error"] if args.build_dir else ["ctest", "--preset", "asan-ubsan"]
# Offline compiler subprocesses do not need the GPU driver preloaded into them.
# Both complementary label selections are required: this still runs the entire suite.
subprocess.run([*ctest, "-LE", "window"], check=True)
preload = ":".join(filter(None, [os.environ.get("LD_PRELOAD"), str(library)]))
environment = dict(os.environ, VK_DRIVER_FILES=str(icds[0]), LD_PRELOAD=preload, SDL_ASSERT="abort")
subprocess.run(["xvfb-run", "-a", *ctest, "-L", "window"], env=environment, check=True)
