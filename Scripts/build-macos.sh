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
wingman_app="build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app"
wingman_executable="$wingman_app/Contents/MacOS/JASWINGMAN PRO"
if [[ ! -x "$wingman_executable" ]]; then
  echo "Standalone build is missing: $wingman_executable. Ensure WINGMAN_BUILD_PLUGIN=ON." >&2
  exit 1
fi
/usr/bin/lipo -archs "$wingman_executable"
if [[ "${WINGMAN_ARCHS:-arm64}" == *arm64* ]]; then
  /usr/bin/lipo "$wingman_executable" -verify_arch arm64
fi
ctest --test-dir build-macos --output-on-failure
printf 'Builds are under build-macos/Wingman_artefacts/Release/\n'
printf 'Install and validate the actual compiled AU identifiers:\n'
printf './Scripts/install-macos.sh "build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component" --adhoc\n'
printf './Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component"\n'

printf 'Install standalone separately (default: ~/Applications):\n'
printf 'python3 Scripts/install-standalone-macos.py "build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app"\n'
printf './Scripts/launch-standalone-macos.sh\n'
