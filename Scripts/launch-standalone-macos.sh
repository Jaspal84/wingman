#!/usr/bin/env bash
set -euo pipefail
if [[ "$(uname -s)" != Darwin ]]; then
  echo 'Launching the standalone requires macOS.' >&2
  exit 1
fi
wingman_app="${1:-$HOME/Applications/JASWINGMAN PRO.app}"
wingman_executable="$wingman_app/Contents/MacOS/JASWINGMAN PRO"
if [[ ! -x "$wingman_executable" ]]; then
  echo "Standalone is not installed or executable: $wingman_executable" >&2
  exit 1
fi
/usr/bin/lipo -verify_arch arm64 "$wingman_executable"
/usr/bin/codesign --verify --deep --strict "$wingman_app"
/usr/bin/open "$wingman_app"
printf 'Launch requested for: %s\n' "$wingman_app"
