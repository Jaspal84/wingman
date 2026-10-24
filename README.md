# Wingman Pro

A C++20 / JUCE 8 audio workspace for four imported or recorded tracks and editable chord MIDI. Phase 1 implements local audio and MIDI processing; it does **not** implement an AI model or stem separation. Track names describe user-supplied audio, not automatically separated sources.

## Features

- Original charcoal interface following the supplied transport / four-lane / export-sidebar / ideas layout, with purple, cyan, coral and teal tracks.
- WAV import (mono or stereo, 8–192 kHz, up to 120 seconds per track), waveform envelopes, sample-rate conversion by linear interpolation, playback, gain, mute and multi-solo.
- Record the plugin's audio input into a selected lane. Click Record again or Stop to finish. Standalone input must be enabled in the audio device settings; macOS requires microphone permission. Recording is limited to 120 seconds.
- Basic onset-energy autocorrelation tempo estimation after import. Requires at least three seconds of rhythmic audio; silence and weak periodicity are rejected. Half/double tempo ambiguity remains. BPM can be edited manually.
- Edit 1–16 major/minor chords (`C Am F G`, `F#m`, `Bb`), four beats per chord. Ideas selects one of three deterministic musical progressions.
- Key control transposes the written chords in semitones from C; it does not detect the input key or time-stretch audio.
- Generate three-note chord MIDI, audition with an internal sine synthesizer, output sample-timed MIDI events to compatible hosts, and export a standard MIDI file (960 PPQN, tempo, 4/4 and note releases).
- Export the audible audio mix or an individual source lane to stereo 48 kHz / 24-bit WAV. Individual lane export applies its gain and ignores mute/solo. Mix export respects mute/solo. WAV export contains audio tracks only; export chord MIDI separately.
- Plugin state embeds audio samples, gain/mute/solo, tempo, key, progression and MIDI settings, so restored sessions do not depend on source paths. Transport and active recording do not resume on restore.

## Build

CMake 3.22+, a C++20 compiler and Git are required. JUCE **8.0.6** is fetched at a pinned commit; pass `-DJUCE_SOURCE_DIR=/path/to/JUCE` for an existing matching checkout. JUCE licensing terms apply; choose an appropriate JUCE licence before distributing binaries.

### macOS Apple Silicon

Install Xcode Command Line Tools and CMake, then run:

```sh
./Scripts/build-macos.sh
```

Outputs are under `build-macos/Wingman_artefacts/Release/`: AU, VST3 and Standalone. The script defaults to `arm64` and macOS 11+. Set `WINGMAN_ARCHS='arm64;x86_64'` for a universal build when the toolchain supports it. Additional CMake options can be passed to the script. Plugins are not automatically installed into system folders.

Install a development copy and validate using the actual compiled identifiers:

```sh
./Scripts/install-macos.sh "build-macos/Wingman_artefacts/Release/AU/Wingman Pro.component" --adhoc
./Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/Wingman Pro.component"
```

**macOS-only checks still required:** AU build and validation, microphone permissions, CoreAudio hardware capture/playback, Logic Pro and MainStage loading, host state restoration and MIDI routing. These have not been tested in the Linux development environment. Signing/notarization and distribution packaging are not configured.

### Linux

On Debian/Ubuntu, install the development prerequisites:

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev \
  libxcursor-dev libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Linux builds VST3 and Standalone. For headless editor rendering tests use `xvfb-run -a ctest --test-dir build --output-on-failure` (requires `xvfb` and `xauth`). The standalone is `build/Wingman_artefacts/Debug/Standalone/Wingman Pro`. A real audio device is needed for audible playback and recording; the tests exercise actual processor buffers and file I/O without hardware.

A JUCE-free portable DSP-only build is also available:

```sh
cmake -S . -B build-dsp -DWINGMAN_BUILD_PLUGIN=OFF
cmake --build build-dsp
ctest --test-dir build-dsp --output-on-failure
```

## Use

1. Import a WAV into each desired lane. No demo waveforms or automatic separation are used.
2. Set BPM and transpose; Play starts the audio timeline and Stop rewinds. Imported lanes play once from time zero; generated chord MIDI repeats over its progression.
3. Adjust lane volume, Solo and Mute. To record, select the target lane in the transport dropdown and click Record. Finishing replaces that lane's clip.
4. Enter chord names and Apply, or select an Ideas progression. Generate MIDI enables chord playback; Audition controls the internal synth. Compatible hosts can receive the same outgoing MIDI with audition disabled.
5. Export a lane, the audio mix or the chord MIDI through the right-side controls. Plugin hosts save and restore the full session with their usual project/state controls.

Standalone JUCE wrappers normally mute their inputs to avoid feedback. Use the wrapper's audio settings/input-mute control when recording hardware input. The mixer does not pass live input directly to the outputs.

## Implementation and limits

`Source/DSP` contains portable harmony and tempo logic, `Audio` immutable clips and interpolation, `MIDI` beat-domain chord sequencing, and `GUI` the theme. Processor access is synchronized with a nonblocking audio-thread try-lock: a concurrent state edit may silence a block instead of waiting. File decoding, waveform calculation, export and state serialization run outside the audio callback, but currently execute synchronously on the calling/UI thread. Large files/state may briefly stall the editor; worker jobs and lock-free session swaps are future work. A two-minute stereo recording buffer is reserved when the device starts.

State embeds raw float audio in JUCE's binary ValueTree format and can be large. Audio samples are serialized in native float byte order; cross-endian state transfer is unsupported. Playback is a prototype mixer with linear resampling and no limiter; avoid overloading the live output. PCM WAV export clips to [-1, 1].

Not implemented: AI inference/stem separation, key/chord detection from audio, melody-to-MIDI, bass/drum/melody generation, host transport sync, timestretching, ZIP stem bundles, MIDI drag/drop, automation parameters or preset-file management. Controls for these unimplemented features are deliberately absent. The music ideas are deterministic, not AI-generated.

Tests cover chord parsing/transposition, pulse BPM detection and rejection, WAV decoding/playback, mute/solo/gain, MIDI emission/export, WAV mix samples, embedded state round trips, input recording and editor rasterization. See `Tests/` and CTest results for executed checks.

## Phase 1 validation (Linux)

Validated on Debian 13 / GCC 14.2 with JUCE 8.0.6, CMake and Ninja:

- Debug VST3 and Standalone compiled and linked; JUCE's VST3 helper loaded the module to generate its manifest.
- CTest: **2/2 suites passed — 16 DSP checks and 59 integration checks**. Optional screenshot fixtures add eight additional file-writing/import checks and one screenshot-write check.
- Editor rendered to an image under Xvfb. Standalone window startup was smoke-tested under Xvfb; headless X11 emitted window-manager warnings.
- No audio/MIDI hardware is available in this cloud machine. Tests rendered and verified processor buffers and file contents; physical device playback/capture and external DAW MIDI routing remain untested.
- macOS AU/VST3/Standalone builds, `auval`, Logic Pro and MainStage integration remain unrun.

`Scripts/build-linux.sh` builds and runs the Linux suites. Set `WINGMAN_TRACE_TESTS=1` for per-check diagnostics, or `WINGMAN_SCREENSHOT=/absolute/path/ui.png` to render an editor screenshot using generated WAV test fixtures (never application demo data).

The original `aufx` validation example did not match JUCE's MIDI-capable Music Effect default (`aumf`). This category is now explicit. See [AU validation](Documentation/AU-VALIDATION.md) and the [audit and implementation plan](Documentation/IMPLEMENTATION.md). Installation/metadata tooling is tested on Linux fixtures; actual macOS validation is still pending.
