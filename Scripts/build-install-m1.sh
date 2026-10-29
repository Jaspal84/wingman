#!/usr/bin/env bash
# Native developer build; does not claim Logic Pro acceptance or distribution signing.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ "$(uname -s)" != Darwin || "$(uname -m)" != arm64 ]]; then
  echo 'Run this from a native ARM64 Terminal on your Apple Silicon Mac.' >&2
  exit 1
fi
if [[ "${1:-}" == --help ]]; then
  echo 'Usage: ./Scripts/build-install-m1.sh [CMake configure options]'
  echo 'Builds/tests ARM64 Release, backs up and installs the user AU, then runs auval.'
  exit 0
fi
command -v cmake >/dev/null
command -v python3 >/dev/null
xcrun --find clang >/dev/null
mkdir -p build-macos/validation
# pipefail preserves failures while retaining evidence for review.
WINGMAN_ARCHS=arm64 ./Scripts/build-macos.sh \
  -DWINGMAN_BUILD_PLUGIN=ON -DWINGMAN_BUILD_STUDIO=ON -DBUILD_TESTING=ON "$@" \
  2>&1 | tee build-macos/validation/build-tests.log
component='build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component'
executable="$component/Contents/MacOS/JASWINGMAN PRO"
/usr/bin/lipo -verify_arch arm64 "$executable"
./Scripts/install-macos.sh "$component" --adhoc \
  2>&1 | tee build-macos/validation/install.log
installed="$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component"
./Scripts/validate-au.sh "$installed" \
  2>&1 | tee build-macos/validation/auval.log
ctest --test-dir build-macos --output-on-failure \
  --output-junit "$PWD/build-macos/validation/tests.xml"
printf '\nAU build, install and auval completed. Logic Pro acceptance remains manual.\n'
printf 'Follow Documentation/MACBOOK-M1-VALIDATION.md and retain the logs above.\n'
