# Wingman Pro development audit

Baseline: e9c0283, Debian 13 / GCC 14.2 / JUCE 8.0.6. On 2026-10-24, CTest passed 2/2 suites: 16 DSP and 59 integration checks, without optional screenshot fixtures.

Existing features: four WAV lanes, input recording, linear resampling, waveform peak envelopes, mute/solo/gain, basic onset BPM analysis, major/minor triads, sample-offset MIDI output, standard MIDI and 24-bit WAV export, version-1 binary ValueTree state embedding audio. The standalone/VST3 baseline builds; no macOS machine or physical audio device is attached.

Audit findings:

- MIDI input causes JUCE to choose Music Effect (`aumf`) by default. The old README/build script guessed `aufx`; `auval -v aufx Wgm1 Wgma` can therefore report a missing component even for an installed working bundle. This is a likely cause, not a diagnosis of the user's uninspected macOS binary.
- Preserve bundle ID `com.wingmanaudio.wingman`, manufacturer `Wgma`, subtype `Wgm1`, and state root `Wingman`. Changing identity would break DAW discovery of existing sessions.
- Audio callback uses a try-lock, but JUCE's Synthesiser internally takes another lock. UI/state operations can cause silent blocks. MIDI buffers can grow. No bypass override or live clipping guard exists.
- Recording has a 120-second cap. WAV import, export, state copies and waveform construction are synchronous. No model, harmonic recognizer, bass/drum/melody engines or host playhead synchronization exists.
- GUI is fixed-size and controls are not host automation parameters. No model manager, per-track pan or user-editable MIDI exists.

Execution order and acceptance:

1. Stability/AU repair: make existing category explicit, read compiled plist identifiers, safe user-level installation and signing/discovery checks; regression and metadata fixture tests on Linux. Real `auval`/Logic acceptance remains pending macOS.
2. GUI: responsive component layout and working controls for implemented capabilities, processing status and an honest model status. Screenshots from actual rendering.
3. Music analysis: deterministic spectral/chroma analysis with tuning and confidence, eight chord qualities, temporal segmentation; measure synthetic reference accuracy and document recorded-audio gaps.
4. Generation: seeded harmony-aware bass, GM drums and constrained melody with MIDI preview/export and parameter/state tests.
5. Separation: evaluate a pinned worker/model, verify license/runtime/checksum, then install only with explicit consent; run actual inference and measure synchronized output, wall time and peak RSS. Never label an adapter as a tested separator.
6. DAW integration: bounded host timeline scheduling, missing-position and seek/loop tests through a mock playhead; real Logic/MainStage testing on macOS remains mandatory.
7. Release: regression builds, measured benchmarks and manuals. M1 CPU/CoreML performance cannot be inferred from Linux timings.

Risks/dependencies: JUCE license selection before distribution; PyTorch/ONNX/CoreML compatibility must be measured, not assumed; model weights and runtime are absent; no licensed recorded reference corpus has been supplied; M1 and Logic are inaccessible. Synthetic harmonic tests do not establish commercial-song accuracy. Preserve v1 state reads as new properties are introduced. Do not call a phase complete while its platform/model acceptance remains pending.

Progress: AU preparation committed as `9cfc5c8`; 3/3 Linux suites pass including six metadata fixtures. Actual macOS registration/validation remains pending. The next GUI increment adds persisted resizable vector layout, lane order Vocals/Drums/Bass/Other, tested stereo balance in playback/export/state, and an honest model-status dialog. It is a partial Phase 2 increment: generation panels and harmonic editing are implemented alongside their backends in later increments, not declared complete here.

Current evidence and per-increment file/commit/test status are recorded in [PHASE-REPORT.md](PHASE-REPORT.md). The baseline audit above is historical; later commits address many of its findings without claiming that macOS/model acceptance is complete.
