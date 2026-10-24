#!/usr/bin/env bash
set -euo pipefail
exec python3 "$(dirname "$0")/macos_au.py" validate "$@"
