#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'This script requires macOS and Xcode Command Line Tools.' >&2
  exit 1
fi
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="${WINGMAN_ARCHS:-arm64}" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 "$@"
cmake --build build-macos --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-2}"
ctest --test-dir build-macos --output-on-failure
printf 'Builds are under build-macos/Wingman_artefacts/Release/\n'
printf 'Install and validate the actual compiled AU identifiers:\n'
printf './Scripts/install-macos.sh "build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component" --adhoc\n'
printf './Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/Wingman Pro.component"\n'
