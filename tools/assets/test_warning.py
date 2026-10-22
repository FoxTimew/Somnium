"""Count the actual Log output for a model containing several ignored features.
Repeated requests must hit the asset identity cache and emit no duplicate warning.
"""
import subprocess
import sys

result = subprocess.run([sys.argv[1], "--test-case=optional material*"], capture_output=True, text=True)
print(result.stdout, end="")
print(result.stderr, end="", file=sys.stderr)
if result.returncode:
    sys.exit(result.returncode)
if result.stdout.count("ignored/fallback:") != 1:
    sys.exit("Expected exactly one aggregate warning for the model")
