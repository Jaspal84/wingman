# Wingman Pro implementation report — 2026-10-24

This is an incremental development delivery. **The master acceptance criteria are not all complete.** Linux builds/tests do not validate AU, Logic or real trained-model separation.

| Increment | Commit | Main modified files | Evidence and outstanding acceptance |
| --- | --- | --- | --- |
| Audit / AU preparation | `9cfc5c8` | CMakeLists.txt; Scripts/build-macos.sh, install-macos.sh, validate-au.sh, macos_au.py; Tests/test_au_metadata.py; Documentation/IMPLEMENTATION.md and AU-VALIDATION.md | Baseline 2 suites passed; then 3 suites passed including six metadata fixtures. Explicit `aumf` fixes the old command/category mismatch. Actual compiled AU registration, auval and Logic are pending macOS. |
| GUI / mixing increment | `f217061` | Source/PluginEditor.*, PluginProcessor.*, Audio/Track.h; Tests/IntegrationTests.cpp | Resize, stereo balance and optional state properties tested. Current GUI includes project name, processing indicator, generator/model panels and MIDI editing from later commits. Full commercial accessibility/UX and Mac Retina validation remain open. |
| Harmonic analysis | `89cf6fc` | Source/AI/ChordDetection/HarmonicAnalysis.h; DSP/Music.h; MIDI/ChordSequence.h; processor/editor; Tests/HarmonyTests.cpp | Background/cancellation and WAV integration pass. Synthetic held chords 96/96; keys 24/24. No annotated recorded-song or genre-accuracy benchmark. Confidence is uncalibrated; silence-gap transcription is incomplete. |
| MIDI generation | `e34e3a4` | Source/Generation/Patterns.h, State.h; MIDI/PatternSequence.h; GUI/GeneratorPanel.h; processor/editor; Tests/GenerationTests.cpp | 96 style/length/kind combinations checked for repeatability, timing, bounds and constraints; MIDI/state round trips and phrase preservation pass. Later UI adds bass/melody note editing. 4/4 only; no candidate browser or full drag-resize piano roll. |
| Host / callback hardening | `178bd90` | Source/Host/Transport.h; Audio/PreviewSynth.h; processor/editor; Tests/TransportTests.cpp and IntegrationTests.cpp | Mock host start/stop, seek/chase, missing position, loop offset, bypass and control concurrency pass. Fixed preview voice pool removes Synthesiser mutex. Session try-lock dropouts and host MIDI-buffer growth still need release work. Real DAW tests pending. |
| Model worker / manager | `87cdf17` | Scripts/stem_worker.py, setup-inference.sh and worker locks; Source/AI/ModelManager/*; GUI/ModelPanel.h; processor/editor; Tests/test_stem_worker.py | Pinned torch/torchaudio/Demucs runtime imports. Seven worker failure/consent/checksum/cancel/status fixtures plus real missing-model subprocess test pass. Atomic four-file WAV import validation passes. **No actual trained inference:** weight-host request returned HTTP 403 and running configuration still lacks the saved allowlist addition. No inference RAM/time or quality claim. |
| Apple toolchain portability | `1208ec4` | Source/Threading/BackgroundThread.h; processor and ModelJob; thread tests; CMake | Avoids std::jthread/stop_token SDK availability gaps using std::thread/atomic cancellation. Cooperative shutdown test and all seven suites pass on Linux. Still not a macOS compilation result. |

## Final current-instance validation

Debian 13 x86_64, GCC 14.2, JUCE 8.0.6, CMake 4.4.4, Ninja. Built and linked Linux Debug VST3 and Standalone. JUCE's VST3 helper loaded the module to write its manifest; this is not a full VST3 host validator. The real editor and both settings panels render in integration tests. Main GUI screenshot uses synthetic WAV fixtures, never fake application lanes.

**7/7 CTest suites passed:**

- AU metadata: 6 Python fixtures (not auval).
- Model worker: 7 Python failure/consent/status/cancel fixtures (not trained inference).
- Portable DSP: 15 checks, including chord parsing and pulse BPM.
- Harmony: 108 checks; held-chord labels 96/96 and cadence keys 24/24.
- Generation: 32,708 per-note/constraint/repeatability checks across 96 scenarios.
- Transport/threading: 13 checks.
- Integration: 222 checks with optional screenshot fixtures, including actual WAV buffers/exports, generated MIDI/state, stem-set validation, subprocess failures, GUI button invocation/rendering and mock playhead behavior. Nine checks belong to visual fixture writing/import/screenshot output.

No failed or skipped suite is represented as passed. Shell scripts pass bash syntax checks; Python tooling compiles and its registered fixture suites run. Native AU, physical playback/capture, actual DAW drag/drop and CoreML/M1 tests are unrun. The current standalone also remained alive for a five-second Xvfb startup smoke test and was stopped by the timeout (expected exit 124). It emitted missing-ALSA-device and headless X11 window-manager warnings, without JUCE assertion or crash messages; this is not audible playback validation. A separate JUCE-free configuration passes 6/6 suites.

The measured Linux harmonic benchmark is retained separately with raw hardware/command/RSS/wall-time data. It is not an inference or M1 benchmark. See PERFORMANCE.md.

## Required external actions and remaining work

1. Apply the saved network addition for `dl.fbaipublicfiles.com` through environment settings, then publish the environment. Model download/runtime consent was explicitly supplied in this chat. After access changes, retry the official installer, retain its full hash and run actual inference on an appropriately licensed stereo recording. Measure synchronized stem lengths, finite samples, signal statistics, time and RSS. Do not call the separation feature verified until that succeeds.
2. On the M1 Mac, build, install and run the compiled-metadata validator. Retain actual auval output and confirm Logic Plug-in Manager/MainStage discovery. Then test host routing, tempo/loop behavior, restoration, microphone permissions and physical devices.
3. Complete remaining platform-independent product work listed above: recorded/reference analysis evaluation, silence/rest handling and beat alignment, broader meter generation, candidate browsing and deeper MIDI editing, lock-free session snapshots, callback allocation instrumentation, asynchronous import/export/waveform/state work and full accessibility/UX polish.
4. Only then profile native 8-GB M1 memory pressure, investigate proven-compatible CoreML/ONNX conversions, select licensing/signing/notarization and produce a validated release package.

The work is published as reviewable source commits; this report does not mark all seven requested development phases complete or claim a production-ready plugin.
