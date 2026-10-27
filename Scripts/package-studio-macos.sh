#!/bin/bash
# Native macOS packaging only. No signing identity or notarization secret is stored.
set -euo pipefail
if [[ ${1:-} == --help ]]; then
  printf '%s\n' 'Usage: package-studio-macos.sh [Release-build-directory] [output-directory]' 'Builds an arm64 standalone and creates an unsigned pkg by default.' 'Optional: JASPRO_CODESIGN_IDENTITY and JASPRO_INSTALLER_IDENTITY from your keychain.'
  exit 0
fi
if [[ $(uname -s) != Darwin ]]; then printf '%s\n' 'Packaging requires native macOS, Xcode command line tools and pkgbuild.' >&2; exit 1; fi
repo=$(cd "$(dirname "$0")/.." && pwd)
build=${1:-"$repo/build-studio-release"}
output=${2:-"$repo/build-studio-packages"}
cmake -S "$repo" -B "$build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DWINGMAN_BUILD_STUDIO=ON
cmake --build "$build" --target JasProStudio --config Release --parallel 2
app="$build/JasProStudio_artefacts/Release/Jas PRO Ultimate Loop Studio.app"
test -d "$app"
if ! lipo -archs "$app/Contents/MacOS/Jas PRO Ultimate Loop Studio" | tr ' ' '\n' | /usr/bin/grep -qx arm64; then printf '%s\n' 'Missing arm64 executable' >&2; exit 1; fi
# Reject unbundled Homebrew/private dylibs rather than creating a broken package.
while IFS= read -r dependency; do
  case "$dependency" in /usr/lib/*|/System/Library/*) ;; *) printf '%s\n' "Unbundled dependency: $dependency. Use system/static SQLite or bundle/sign dependencies before packaging." >&2; exit 1 ;; esac
done < <(otool -L "$app/Contents/MacOS/Jas PRO Ultimate Loop Studio" | awk 'NR > 1 {print $1}')
mkdir -p "$output"
# Stage a copy; preserve the build output and all installed applications.
stage=$(mktemp -d "${TMPDIR:-/tmp}/jaspro-package.XXXXXX")
trap 'rm -rf "$stage"' EXIT
ditto "$app" "$stage/Jas PRO Ultimate Loop Studio.app"
if [[ -n ${JASPRO_CODESIGN_IDENTITY:-} ]]; then
  codesign --force --options runtime --timestamp --sign "$JASPRO_CODESIGN_IDENTITY" "$stage/Jas PRO Ultimate Loop Studio.app"
  codesign --verify --deep --strict "$stage/Jas PRO Ultimate Loop Studio.app"
fi
package="$output/JasProStudio-arm64-0.3.0.pkg"
args=(--component "$stage/Jas PRO Ultimate Loop Studio.app" --install-location /Applications --identifier com.wingmanaudio.jaspro.studio --version 0.3.0)
if [[ -n ${JASPRO_INSTALLER_IDENTITY:-} ]]; then args+=(--sign "$JASPRO_INSTALLER_IDENTITY"); fi
pkgbuild "${args[@]}" "$package"
printf '%s\n' "Created $package" 'This package has not been notarized. Validate it and the application on the target Mac before distribution.'
