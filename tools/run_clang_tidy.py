# Runs clang-tidy 18 against every owned translation unit in the compilation database.
# Missing tools/database or an empty selection fail explicitly; dependencies are never selected.
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path
import argparse
import json
import re
import subprocess
import sys

parser = argparse.ArgumentParser()
parser.add_argument("--build", type=Path, default=Path("build/debug"))
parser.add_argument("--tool", default="clang-tidy-18")
parser.add_argument("--jobs", type=int, default=3)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
version = subprocess.check_output([args.tool, "--version"], text=True)
print(version, flush=True)
if not re.search(r"version 18\.", version):
    sys.exit("clang-tidy major version 18 is required")
database = json.loads((args.build / "compile_commands.json").read_text(encoding="utf-8"))
sources = set()
for entry in database:
    path = (Path(entry["directory"]) / entry["file"]).resolve()
    if path.is_relative_to(root) and path.relative_to(root).parts[0] in {"engine", "game", "sandbox", "tools"}:
        sources.add(path)
if not sources:
    sys.exit("No Somnium translation unit found in the compilation database")
reports = args.build.resolve() / "clang-tidy"
reports.mkdir(exist_ok=True)

def check(path):
    report = reports / ("_".join(path.relative_to(root).parts) + ".yaml")
    result = subprocess.run([args.tool, str(path), "-p", str(args.build.resolve()),
                             f"--export-fixes={report}", "--quiet"], capture_output=True, text=True)
    report.with_suffix(".log").write_text(result.stdout + result.stderr, encoding="utf-8")
    return path, result

failed = False
with ThreadPoolExecutor(max_workers=args.jobs) as executor:
    for future in as_completed([executor.submit(check, path) for path in sorted(sources)]):
        path, result = future.result()
        print(f"{'FAIL' if result.returncode else 'OK'} {path.relative_to(root)}", flush=True)
        if result.returncode:
            print(result.stdout + result.stderr, flush=True)
            failed = True
sys.exit(1 if failed else 0)
