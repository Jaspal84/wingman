# Stem Splitter and external MIDI dragging

The main editor has a framed Stem Splitter row with Import Audio, Separate
Stems, a graphical progress bar, status text and Cancel. Select audio or drop one
file on the editor, then press Separate Stems. Successful separation replaces all four lanes.
Configure the local Python runtime and model directory in Model Manager first;
model installation remains a separate explicit-consent action. No automatic
model download occurs when selecting, dropping, or separating audio.

The registered JUCE decoders handle WAV, AIFF, FLAC and MP3; macOS may additionally
supply M4A decoding through CoreAudio. Unsupported or protected files fail with an
error. The worker thread decodes in 8192-frame blocks to PCM24 WAV in a private,
unique job directory before launching the existing verified HTDemucs CPU worker.
Inputs must be mono/stereo, 8–192 kHz, and no longer than 120 seconds. Decoder
cancellation is checked between blocks. Input files are never modified.

The panel shows worker progress, completion and errors. Cancel terminates the
child process and removes job files after it exits. The processor owns the job:
closing/reopening the editor does not leave callbacks to a deleted editor. Plugin
destruction cancels and joins the job, which can still delay teardown. This is
not a claim of tested AU sandbox compatibility. Restricted hosts may deny child
process execution or file access; configure accessible local paths or run the
standalone application. Permission/security bookmark persistence has not been
implemented. Files must be selected again if access is revoked.

All four outputs must be readable stereo PCM24 WAV at 44100 Hz, match reported
frame count, and pass the existing transactional stem importer (equal durations,
finite decoded samples) before success. They become the four preview/mixer lanes.
Solo, mute, pan, volume and Play operate on those lanes. Export buttons write the
lane audio with mixer settings. Stem Drag uses the **raw separated WAV**, before
volume/pan. It is intended for an audio track in Logic, not an instrument track.
For lanes imported independently, use Export WAV instead.

External MIDI dragging previously started from TextButton::onClick (mouse
released) and removed its file in the drag completion callback. The replacement
starts from mouseDrag after a six-pixel held gesture. MIDI and stem files are
retained in unique subdirectories of the OS temporary `Wingman-DragFiles` folder;
there is deliberately no completion-callback deletion because hosts can read
files later. A processor-start background pass removes UUID directories older
than seven days, checks at most 4096 entries and removes at most 128 directories
per pass. Recent exports, unrelated directory names and symlinks are skipped.
Every drag refreshes its directory timestamp. Cleanup is opportunistic at
plugin/application startup; recent exports are not evicted to enforce a hard disk
quota. The OS can also remove temporary files. Drag again to regenerate
MIDI, or re-separate/Export WAV if raw stems expire. Quit DAWs before manually
removing this folder. Files are not intended as permanent project storage; save
exports and ask the DAW to copy imported audio into its project.

MIDI uses JUCE MidiFile/MidiMessageSequence, 960 ticks per quarter, tempo and 4/4
metadata, a descriptive track name, and per-note channels, velocities, attacks
and releases. Export MIDI remains available if native external dragging cannot
start or the host rejects it. A successful native drag-start return value does
not prove that Logic accepted the region. Generated MIDI carries notes rather
than rendered audio and should remain editable in hosts supporting SMF import.

## Build and automated verification

Linux uses the existing CMake build and full CTest suites. The integration suite
checks exact SMF pitch/channel/velocity/timing/duration, all three generated parts,
chord export, editor rendering, and the optional real worker integration.

