# Runs the complete Linux sanitizer suite with one known Vulkan ICD.
# Keeping lavapipe loaded preserves driver symbols/global roots through LSan's exit scan.
import json
import os
from pathlib import Path
import subprocess

icds = list(Path("/usr/share/vulkan/icd.d").glob("lvp_icd*.json"))
if len(icds) != 1:
    raise SystemExit("Sanitizers require exactly one installed lavapipe ICD")
library = Path(json.loads(icds[0].read_text())["ICD"]["library_path"])
if not library.is_absolute():
    library = Path("/usr/lib/x86_64-linux-gnu") / library
if not library.is_file():
    raise SystemExit(f"Lavapipe library unavailable: {library}")
print(f"Sanitizers: ICD={icds[0]}, retained driver={library}", flush=True)
environment = dict(os.environ, VK_DRIVER_FILES=str(icds[0]), LD_PRELOAD=str(library), SDL_ASSERT="abort")
subprocess.run(["xvfb-run", "-a", "ctest", "--preset", "asan-ubsan"], env=environment, check=True)
