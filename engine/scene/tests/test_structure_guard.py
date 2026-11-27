"""Check assertion and release behavior using isolated, noninteractive child processes.
Intentional assertion failures must not create core dumps or native error dialogs.
"""
import argparse
import os
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument("executable")
parser.add_argument("--assertions", choices=["on", "off"], required=True)
args = parser.parse_args()
if os.name != "nt":
    import resource
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
for operation in ["create", "destroy", "reparent"]:
    process = subprocess.run([args.executable, operation], capture_output=True, timeout=15)
    output = (process.stdout + process.stderr).decode("utf-8", errors="replace")
    if args.assertions == "on":
        assert process.returncode != 0 and "m_traversals == 0" in output, (operation, process.returncode, output)
    else:
        assert process.returncode == 0, (operation, process.returncode, output)
print("create/destroy/reparent forbidden during traversal; assertions=" + args.assertions)
