#!/usr/bin/env bash
# Optional hosted runner bootstrap; exact portable versions, SDK supplied by Xcode.
# No artifact service or cross-job cache is used.
set -euo pipefail
[[ $(uname -m) == arm64 ]] || { echo 'This optional provisioning lock targets Apple Silicon' >&2; exit 1; }
root=${SOMNIUM_CI_ROOT:-$HOME/somnium-ci}
python3 "$(dirname -- "$0")/provision.py" --root "$root"
xcodebuild -version
