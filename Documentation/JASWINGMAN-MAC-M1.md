# JASWINGMAN PRO — MacBook Air M1 runbook

These are local execution instructions. Linux tests do not establish that AU,
Logic Pro, MainStage, Retina behavior, device audio or M1 performance has passed.
The product display name changed; bundle ID, AU IDs and VST3 class IDs did not.
Existing Python/model directories stay under `WingmanPro` intentionally.

## Update and build (Terminal)

Close Logic, MainStage and old standalone instances before replacing plugins.
Keep local work: if `git status --short` shows changes, save/commit them before
pulling; do not reset or delete them.

```bash
cd "$HOME/Developer/wingman"
git status --short
git switch main
git pull --ff-only origin main
uname -m
xcode-select -p
```

Expected native architecture: `arm64`. If CLT is missing, run
`xcode-select --install`, complete its installer, then continue. If CMake/Ninja
are missing, install them with native Homebrew:

```bash
/opt/homebrew/bin/brew install cmake ninja
export PATH="/opt/homebrew/bin:$PATH"
./Scripts/build-macos.sh
```

The script builds arm64 Release AU, VST3 and standalone, then runs CTest. No model
installation or download is performed by the build. Artifacts:

- `build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component`
- `build-macos/Wingman_artefacts/Release/VST3/JASWINGMAN PRO.vst3`
- `build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app`

## Back up legacy AU names, install, validate (Terminal)

Do this **after a successful build**. The installer deliberately rejects duplicate
AU identities with different filenames. This block moves old user-installed
bundles to uniquely named backups outside Components; it does not delete them.

```bash
python3 - <<'PY'
from pathlib import Path
import uuid
root = Path.home() / 'Library/Audio/Plug-Ins/Components'
backup = Path.home() / 'Documents/JASWINGMAN-AU-Backup'
backup.mkdir(parents=True, exist_ok=True)
for name in ('Wingman.component', 'Wingman Pro.component'):
    source = root / name
    if source.exists():
        destination = backup / (name + '-' + uuid.uuid4().hex)
        source.rename(destination)
        print('Backed up:', destination)
PY

./Scripts/install-macos.sh \
  "build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component" --adhoc

set -o pipefail
mkdir -p mac-test-evidence
./Scripts/validate-au.sh \
  "$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component" \
  2>&1 | tee mac-test-evidence/au-validation.txt
```

The validation script reads the actual plist before codesign/architecture/auval
checks. Expected identity remains **aumf / Wgm1 / Wgma**, bundle
`com.wingmanaudio.wingman`. Ad-hoc signing is for local development, not distribution
notarization. If a conflicting old AU exists in `/Library/Audio/Plug-Ins/Components`,
resolve that system installation separately; the commands above do not modify it.
Restart the DAW after installation. Use Logic Plug-in Manager to inspect failures;
do not assume that compilation proves AU validation or discovery.

Optional user VST3 installation (preserve your existing installation first):

```bash
mkdir -p "$HOME/Library/Audio/Plug-Ins/VST3"
ditto "build-macos/Wingman_artefacts/Release/VST3/JASWINGMAN PRO.vst3" \
  "$HOME/Library/Audio/Plug-Ins/VST3/JASWINGMAN PRO.vst3"
python3 Scripts/migrate-standalone-settings.py
open "build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app"
```

The migration helper copies JUCE legacy standalone settings only when the new
settings file is absent. It preserves both files and is covered by portable
tests. Actual Mac preference restoration remains a manual check.

## Reuse the already installed AI runtime and model

Do not reinstall them. First verify the existing installation:

```bash
wingman_python="$HOME/Library/Application Support/WingmanPro/runtime-2.5.1/bin/python"
wingman_models="$HOME/Library/Application Support/WingmanPro/models"
"$wingman_python" --version
"$wingman_python" Scripts/stem_worker.py check --models "$wingman_models"
printf 'Python field: %s\nModels field: %s\n' "$wingman_python" "$wingman_models"
```

Paste the printed full paths into Model Manager's Python/model fields; folder
paths alone are not Terminal commands. Use Verify local model, then Import Audio
and Separate Stems in Stems & Tracks. A successful split replaces four stem lanes
and leaves Guitar independent. The new Guitar lane is not isolated by HTDemucs.

For an actual timed CLI test, supply a licensed **8–120 second WAV**, use a fresh
output name, and preserve the report:

