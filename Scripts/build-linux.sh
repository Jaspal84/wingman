#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Linux ]]; then
  echo 'This script requires Linux; use build-macos.sh on macOS.' >&2
  exit 1
fi
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug "$@"
cmake --build build --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
if [[ -z "${DISPLAY:-}" ]] && command -v xvfb-run >/dev/null; then
  xvfb-run -a ctest --test-dir build --output-on-failure
else
  ctest --test-dir build --output-on-failure
fi
