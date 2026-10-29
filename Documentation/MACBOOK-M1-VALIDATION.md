# Wingman native MacBook Air M1 build and acceptance

This checkpoint was built and tested on **Linux**, not macOS. No compiled ARM64
AU artifact, successful `auval` result or Logic Pro loading result is claimed.
The standalone Jas PRO discovery workspace and the JASWINGMAN PRO AU are separate
products sharing repository functionality; the new discovery workspace is not
embedded inside the AU. This plugin is not declared production-ready.

## Build and install on the Mac

Install Xcode Command Line Tools (`xcode-select --install`) and, using Homebrew,
`brew install cmake python`. Close Logic before installation. Run a native Terminal
(`uname -m` must print `arm64`, not an Intel terminal under Rosetta).

```sh
mkdir -p "$HOME/Developer"
cd "$HOME/Developer"
git clone https://github.com/Jaspal84/wingman.git
cd wingman
git switch development/omniloop-eight
./Scripts/build-install-m1.sh
```

For an existing checkout, preserve local changes, then `git fetch origin`,
`git switch development/omniloop-eight` and `git pull --ff-only` instead of cloning.
JUCE is fetched at its pinned commit. macOS SDK SQLite must include FTS5.
The default two build jobs suit an M1 Air. The script runs the complete configured
suite; a failing test, install, signature or AU validation stops it with a nonzero
status. It verifies ARM64 in the actual AU executable and derives AU codes from
the compiled Info.plist rather than guessing identifiers. No sudo is needed.
Local ad-hoc signing is for development, not notarized public distribution.

Separate commands if diagnosing a failure:

```sh
WINGMAN_ARCHS=arm64 ./Scripts/build-macos.sh
./Scripts/install-macos.sh 'build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component' --adhoc
./Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component"
# Expected current identity: aumf / Wgm1 / Wgma; compiled metadata is authoritative.
/usr/bin/auval -v aumf Wgm1 Wgma
python3 Scripts/install-standalone-macos.py 'build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app'
./Scripts/launch-standalone-macos.sh
open 'build-macos/JasProStudio_artefacts/Release/Jas PRO Ultimate Loop Studio.app'
```

The AU installer backs up a previous component beside the installation with a
`.backup-<unique-id>` suffix. It does not delete project files, catalogues or AU
caches. To roll back, quit Logic, move the new component out of Components and
restore the saved component to its original name. Reopen Logic (or log out/in if
discovery remains stale). Do not erase user library data to troubleshoot discovery.

## Artifacts and evidence

- AU: `build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component`
- VST3 and plugin standalone: `build-macos/Wingman_artefacts/Release/`
- Jas PRO app: `build-macos/JasProStudio_artefacts/Release/Jas PRO Ultimate Loop Studio.app`
- Installed AU: `~/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component`
- Logs: `build-macos/validation/{build-tests,install,auval}.log` and `tests.xml`.

## Manual acceptance required on the Mac

Record the git SHA, macOS/Logic versions, device/buffer/rate, result and evidence
for each item. A completed script does not establish Logic compatibility.

1. In Logic Plug-in Manager, rescan JASWINGMAN PRO and retain the validation result.
   Its AU category is **music effect** (`aumf`), not instrument (`aumu`); load through
   a MIDI-controlled audio-effect instrument slot where offered. Confirm the
   editor opens, closes and reopens in native ARM64 Logic.
2. Route real audio and MIDI through the plugin. Generate MIDI and verify actual
   events, note-off pairing and audible output with the intended instrument.
   Test start/stop, cycle, tempo changes and seek against Logic's transport,
   including save/reopen with plugin state retained. Check for stuck notes.
3. In Jas PRO, import licensed local samples. Preview audio through CoreAudio;
   generate, lock, regenerate and replace compatible samples. Verify no-alternative
   and empty-library messages. Export WAV and sample-trigger MIDI (not audio-to-MIDI
   transcription), listen/import in Logic and verify duration and timing.
4. Send layers to Arrangement. Save and reopen a copy of an existing project;
   compare source paths, positions, role names, favorite/tag data and playback.
   Test undo/redo, sample replacement and reopening the same library repeatedly.
5. At 1100×740 and larger sizes, resize both dividers and scroll every layer.
   Test the built-in Retina display at default and scaled resolutions plus an
   external monitor when available. Check text, controls, waveform visibility,
   pointer targets, dialogs and moving between displays. Linux 2× offscreen
   rendering does not prove macOS Retina behavior.
6. Check MIDI controller input, audio device changes, sample-rate changes, repeated
   plugin instances and closing during background work. Record CPU and underruns
   with actual projects. Keep copies of projects used for compatibility checks.

## Validation boundary

Linux regression covers DSP, MIDI, transport, project persistence, library/FTS
migration and favorites, real sample combination/render/export, empty-content UI,
replacement feedback, minimum/large layouts and a 2× offscreen render. Linux AU
metadata tests use fixtures and do **not** execute Apple's validator. Installation,
ARM64 compilation/linking, CoreAudio, actual Retina behavior, AU registration,
`auval` and Logic synchronization remain **unrun until executed on a Mac**.
