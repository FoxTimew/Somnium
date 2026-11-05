#!/usr/bin/env bash
# Uses the same local/CI command line and native CMake executable, avoiding pip launchers.
# WSL work and persistent state must live on ext4, not on a Windows drive mount.
set -euo pipefail
root=${SOMNIUM_CI_ROOT:-$HOME/somnium-ci}
arguments=("$@")
for ((i=0; i<${#arguments[@]}-1; ++i)); do
    if [[ ${arguments[i]} == --root ]]; then root=${arguments[i+1]}; fi
done
[[ -x $root/tools/python/bin/python3 ]] || { echo "Run tools/ci/provision_wsl.sh --root $root first" >&2; exit 1; }
export SOMNIUM_CI_ROOT=$root
export PATH="$root/tools/bin:$root/tools/python/lib/python3.12/site-packages/cmake/data/bin:$root/tools/python/lib/python3.12/site-packages/clang_format/data/bin:$root/tools/python/bin:$PATH"
exec "$root/tools/python/bin/python3" "$(dirname -- "$0")/run.py" "$@"
