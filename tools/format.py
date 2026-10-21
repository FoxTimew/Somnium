# Formats or verifies only project C/C++ sources, never dependency caches.
# Pass --check in CI; use the same clang-format major version on all hosts.
from pathlib import Path
import argparse
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("--check", action="store_true")
parser.add_argument("--clang-format", default="clang-format")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
paths = [
    str(path)
    for folder in ("engine", "game", "sandbox", "tools")
    for path in (root / folder).rglob("*")
    if path.suffix in {".hpp", ".cpp", ".h", ".c", ".mm"}
]
flags = ["--dry-run", "--Werror"] if args.check else ["-i"]
subprocess.run([args.clang_format, *flags, *paths], check=True)
