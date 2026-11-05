#!/usr/bin/env bash
# Provision Ubuntu 24.04 inside WSL2; build storage must remain on its Linux filesystem.
# Root installs system packages only. Re-run without root for portable user-owned tools.
set -euo pipefail
root="${SOMNIUM_CI_ROOT:-$HOME/somnium-ci}"
if [[ ${1:-} == --root ]]; then root=$2; shift 2; fi
if [[ $# -ne 0 ]]; then echo 'Usage: provision_wsl.sh [--root /home/somnium-ci/somnium-ci]' >&2; exit 2; fi
source /etc/os-release
[[ $ID == ubuntu && $VERSION_ID == 24.04 ]] || { echo 'Ubuntu 24.04 is required' >&2; exit 1; }
[[ $root == /* && $root != /mnt/* ]] || { echo 'Use an absolute Linux filesystem path, never /mnt/c' >&2; exit 1; }
here=$(cd -- "$(dirname -- "$0")" && pwd)
if [[ $EUID == 0 ]]; then
    apt-get update
    apt-get install -y python3
    version=$(python3 -c 'import json,sys; print(json.load(open(sys.argv[1]))["llvm_deb"])' "$here/toolchain.json")
    apt-get install -y "clang-18=$version" "clang-tidy-18=$version" build-essential libstdc++-13-dev \
        python3 ca-certificates curl git xz-utils xvfb xauth xdotool imagemagick mesa-vulkan-drivers vulkan-tools \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libxtst-dev \
        libxkbcommon-dev libegl1-mesa-dev libgl1-mesa-dev libasound2-dev libpulse-dev libudev-dev
    echo 'System packages installed. Now run this script without sudo as the dedicated runner user.'
else
    command -v clang-18 >/dev/null || { echo 'First run the system-package pass with sudo' >&2; exit 1; }
    python3 "$here/provision.py" --root "$root"
    echo "Provisioned $root. Register the Linux/X64 runner as described in docs/CI_RUNNER.md."
fi
