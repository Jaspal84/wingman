# MacBook Air M1 execution runbook

**Status: instructions only. No AU/Logic/MainStage/M1 test has been executed by the Linux cloud agent.** Linux inference results are in INFERENCE-VALIDATION.md and cannot predict M1 timing, memory pressure or DAW behavior.

## 0. Download or update the repository

Open Terminal on the MacBook Air M1. Check native execution:

```sh
uname -m
# Expected: arm64. If x86_64 is shown, start a native shell:
/usr/bin/arch -arm64 /bin/zsh
```

Download the repository into a new directory:

```sh
mkdir -p "$HOME/Developer"
cd "$HOME/Developer"
git clone --branch main https://github.com/Jaspal84/wingman.git
cd wingman
```

If you already have that checkout, use these commands **instead of cloning**:

```sh
cd "$HOME/Developer/wingman"
git status --short
git switch main
git pull --ff-only origin main
git log -1 --format='%H %s'
```

Preserve/commit your own local changes before updating. Do not reset or overwrite them. If fast-forward fails, inspect the divergence rather than forcing the update.

Install missing prerequisites. Xcode Command Line Tools installation opens an Apple installer; let it finish before continuing:

```sh
xcode-select --install
```

If those tools are already installed, verify them with `xcode-select -p` and skip that installer command. With native Homebrew installed from https://brew.sh, install the remaining tools:

```sh
/opt/homebrew/bin/brew install cmake ninja python@3.12
export PATH="/opt/homebrew/bin:$PATH"
cmake --version
python3.12 --version
xcrun --sdk macosx --show-sdk-path
```

Homebrew is only a prerequisite installer; the project build uses CMake/JUCE. No model is downloaded by building the plugin.

## 1. Native tools and evidence directory

Use a native Terminal session, not Rosetta. Install Xcode Command Line Tools, CMake and native Python 3.12. From the repository:

```sh
uname -m                    # Must report arm64 for a native run
sw_vers
xcode-select -p
cmake --version
python3.12 -c 'import platform; print(platform.machine())'
mkdir -p mac-test-evidence
{
  date -u
  uname -m
  sw_vers
  /usr/sbin/sysctl -n hw.memsize
  /usr/sbin/sysctl -n hw.ncpu
  xcrun clang++ --version
  cmake --version
} > mac-test-evidence/system.txt 2>&1
```

Record whether the machine has 8 or 16 GB, power/battery mode, other running audio/model workloads and the git commit under test. Do not use system_profiler dumps containing hardware serial numbers for the report.

## 2. Build, install and validate the compiled AU

```sh
set -o pipefail
./Scripts/build-macos.sh 2>&1 | tee mac-test-evidence/build.log
python3 Scripts/macos_au.py inspect \
  'build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component' \
  > mac-test-evidence/compiled-au.json
./Scripts/install-macos.sh \
  'build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component' --adhoc \
  2>&1 | tee mac-test-evidence/install.log
./Scripts/validate-au.sh \
  "$HOME/Library/Audio/Plug-Ins/Components/Wingman Pro.component" \
  2>&1 | tee mac-test-evidence/auval.log
/usr/bin/auval -a > mac-test-evidence/registered-components.txt
```

The helper derives type/subtype/manufacturer from the actual installed plist and checks codesign/lipo before auval. Expected identity is Music Effect `aumf`, `Wgm1`, `Wgma`, bundle `com.wingmanaudio.wingman`; do not validate the old guessed `aufx` type. Do not mark AU validation passed unless the actual auval command exits zero with its full validation success output. `pipefail` preserves failures through tee. Ad-hoc signing is local development only; distribution signing/notarization remains separate.

The installer preserves a previous same-name component and refuses conflicting old differently named components with the same identity. Move confirmed obsolete Wingman copies to a recoverable backup outside Components. Close/reopen the DAW and rescan in Plug-in Manager. Follow AU-VALIDATION.md if discovery still fails; do not delete unrelated AU caches.

You can launch the built standalone separately:

```sh
open 'build-macos/Wingman_artefacts/Release/Standalone/Wingman Pro.app'
open -a 'Logic Pro'
# If MainStage is installed, for its separate test:
open -a MainStage
```

## 3. Logic Pro test matrix

