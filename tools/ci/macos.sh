#!/usr/bin/env bash
# Reuses the same portable layout and CLI as Linux; the optional job selects its GPU proof.
# CMAKE_OSX_DEPLOYMENT_TARGET remains 13.3 in the engine configuration.
set -euo pipefail
exec bash "$(dirname -- "$0")/linux.sh" "$@"