```bash
/usr/bin/time -l "$wingman_python" Scripts/stem_worker.py separate \
  --models "$wingman_models" \
  --input "$HOME/Music/wingman-test.wav" \
  --output "$PWD/mac-test-evidence/stems-v1-run1" \
  2>&1 | tee mac-test-evidence/inference-v1-run1.txt

"$wingman_python" Scripts/validate_stems.py \
  "$HOME/Music/wingman-test.wav" \
  mac-test-evidence/stems-v1-run1 \
  --report mac-test-evidence/stem-quality-v1-run1.json
```

Nonempty output folders are rejected. Repeats require another output name. The
worker's benchmark includes elapsed/inference time and worker RSS; DAW/parent
memory is additional. This CPU pathway is offline, not CoreML or a realtime M1
performance guarantee. The pinned checkpoint is 955717e8, SHA-256
`8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4`,
with verified MIT provenance. No silent weight download occurs.

To include validator and actual plugin-controller inference tests on Mac:

```bash
./Scripts/build-macos.sh -DWINGMAN_INFERENCE_PYTHON="$wingman_python"
WINGMAN_REAL_MODELS="$wingman_models" \
WINGMAN_REAL_PYTHON="$wingman_python" \
WINGMAN_REAL_INPUT="$HOME/Music/wingman-test.wav" \
ctest --test-dir build-macos -V
```

## Manual Logic / MainStage acceptance checklist

1. Locate JASWINGMAN PRO in Logic's AU MIDI-controlled effect/instrument routing
   appropriate to `aumf`. Open a copy of a prior Wingman project and verify state
   recall, audio and MIDI routing. Do not infer native MIDI-out support solely
   from the plugin's `producesMidi()` flag.
2. Import WAV/AIFF/FLAC/MP3 (M4A where the installed decoder supports it). Verify
   all five lanes, recording with headphones/input configured, playback, pan,
   volume, solo/mute, mix export and independent per-track WAV drag. Check old
   imported lanes can drag without running separation.
3. Hold and move Drag at least six pixels, then drop audio into Logic audio tracks.
   Copy audio into the Logic project before saving; cache files are temporary.
4. Generate chord, bass, drum, melody and Guitar MIDI. Drop each into a Logic
   Software Instrument track. Confirm editable Piano Roll regions with matching
   pitches, velocities, note lengths, string/drum channels and positions. Test
   Export MIDI/import fallback if a native drag is rejected. Pick a guitar
   instrument in Logic; MIDI does not carry an acoustic sample library.
5. Guitar Studio: try all techniques, styles, types, energy/density/swing,
   humanization, four variations, seed and lengths. Preview renders **basic
   original synthesis**; it plays through the current session mixer. Solo Guitar
   to hear it alone. Render at 44.1 and 48k, check waveform/export/drag, cancel and
   change harmony/tempo while rendering. Do not rate these timbres as realistic
   acoustic instruments without listening tests.
6. Tempo & Analysis: compare imported real music against known BPM, use half/double,
   Tap Tempo, manual correction and Apply Detected Tempo. Silence must not show a
   measured default. Confidence is heuristic. Validate beat alignment, multiple
   genres and DAW Sync. Guitar meter selection is manual, not detected meter.
7. Chords & Ideas: edit extended/slash chords, generate cinematic style rules,
   toggle voice leading and export. Chord detection still models the original
   eight qualities; it is not a detector of every supported extension.
8. Test separation cancellation, corrupt/missing audio/model paths, editor
   close/reopen, plugin removal during inference/render, state saves and old
   four-track sessions. Host sandbox/file permissions and security-bookmark
   persistence need testing; persistent security bookmarks are not implemented.
9. Inspect every view at minimum/default/maximum sizes and Retina scale. Test
   keyboard navigation, parameter entry and file chooser dialogs.
10. Record M1 model time/RSS, total DAW memory, audio dropouts, device sample rate,
    buffer size, thermal behavior and guitar render time. Repeat in MainStage
    independently. Save auval logs and plugin/version/hardware details.

Known engineering limits: session locks can skip audio blocks during publication
or state changes; host MIDI-buffer zero-allocation behavior is not proven; legacy
lane import/export and embedded large state operations can block callers. Waveform
resizing now uses prepared peaks. Child-process restrictions may prevent inference
in some hosts. This is a development build, not a validated v1 release.