Create a new disposable project and record Logic/macOS versions and native/Rosetta mode. Check Plug-in Manager for the actual manufacturer/name and validation status. A MIDI-capable Music Effect may be listed under **AU MIDI-controlled Effects in a software-instrument channel's Instrument slot**, rather than as an ordinary Audio FX insert. Verify the category/routing on the installed version; do not change the component identity just to move menus.

- Load the plugin, resize the editor, change project name and reopen it. Verify pan/window/session state.
- Import four known WAVs; check actual playback, mono/stereo routing, gain, mute, multi-solo and left/right balance. Test 44.1/48/96-kHz projects and 64/128/256/512-sample device buffers.
- Verify hardware or host-input recording with the required permissions and actual input route. Use an independent source to confirm the captured waveform is the input rather than silence. For a MIDI-controlled effect, inspect the host's available audio/side-chain routing; input connectivity must be measured, not assumed.
- Enable DAW sync; test start/stop, seek into a held chord, loop wrap, tempo change, negative pre-roll and host position loss. Check no duplicate/hanging notes and the waveform/chord transport alignment. Test bypass during a held chord.
- Generate/export chords, bass, drums and melody. Reimport the MIDI file into Logic and compare notes/timing/tempo. Test external file drag separately. **Arbitrary AU Music Effect MIDI-out routing to another instrument is not guaranteed by Logic**; file export is the verified transfer fallback. Do not report live routing passed unless observed.
- Save/close/reopen the Logic project and compare embedded clips, mix, progression, edited/generated MIDI, model paths and UI state. Restore an older v1 project too.
- Run separation while audio plays. Use Logic's audio performance meter and an independent recording to detect dropouts. The known try-lock and synchronous file/state work can still cause silent blocks; a successful inference does not clear these real-time limitations.

Generators currently assume 4/4; displaying another host meter does not imply meter-aware generation. Audio alignment across historical tempo maps and sample-accurate tempo ramps needs explicit testing.

## 4. MainStage

Use a disposable concert. Confirm component discovery, strip category, actual input/output routing, patch changes, held-note release and saved-state restoration. MainStage may not supply a running musical PPQ clock in a live strip. Check the plugin's status; if PPQ is unavailable, verify safe stop and use local transport with DAW sync off. Do not claim Logic results establish MainStage support.

## 5. Run the real AI test and native M1 benchmark

Use your own or otherwise appropriately licensed stereo WAV (8-192 kHz, at most 120 seconds), ideally with clearly audible vocals, bass and drums. The upstream Linux fixture is weak evidence for vocal/bass quality. Preserve input SHA-256, duration, sample rate and model manifest. Do not publish private music with a benchmark.

```sh
./Scripts/setup-inference.sh \
  "$HOME/Library/Application Support/WingmanPro/runtime-2.5.1"
wingman_python="$HOME/Library/Application Support/WingmanPro/runtime-2.5.1/bin/python"
wingman_models="$HOME/Library/Application Support/WingmanPro/models"
# Set this to an existing, appropriately licensed WAV on your Mac:
wingman_input="$HOME/Music/wingman-test.wav"
test -f "$wingman_input"
"$wingman_python" -c 'import platform,torch,torchaudio,demucs; print(platform.machine(),torch.__version__,torchaudio.__version__,demucs.__version__)'
"$wingman_python" Scripts/stem_worker.py install --models "$wingman_models" \
  --sha256 8726e21a993978c7ba086d3872e7608d7d5bfca646ca4aca459ffda844faa8b4 \
  --allow-download --accept-mit-license
"$wingman_python" Scripts/stem_worker.py check --models "$wingman_models"
"$wingman_python" Scripts/stem_worker.py separate --models "$wingman_models" \
  --input "$wingman_input" \
  --output "$PWD/mac-test-evidence/stems-run-1" \
  --status "$PWD/mac-test-evidence/separation-status.json"
"$wingman_python" Scripts/validate_stems.py "$wingman_input" \
  mac-test-evidence/stems-run-1 --report mac-test-evidence/stem-validation.json
```

To reproduce the exact cloud music fixture for local evaluation, you can use this optional block after installing the runtime. It is the upstream repository test file, not a production sample pack or an isolated-source reference set:

