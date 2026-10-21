# Captures the real logging sink at process creation, including native Windows handles.
# The same doctest cases also verify thresholds and that filtered messages are not formatted.
import subprocess
import sys

result = subprocess.run([sys.argv[1], "--test-case=Log*"], capture_output=True, text=True)
print(result.stdout, end="")
print(result.stderr, end="", file=sys.stderr)
if result.returncode:
    sys.exit(result.returncode)
for expected in ["visible-warning", "visible-category", "value=7 ratio=1.25 tag=ok"]:
    if expected not in result.stdout:
        sys.exit(f"Missing emitted message: {expected}")
if "hidden-" in result.stdout:
    sys.exit("A filtered message reached the output sink")
print("Log output and filtering: OK")
