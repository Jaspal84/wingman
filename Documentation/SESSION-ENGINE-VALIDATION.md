# Session engine validation — 2026-10-25

This report covers changes after `f1913c024afda29cbc7b88c346acef64efb0f4c3`.
Native AU, Logic Pro, MainStage, CoreAudio devices and Apple M1 benchmarks have not
been executed in this Linux workspace. The user's report that an earlier 1.0.0 AU
passed does not validate this revision.

## Stable stages

| Stage | Commit | Verified scope |
| --- | --- | --- |
| 1 | f1ee977 | Five-lane playback, endpoint restart, seek/loop, immutable audio publication |
| 2 | 9a747bc | Partial host timing, play/stop/loop, independent BPM preservation |
| 3 | b3caa64 | Queued import analysis, explicit Apply, anti-phase input, independent recording capture |
| 4 | 584a3bf | Editable detected regions and time-aligned song-follow guitar |
| 5 | a55986e | Live SFZ voices, articulation cues, state compatibility |
| 6 | 9bc9f9e | Standalone build checks and separate install/launch tools |
| 7 | final commit | Portable regression validation, fully audio-owned MIDI scheduling/host telemetry, Mac runbook; native execution pending |

## Functional evidence

- WAV import and real nonzero processBlock output for all five lanes independently
  and together at 44.1/48 kHz; mono decoding, gain, pan, mute/solo, Stop/rewind,
  waveform seek, endpoint restart and looping. Concurrent session edits no longer
  silence imported audio.
- Audio-owned immutable MIDI sequences preserve scheduling when editors or analysis
  publish results. Existing tests verify MIDI onset offsets, matching releases,
  loop boundaries, seek/chase, host stop and bypass. Host-loop audio and MIDI wrap
  at the same tested sample. Host-owned MidiBuffer allocation behavior remains a
  native instrumentation task; no blanket zero-allocation host claim is made.
- Mock hosts exercise PPQ, seconds-only and sample-position timing, missing fields,
  retained BPM/meter, synchronized stop and independent BPM restoration.
- Actual generated input is captured even while session updates occur; no disk work
  or inference is performed in the recording callback.
- Synthetic rhythm tests include 80, 90, 100, 120 and 140 BPM with right-channel-only
  stereo input, confidence/half/double candidates, cancellation and silence cases.
- Existing chroma/template tests cover 96 held-chord combinations and 24 major/minor
  cadence keys. Stereo anti-phase integration input analyzes correctly. Applying a
  detected D key retains manual chart transposition; applying absolute detected D
  chords avoids a second transposition.
- Detected MIDI export uses actual region timestamps. Guitar song-follow tests
  verify silent gaps, chord pitches, playable strings, note-end boundaries, odd bar
  counts and cancellation. Manual detected-chord corrections retain timing.
- Live SFZ tests verify exact MIDI attack offsets at 44.1/48 kHz, stereo output,
  note-off completion and a sample-by-sample live/offline fixture comparison.
  Existing sampled tests retain velocity-layer, round-robin, release mapping,
  cancellation, missing sample and unsupported opcode checks. Generated guitar
  state retains optional articulation metadata and accepts older states.
- Integration tests export/read stereo PCM24 WAV and SMF, restore five-lane and
  legacy four-lane sessions, paint real JUCE views and exercise actual GUI callbacks.
- Standalone installer tests validate bundle metadata/executable paths and reject
  native installation on Linux. They do not exercise macOS ditto, lipo, codesign,
  application launch or physical audio output.

## Builds and limitations

Linux VST3 and standalone are built with C++20/JUCE 8.0.6. Portable plugin-off tests
also build without JUCE. No AU target or auval exists on this Linux executor.
Real Demucs inference was not rerun for these changes; its worker was not modified.
Existing worker/model/stem-validation tests run, while the integration suite
explicitly reports when opt-in real inference is not requested.