```sh
curl --fail --location \
  https://raw.githubusercontent.com/facebookresearch/demucs/ef66d254cd6d558e207eeff2c4b8d053db2e77dd/test.mp3 \
  --output mac-test-evidence/upstream-test.mp3
printf '%s  %s\n' \
  aa8d8e51ca1b65e4e04f0be52c7e63b38c17aaaeded9193bc6ce0acf6d6a6d9a \
  mac-test-evidence/upstream-test.mp3 | shasum -a 256 -c -
"$wingman_python" -c 'import soundfile as sf,sys; a,r=sf.read(sys.argv[1],dtype="float32",always_2d=True); sf.write(sys.argv[2],a,r,subtype="PCM_24")' \
  mac-test-evidence/upstream-test.mp3 mac-test-evidence/upstream-test.wav
wingman_input="$PWD/mac-test-evidence/upstream-test.wav"
"$wingman_python" Scripts/stem_worker.py separate --models "$wingman_models" \
  --input "$wingman_input" --output "$PWD/mac-test-evidence/upstream-stems"
"$wingman_python" Scripts/validate_stems.py "$wingman_input" \
  mac-test-evidence/upstream-stems --report mac-test-evidence/upstream-validation.json
```

The upstream fixture produced very quiet vocal/bass estimates in Linux, so also use a vocal-rich licensed recording before judging those sources. Do not redistribute the fixture or its derived audio as project assets.

The setup helper requires a new runtime directory and preserves existing environments. If weights already exist, use `check`; installer deliberately refuses to overwrite. Native macOS runtime pins have not been installed/tested here; retain any actual pip/import errors.

Run a fresh process for each cold/warm comparison and use a new empty output directory. Retain benchmark.json (worker end-to-end time and peak RSS in bytes), model manifest, validation JSON, hardware facts and commit. The worker uses two CPU compute threads, four-second outer segments, 25% overlap and HTDemucs's observed 7.8-second trained context. Peak RSS covers the worker, not the DAW/GUI plus child combined. Measure total memory pressure/swap in Activity Monitor while the plugin and DAW run, especially on 8-GB machines. Do not infer suitability from the Linux 1.5–1.6-GB worker peak.

Also test a mono/48-kHz input and cancellation after progress first becomes positive. Through Model Manager verify immediate owned-child termination, no partial lane replacement and retained previous audio. Offline `check`/`separate` must work with installed weights. Keep checksum-corruption tests on a copied installation, not your working checkpoint.

For the opt-in embedded-worker regression, use an installed model and a real WAV:

```sh
cmake -S . -B build-macos -DWINGMAN_INFERENCE_PYTHON="$wingman_python"
WINGMAN_REAL_MODELS="$wingman_models" \
WINGMAN_REAL_PYTHON="$wingman_python" \
WINGMAN_REAL_INPUT="$wingman_input" \
ctest --test-dir build-macos -R '^integration$' -V \
  2>&1 | tee mac-test-evidence/real-plugin-inference.log
```

Use an 8–120-second input for this cancellation regression. This opt-in test runs the model twice (one completed run, then cancellation after a completed chunk). Do not run concurrent copies. Normal unit tests do not silently download weights. Keep CoreML/ONNX disabled until a compatible conversion/runtime and equivalent quality are actually tested; this implementation uses native PyTorch CPU.

## 6. Results table to complete on the Mac

Record pass/fail/unrun plus logs for: native build, actual auval, Logic discovery, MainStage discovery, physical recording/playback, buffer/sample-rate matrix, transport/loop/bypass, MIDI file/drag/live routing, old/new state, model install/offline inference, stem quality, cancellation, combined 8-GB memory pressure, and sustained audio during model/file/state work. Missing isolated reference stems prevents a source SDR/SI-SDR accuracy claim; mixture-sum SNR is a different metric. Native M1 numbers should remain blank until measured.

## Automated versus physical-device results

Passed in Linux: both VST3/Standalone builds, all ordinary suites plus the optional stem-validator suite, genuine HTDemucs processing of a 20-second stereo music fixture, mono/48-kHz conversion, plugin-managed stem import/export, and supervised cancellation after inference began. The final opt-in regression log and exact model digest are recorded in INFERENCE-VALIDATION.md. These outcomes do not establish vocal-rich recording quality, AU validity or native M1 speed.

Requires this Mac: native arm64 compilation and runtime import; actual codesign/lipo/auval checks; Logic and MainStage discovery/routing; physical CoreAudio recording/playback; real host tempo/loop behavior and live MIDI routing; Retina/accessibility; combined 8-GB memory pressure and uninterrupted audio during inference. Mark these pass/fail only after executing them, retaining logs. Current model backend is PyTorch CPU; CoreML/ONNX remain unverified.