```bash
cmake -S . -B build -DJUCE_SOURCE_DIR=/path/to/JUCE \
  -DWINGMAN_INFERENCE_PYTHON=/path/to/runtime/bin/python
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

To actually execute inference and cancellation, use an installed, verified model
and a real WAV fixture lasting 8–120 seconds:

```bash
WINGMAN_REAL_MODELS=/absolute/path/to/models \
WINGMAN_REAL_PYTHON=/absolute/path/to/runtime/bin/python \
WINGMAN_REAL_INPUT=/absolute/path/to/music.wav \
ctest --test-dir build -V
```

The opt-in integration converts the real fixture to AIFF, tests background decode
and actual separation, validates aligned exports, checks retained raw files, then
cancels another inference after a completed chunk and verifies existing lanes
survive. Without these selectors, CTest does not run real inference.

## Manual Mac / Logic / MainStage checklist — not executed in Linux

Build, install and validate AU with the existing scripts (AU, VST3 and standalone
are macOS build targets). Close the old application/plugin before rebuilding:

```bash
cd "$HOME/Developer/wingman"
git pull --ff-only origin main
./Scripts/build-macos.sh
./Scripts/install-macos.sh "build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component" --adhoc
./Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/Wingman Pro.component"
```

1. Record macOS, Logic/MainStage and plugin versions, Apple Silicon architecture,
   AU validation exit code, and available RAM. See MACBOOK-M1-TESTING.md for runtime
   installation and model checksum details.
2. Open Wingman in standalone, then in Logic's supported AU MIDI-controlled effect
   slot. Verify the splitter row and all previous controls fit at min/default/max
   window sizes and Retina scale. Select and drop WAV/AIFF/FLAC/MP3; try M4A and
   report actual decoder support. Check missing, unreadable, protected, >120s and
   unsupported files show errors without replacing lanes.
3. Use a licensed 10–20s vocal-rich song. Verify progress, cancellation, retry,
   all four stems, solo/mute/pan/volume, and individual/mix exports. Listen for
   leakage, transients, alignment and clipping; reconstruction alone does not
   establish perceptual source separation quality.
4. Drag each raw stem onto a Logic audio track. Confirm region duration/rate,
   waveform and playback; save with project assets copied. Reopen the project.
5. Generate Chord MIDI, Bass MIDI, Drum MIDI and Melody MIDI separately. Hold
   each Drag button, move at least six pixels, and drop into a Software Instrument
   track's Tracks area. Confirm a MIDI region appears (not an audio region).
6. Open Piano Roll: compare pitch, note duration, velocity and positions with the
   generated pattern. Check drum channel 10, chords/inversions, non-120 BPM,
   multiple bars, stopped/running transport, repeat/cancelled drags, and dropping
   after editor closure. Check tempo import according to Logic's project settings.
7. If drag fails, Export MIDI and import the same .mid through Logic File > Import
   > MIDI File. Record the exact failure and whether fallback works. Do not mark
   external drag as passed based only on successful file export.
8. Close/reopen the editor during inference, cancel/close the plugin, remove and
   reinsert the AU, reopen a saved project, and test paths with spaces. Observe UI
   responsiveness, dropouts and permission denials. Test MainStage independently;
   it is not implied by Logic passing.
9. Capture M1 inference elapsed time/worker RSS and DAW memory/CPU/dropout readings.
   CPU inference is offline, not a realtime or CoreML performance guarantee.

Existing limitations remain: session locking may silence audio blocks during
publication/state changes, import/export and waveform rebuilding outside the new
splitter path may block the UI, and there is no proof of zero allocations in the
host MIDI buffer. Native external dragging, AU lifecycle/sandbox behavior and M1
performance require physical Mac verification before a release claim.

## Executed Linux results for the initial splitter update (805f2a2)

- GCC 14 / JUCE 8.0.6: VST3, standalone and integration executable built.
- Full CTest: **8/8 passed in 69.57 s**; integration **775 checks in 57.40 s**.
- Python worker suite: **12 tests**, including consent, checkpoint/license checks,
  missing input, existing output preservation and cancellation before loading.
- Actual upstream real-music fixture: 20 s stereo at 44100 Hz; converted to AIFF
  by the test and decoded back to WAV by the controller before real inference.
- Four readable, finite, aligned raw WAVs: **882000 frames, stereo 44100 Hz PCM24**;
  plugin mix/export and retention assertions passed. Cancellation after a genuine
  inference chunk returned in **100.228 ms**, preserving previously imported lanes.
- Worker end-to-end **39.3217 s**, model inference **37.1572 s**, peak worker RSS
  **1603452928 bytes** (1.49 GiB). Parent/DAW memory is additional. Benchmark JSON:
  `Validation/stem-splitter-aiff-benchmark.json`.
- Exact note timing, lengths, velocities and channels survived SMF round trips for
  bass, drums and melody; existing chord export tests also passed.
- Editor rasterization reviewed; shell syntax and Python compilation checks passed.
- No AU build/auval, physical audio device test, Logic/MainStage drag acceptance,
  sandbox permission test, subjective separation listening, or M1 benchmark was
  executed here. Automated reconstruction and finite-sample checks do not prove
  source isolation quality. The fixture has quiet vocal/bass estimates; use a
  vocal-rich licensed recording for manual quality assessment.

## Follow-up: graphical progress, retention and responsive layout

`Files/DragFileStore.h` now centralizes cache creation and background expiry for
MIDI and WAV drag files. The parent cache has owner-only permissions. The worker
uses this store for raw stems; the editor uses it for the selected MIDI pattern.
`FileDragButton` refreshes retention when a gesture begins. No model installation
or download is performed by these changes.

Integration tests additionally exercise bounded cleanup, preservation of recent
host-import files and unrelated directories, and all five drag buttons at
960×660, 1200×760, 1920×1318 and 960×1216. They also assert that Import Audio,
Separate Stems, graphical progress and Export MIDI are present. Existing exact
SMF timing/note pairing tests and actual inference opt-in remain enabled.

The old `(1090,680)` coordinates are design-space positions, transformed with all
other controls using the editor size. Tests check transformed bounds rather than
assuming those numbers are physical pixels. GUI behavior still requires Retina
and native host verification on a Mac.

Follow-up verification on Linux: VST3 and standalone rebuilt; **8/8 CTest suites
passed in 70.86 seconds**, with **804 integration checks**. The 20-second real
music fixture passed background AIFF conversion, genuine HTDemucs inference,
aligned raw/exported WAV checks, and cancellation after an inference chunk
(**100.207 ms** cancellation latency). Raw-output structural validation passed;
mixture reconstruction SNR was **31.98 dB**. Quiet vocal/bass warnings remain,
and this does not constitute perceptual source-isolation testing. Worker elapsed
time was **36.8069 seconds**, peak RSS **1548562432 bytes**. Evidence is in
`Validation/stem-splitter-progress-benchmark.json` and
`Validation/stem-splitter-progress-quality.json`. Graphical splitter rendering
was inspected. macOS AU builds/auval, real Logic/MainStage drag acceptance and
Apple M1 performance remain unexecuted.