All sampled test audio is synthetic test data. No guitar recordings, commercial
libraries or model weights were downloaded or bundled. A user-licensed SFZ is
required for recorded guitar sound. The strict SFZ subset, 256 MiB decoded budget,
128-voice limit and linear sample-rate/pitch interpolation remain; streaming and a
band-limited resampler are not implemented. Real-recording realism and articulation
coverage are not established by synthetic fixtures.

Song analysis is heuristic, with eight chord-template qualities and major/minor
key profiles, not a new trained AI model. Difficult mixes, modulation, unstable
meters and tempo-map alignment require review. Detected results are separate from
manual controls; the dedicated detected-MIDI exporter and song-follow guitar retain
timestamps/gaps, whereas applying a progression produces an editable chord chart.
Audio is not automatically time-stretched. Re-render after tempo/harmony edits.

Sample loading, analysis, guitar rendering and separation are background tasks.
Import, export and large state serialization still perform synchronous caller work.
Host sandbox/security bookmarks and native file dragging require Mac verification.
No measured M1 CPU, RSS, battery or thermal performance is claimed.

Exact Mac build, AU install/validation, standalone install/launch commands and the
physical host checklist are in [the M1 runbook](PLAYBACK-SONG-GUITAR-M1.md).

## Final results

- Linux VST3 and standalone: built successfully (Debug, C++20, JUCE 8.0.6).
- Full CTest: **16/16 passed**, 19.90 seconds, including **1,439 integration checks**.
- Sampled guitar: **536 checks**, including a 512-frame live/offline comparison.
- Harmony fixtures: **96/96** held-chord combinations and **24/24** cadence keys;
  108 assertions. These are synthetic figures, not commercial-recording accuracy.
- Tempo: **27 checks**. Song guitar: **41 note/timeline checks**, plus odd-bar and
  cancellation cases.
- Plugin-off portable build: **14/14 passed**, 14.42 seconds. The final updated
  song-follow test was rebuilt and passed again in that configuration.
- Shell scripts and all Mac runbook bash blocks: syntax checks passed. Python
  installer metadata/platform tests: 2 tests passed. No Mac install was simulated.
- `git diff --check`: passed.

Machine-readable evidence: [session-engine-tests.json](Validation/session-engine-tests.json).
Actual Linux JUCE raster: [Guitar Studio](Screenshots/jaswingman-song-workflow-linux.png).
The screenshot uses Basic Synthesis and test data; it does not demonstrate recorded
sample realism, a physical audio device, or a native Logic Pro session.

## Exact modified files

- `CMakeLists.txt`
- `Documentation/PLAYBACK-SONG-GUITAR-M1.md`
- `Documentation/SESSION-ENGINE-VALIDATION.md`
- `Documentation/Samples/README.md`
- `Documentation/Samples/VALIDATION.md`
- `Documentation/Screenshots/jaswingman-song-workflow-linux.png`
- `Documentation/Validation/session-engine-tests.json`
- `README.md`
- `Scripts/build-macos.sh`
- `Scripts/install-standalone-macos.py`
- `Scripts/launch-standalone-macos.sh`
- `Source/Audio/PlaybackEngine.h`
- `Source/Audio/SampleVoices.h`
- `Source/GUI/GuitarStudioPanel.h`
- `Source/GUI/TempoPanel.h`
- `Source/Generation/Patterns.h`
- `Source/Host/Telemetry.h`
- `Source/MIDI/PatternSequence.h`
- `Source/MIDI/RealtimeSchedule.h`
- `Source/Music/GuitarRhythmGenerator.cpp`
- `Source/Music/GuitarState.h`
- `Source/Music/SampledGuitar.h`
- `Source/Music/SongGuitar.h`
- `Source/PluginEditor.cpp`
- `Source/PluginEditor.h`
- `Source/PluginProcessor.cpp`
- `Source/PluginProcessor.h`
- `Tests/IntegrationTests.cpp`
- `Tests/SampledGuitarTests.cpp`
- `Tests/SongGuitarTests.cpp`
- `Tests/TempoTests.cpp`
- `Tests/test_standalone_install.py`
