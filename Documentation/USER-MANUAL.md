Current renamed build: see [JASWINGMAN PRO user guide](JASWINGMAN-USER-GUIDE.md) and [Mac M1 runbook](JASWINGMAN-MAC-M1.md). Older artifact names below describe the earlier build.

# Wingman Pro development manual

This is a tested Linux audio/MIDI development build, not a validated macOS production release.

## Audio workspace

Four lanes display Vocals, Drums, Bass and Other, while preserving historical state indices (Vocals/Other/Bass/Drums internally). Import a WAV into any lane; Play mixes the imported clips from time zero. S is multi-solo, M is mute, the rotary control is gain and the horizontal slider is stereo balance. Individual WAV export applies gain/balance and ignores mute/solo; mix export respects them. WAV output is 48-kHz stereo 24-bit PCM. Both playback and mix export saturate overloaded samples; lower gain to avoid distortion.

Record captures the plugin's input into the lane selected in the transport dropdown, independent of playback. Click Record again to finish or use Stop while standalone transport is selected. Limit: 120 seconds. Enable the standalone wrapper input and macOS microphone permission. No live input is passed through by the normal mixer; bypass passes dry input. A real device is needed to verify physical capture.

The window resizes using vector rendering and persists its dimensions. Waveform peak envelopes are calculated from actual imported audio; the application ships no fake waveform data. Main project state embeds audio, which can make DAW projects large.

## Analysis and harmony

Detect chords/key analyses the selected lane in the background and reports progress, estimated tuning, key and confidence margin. Cancel stops analysis. This replaces the editable progression; keep important manual edits before reanalysing. The spectral method is experimental; consult HARMONIC-ANALYSIS.md for synthetic metrics and limitations, especially silence gaps, inversions and dense mixes.

Click a chord box for root/quality/inversion/duration editing, or enter tokens in the text field and Apply. Examples: `C Am F G`, `Cmaj7^1:2 Dm7:2 G7:4`. The two small sliders zoom/scroll the chord timeline. Key transposes the written harmony; it is not an audio pitch shifter. Editing harmony invalidates previously generated parts so they cannot silently continue against different chords.

## MIDI composition

The right-hand selector chooses chords, bass, drums or melody. Generate selected part opens the appropriate shared settings panel for bass/drums/melody. Choose a compatible scale, seed, style and 4/8/16/32 bars, then Generate/regenerate. Parameters specific to another instrument do not affect the selected part. These are deterministic, constrained composition engines, not trained AI generation.

Audition enables the internal simple synth; disable it when using MIDI output to external instruments. MIDI export writes the selected part with tempo metadata; WAV export excludes synthesized MIDI. The Drum MIDI preview is a clickable 9-row / 16-step sequencer. Use its bar slider for other bars. Parameter-panel phrase variation changes only its selected four-bar interval. Global variation regenerates the whole pattern. Click bass/melody preview notes for pitch/timing/velocity edits or deletion. Full drag-resize editing, candidate browsing and professionally sampled instrument previews remain future work.

Drag creates a temporary MIDI file and requests JUCE external file drag/drop. This requires support from the target DAW and has not been tested in Logic. Export MIDI is the verified file-transfer fallback. See GENERATION.md for algorithm and GM mappings.

## Model Manager

1. Install the pinned runtime separately with `Scripts/setup-inference.sh /absolute/runtime/path` using Python 3.12. Linux runtime imports were tested; macOS pins remain untested.
2. In Model Manager choose the runtime's `bin/python` executable and a model directory.
3. To download weights, explicitly select the MIT/download consent checkbox, then Install official model. Alternatively select an existing offline installation and Verify local model.
4. Select Separate WAV, choose a mono/stereo mix up to 120 seconds, and monitor progress. Cancel terminates the owned subprocess. Successful output must pass format, finite-sample and synchronization checks before all four lanes are replaced together.

No weights are bundled. Model checksum failures, missing runtime/packages and network denials are errors; the plugin does not substitute EQ or fake stems. The worker contains actual HTDemucs calls, but the official weight host is now accessible and **actual CPU inference, plugin import/export and active-inference cancellation pass in Linux**. Read INFERENCE-VALIDATION.md for measured evidence and limited source-quality coverage; this does not establish AU/Logic/M1 readiness. See Models/README.md for licensing, verification, offline use and remaining RAM/latency acceptance.

## DAW transport and macOS

Enable DAW to follow host BPM, meter, running state, PPQ and loops. Local Play/Stop/BPM are disabled; turn DAW off for standalone control. Missing host PPQ safely stops playback. Generated parts currently assume 4/4 even when another host meter is displayed. Tempo-map audio alignment, complex host routing and real Logic behavior need macOS tests.

Build/install/validate using Scripts/build-macos.sh, install-macos.sh and validate-au.sh. The validator reads the compiled bundle, checks signing/architecture and calls auval with actual identifiers. The historical `aufx` command was inconsistent with the `aumf` Music Effect category. No actual AU validation or Logic/MainStage run has occurred here. Do not erase unrelated plugin caches to work around discovery.
