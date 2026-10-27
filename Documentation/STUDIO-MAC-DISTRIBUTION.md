# Native Apple Silicon build, package and acceptance

No Mac package was built in Linux. The native build and packaging procedure below
requires macOS, Xcode Command Line Tools and a physical Apple Silicon machine.

Install Git and CMake (>=3.22), e.g. `xcode-select --install` and `brew install cmake`.
SQLite3/FTS5 normally comes from the macOS SDK; if your SDK lookup fails, install SQLite with
Homebrew and supply `-DCMAKE_PREFIX_PATH="$(brew --prefix sqlite)"`. A development
build may link Homebrew dynamically; packaging rejects those unbundled dylibs.
Use the system SQLite or configure SQLite3_LIBRARY to the static Homebrew library
and SQLite3_INCLUDE_DIR to its headers before producing a distributable app. The standalone
has its own bundle ID; it does not replace the JASWINGMAN PRO plugin.

```bash
cd "$HOME/Developer/wingman"
git fetch origin
git switch development/omniloop-eight
git pull --ff-only origin development/omniloop-eight
cmake -S . -B build-studio-debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DWINGMAN_BUILD_STUDIO=ON
cmake --build build-studio-debug --parallel 2
ctest --test-dir build-studio-debug --output-on-failure
open "build-studio-debug/JasProStudio_artefacts/Debug/Jas PRO Ultimate Loop Studio.app"
```

For an unsigned native installation package:

```bash
bash Scripts/package-studio-macos.sh
```

The script builds Release arm64, verifies the architecture and system-only dylib dependencies, stages a copy and uses
`pkgbuild` for `/Applications/Jas PRO Ultimate Loop Studio.app`. It never installs
the package or overwrites the build artefact. Running the installer can replace an
installed Jas PRO Studio app; retain your prior app before testing it. The package
identifier is `com.wingmanaudio.jaspro.studio`, version 0.3.0. On a multi-config
build, the script passes `--config Release`.

Signing is optional and owner controlled. To sign, supply your available keychain
identities through JASPRO_CODESIGN_IDENTITY and JASPRO_INSTALLER_IDENTITY. No signing
identity or secret is invented or stored. A signed build is not automatically
notarized. Use your organization's Apple notarization/keychain procedure, verify
the notarization result, staple the ticket and validate the final package before
distribution. Review JUCE's AGPL/commercial license and asset licenses first.

## Physical MacBook Air M1 test checklist (not run in cloud)

- Confirm native `uname -m` is arm64 and the built executable contains arm64.
- Run all CTest suites and the GUI self-test; verify all seven views at Retina and
  non-Retina scale, minimum size, keyboard focus, dock resizing and file dialogs.
- Index a licensed library including WAV/AIFF/FLAC and >30-second files. Test
  cancellation, incremental rescans, corrections, duplicates and persisted tags.
  Benchmark your intended library size; no production-scale timing is assumed.
- Select the CoreAudio output, audition original/matched-rate audio, and drag a
  source from the dock to a timeline track. Verify timing/pitch behavior is labelled.
- Build/inspect actual Inspire/Magic arrangements from tagged sources. Audition the
  reference, regenerate one track, undo and reopen projects with source references.
- Apply Flip, reverse and fades. Export WAV and MIDI and audition/import in Logic.
- Assign samples to Play/Drum, use a real MIDI controller, test polyphony/ADSR/layers,
  pad steps/swing, commit a pattern, export MIDI and save/reopen complete projects.
- Enable effects individually and in combination. Compare playback with the offline
  bounce including tails. Measure CPU, latency and dropouts at 44.1/48 kHz and small
  buffers, during cold-cache paging and under memory pressure. Test seek, loop,
  device switches and closing while indexing/rendering. Parameter smoothing and
  cache-prefetch guarantees are not implemented.
- Open the generated guitar WAV/MIDI; confirm the reported basic synthesis and
  chord voicings. Existing SFZ and stem functionality remains in JASWINGMAN PRO.
- Validate and audition the existing plugin in Logic, following AU-VALIDATION.md:
  `arch -arm64 /usr/bin/auval -v aumf Wgm1 Wgma`. No new Bridge/Listen AU is claimed.
- Apply the disposable Supabase schema and run the two-account RLS/storage/auth/
  conflict tests in SUPABASE.md before claiming account sync works live.
- Inspect/install the staged pkg on a disposable account or test Mac. Verify launch,
  persistence, source-file preservation and uninstall/reinstall behavior. No cloud
  build proves Gatekeeper/notarization, CoreAudio, Logic or hardware performance.
