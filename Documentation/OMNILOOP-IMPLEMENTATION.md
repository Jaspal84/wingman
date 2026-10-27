# Jas PRO Ultimate Loop Studio / OmniLoop 8

This is a separate, early standalone product. It does not replace JASWINGMAN PRO,
change its AU/VST3 identifiers, migrate its sessions, or advertise unfinished AI.
The existing plugin remains the Guitar Studio and stem-separation product while
those capabilities are integrated into the new workstation in later phases.

## Repository audit and dependencies

Existing reusable modules: immutable snapshot playback, device-backed JUCE
standalone, local format decoding, waveform peaks, MIDI scheduling/export/drag
preparation, host telemetry, deterministic analysis, SFZ guitar voices and offline
rendering, verified external HTDemucs worker. Existing compatibility identifiers
remain `aumf Wgm1 Wgma`, bundle `com.wingmanaudio.wingman`.

The old engine had five lanes and one clip per lane. The mixer is now parameterized
by track count, retaining its five-track alias and adding an eight-track alias.
Publication/reclamation occurs on the control thread; the single audio reader
uses a hazard pointer. No decoder, worker, file dialog or disk writer runs inside
the audio callback. Gain changes are not yet smoothed; clipping protection clamps
peaks and may distort an overdriven mix. Rate conversion is linear interpolation,
not independent time stretching. UI workers are cancelled/joined before teardown.

## Implemented initial standalone

`JasProStudio` has its own bundle ID `com.wingmanaudio.jaspro.studio` and product
name Jas PRO Ultimate Loop Studio. Eight lanes: Kick, Snare, Hi-Hat, Bass, Chords,
Melody, Vocals and FX. Each has background WAV/AIFF/FLAC decoding, cached waveform,
mute, solo, gain and pan. Drop a single audio file onto its lane to import; click
the waveform area to seek. Play, Stop, Loop and Audio device operate real playback.
Export WAV renders the mixer off-thread to 48 kHz stereo PCM24, publishes through
a temporary file and reports write failures. Imports are mono/stereo 8–192 kHz,
maximum 30 seconds each; all decoded audio is resident in memory. Worst-case eight
192 kHz stereo imports require about 352 MiB before replacements/worker buffers.
No sample recordings are bundled. Save/Open project persist source-file references,
mixer values and looping. Loading validates the complete replacement before
publishing; missing assets leave the current project intact. Keep original audio
files in place; this is not a portable self-contained archive.

Current standalone does **not** edit multiple clips, stretch audio,
change project BPM, record, index a sample browser, generate arrangements, host
instruments/effects or provide cloud sync. It intentionally exposes only working
controls. The old application continues to provide its existing functions.

## Build and launch on Apple Silicon

Close neither nor replace the existing app until the new target is auditioned.
From a checkout containing this branch:

```bash
cd "$HOME/Developer/wingman"
git fetch origin
git switch development/omniloop-eight
git pull --ff-only origin development/omniloop-eight
cmake -S . -B build-omniloop -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DWINGMAN_BUILD_STUDIO=ON
cmake --build build-omniloop --config Release --parallel 2
ctest --test-dir build-omniloop -C Release --output-on-failure
open "build-omniloop/JasProStudio_artefacts/Release/Jas PRO Ultimate Loop Studio.app"
```

Existing AU, VST3 and standalone remain separate build targets. A native Mac build
and `arch -arm64 /usr/bin/auval -v aumf Wgm1 Wgma` must be executed on the Mac;
Linux results do not validate Audio Units, CoreAudio, Logic or MainStage. This new
standalone is not an AU. No app is installed or overwritten by these commands.

Manual audition: choose Audio device, import a short licensed file into every
lane, play all eight, solo FX, test mute and hard pan, loop across the end, export
and listen to the WAV in a separate player. Verify device changes and shutdown
while importing/exporting. Device rate follows the selected device; export is
fixed at 48 kHz. Native M1 CPU, memory, dropouts and latency are unmeasured.

## Dependency-aware remaining phases

1. Audit completed; baseline eight-track playback surface implemented.
2. Arrangement: clip model with immutable assets, sample scheduling, non-destructive
   trim/split/move/copy/reverse/fades, undo/redo, bounded command delivery, versioned
   project extensions beyond the initial single-clip schema. Gain ramps and bounded background task queue.
3. Browser: SQLite WAL migrations, prepared indexing/search, hash duplicates,
   cancellable folder scanning, file-change handling, audition routing. Do not
   invent genre/mood labels where no classifier exists.
4. Stretch/pitch adapter with licensing review, stereo/transient and timing tests.
5. Reproducible ranking and Magic Arrange built on indexed, licensed source loops.
6. Integrate existing Guitar/SFZ and MIDI exports; add sampled Drum and Play engines.
7. Effects and Flip with automation/state and offline parity tests.
8. Reuse existing verified worker/analysis, map four stems explicitly; no separate
   guitar claim. Model Manager path whitespace is trimmed before configuration.
9. Bridge/Listen need separate stable plugin IDs and bounded timestamped IPC, tested
   disconnect/reconnect and host lifecycle. No remote networking in audio callback.
10. Cloud requires a real authenticated service and account/security specification;
    do not fake entitlements, endpoints or proprietary catalog access.
11. Physical Mac AU/Logic/MainStage/audio-device/Retina/M1 validation and listening.

ARA2, AAX, AUv3, proprietary stretch SDKs and commercial sample services are not
implemented or claimed; they require their respective SDK and license verification.

## Executed Linux validation

The final Debug build passed 20/20 CTest suites in 25.34 seconds. New assertions
cover all eight lanes, stereo rate conversion, mute/solo/pan/loop/clipping bounds,
project save/reopen, missing-file and cancelled restore safety, PCM24 export sample
values/duration, and cancellation preserving a pre-existing WAV. The offscreen
GUI self-test covers three window sizes and fails on logged JUCE assertions.
The final log contains no JUCE assertion failures. ALSA MIDI hardware and writable
font caches are unavailable here; no audible device playback is claimed.

Existing guitar, sampled voices, MIDI, analysis, host timing, state, worker and
validation tests passed. Worker tests are not a new real-inference run.
[Raw CTest evidence](Validation/omniloop-linux-tests.xml) and
[binary hashes and test inventory](Validation/omniloop-linux-tests.json) are recorded.
The new standalone, existing Linux standalone and VST3 built successfully. No
macOS binary, AU validation, physical device audition or Logic host test was run.

Changed files: CMakeLists.txt (separate app/test target), PlaybackEngine.h (track
count parameter), ModelPanel.h (path trimming), Studio/Main.cpp (working UI/device
playback/background jobs), Studio/Project.h (decode/persistence/render helpers),
IntegrationTests.cpp (new routing/file tests), README and this validation report.
