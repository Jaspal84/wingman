# Milestone A: editable eight-track arrangement

This branch extends the separate Jas PRO standalone. JASWINGMAN PRO source,
plugin identifiers, session format and five-track engine are unchanged. This is
an initial editing milestone, not a finished production workstation.

## Working controls

- Add Audio inserts at the playhead; dropping WAV/AIFF/FLAC onto the timeline
  inserts at that track and beat. Multiple clips can share a track and overlap;
  overlaps sum. The arrangement supports 512 regions across eight tracks.
- Select a clip, drag its body horizontally or to another track. Drag the first
  or last seven pixels to trim. Trim can restore previously hidden source audio.
- Click the ruler to seek, or click empty track space to seek and select a paste
  destination. Space toggles play/pause; Stop returns to zero.
- Snap supports 1/16 notes, beats and bars in 4/4. Disable Snap for free movement.
  The ruler labels bars starting at one; inspector and loop positions use beats
  starting at zero. Zoom and the horizontal scrollbar navigate the arrangement.
- S splits the selected region at the playhead. Duplicate appends a copy to its
  right; Copy/Paste places a new region at the playhead on the selected track.
  Delete removes the selected region. Cmd/Ctrl+D/C/V and Delete/Backspace work.
- Undo/Redo (Cmd/Ctrl+Z, Shift+Z or Y) restore edits, import, mixer, tempo and loop
  settings. A timeline drag is one transaction. History retains 64 commands;
  immutable audio caches are shared by regions/history rather than copied.
- The bottom inspector edits start beat, source offset, duration, gain and linear
  fades. Numeric boxes accept exact values. Invalid source bounds are rejected.
- BPM changes beat-anchored clip positions. Source audio retains its speed and
  duration: this milestone does not implement time stretching or pitch shifting.
- Loop uses explicit start/end beats. The loop end must follow its start.
- Track volume, pan, mute and solo affect playback and export. Export writes the
  complete arrangement, including leading gaps, to stereo 48 kHz PCM24 WAV. Loop
  mode is ignored for export. Real-time and offline mixing use the same renderer.

## Assets, persistence and cache

Save writes version-two `.jaspro` XML atomically with clip IDs, original source
paths, beat anchors, trims, gain, fades, tempo, loop range and mixer state. Open
also migrates version-one single-clip sessions in memory. Old app versions cannot
read v2. Keep originals in place; projects are references, not portable archives.
Missing files, malformed settings, duplicate IDs, excessive clips and cancellation
leave the current arrangement unchanged. Opening a project clears editing history.
Saving does not persist the clipboard, undo history, selection or device settings.
Save before opening another project or closing the application.

Import decodes in 2048-frame blocks to an immutable interleaved float PCM file in
the OS temporary directory. A read-only memory mapping and sparse peaks serve
playback and waveform drawing. There is no 30-second cap in this new workflow;
mono/stereo sources at 8-192 kHz are accepted, with a 16 GiB decoded size ceiling
per source and a disk-space check. The old plugin/import API remains unchanged.
Project loading shares a cache for repeated paths. Duplication, trim and split
never modify source audio. Caches are deleted when their final owner is released.
An abnormal process termination may leave temporary caches for OS cleanup.

Mapping uses reclaimable OS file pages, not a hard real-time streaming/prefetch
scheduler. Cold-page faults can stall playback. Long-file import is tested;
latency, cache pressure, disk exhaustion races and dropout behavior on physical
hardware remain unvalidated. Peak display is an amplitude envelope, not a sample
editor. Sources changed outside the app can produce different audio on reopening.

## Exact MacBook Air M1 build and validation

Install Xcode Command Line Tools (`xcode-select --install`), Git and CMake 3.22+
(e.g. `brew install cmake`). Use a native arm64 terminal; `uname -m` should say
`arm64`. Existing application bundles are not overwritten by these commands.

```bash
cd "$HOME/Developer/wingman"
git fetch origin
git switch development/omniloop-eight
git pull --ff-only origin development/omniloop-eight
cmake -S . -B build-timeline-m1 -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DWINGMAN_BUILD_STUDIO=ON
cmake --build build-timeline-m1 --parallel 2
ctest --test-dir build-timeline-m1 --output-on-failure
open "build-timeline-m1/JasProStudio_artefacts/Debug/Jas PRO Ultimate Loop Studio.app"
```

CMake fetches the repository-pinned JUCE 8 commit. For a Release audition, use a
separate build directory and `-DCMAKE_BUILD_TYPE=Release`. With a multi-config
Xcode generator, add `--config Debug` to build and `-C Debug` to CTest.

On the M1, select the intended CoreAudio output and audition licensed files longer
than 30 seconds. Move clips between tracks, split at a beat, trim and restore the
edges, set gain/fades, duplicate/copy/paste, and undo/redo. Change BPM and confirm
beat anchors move while audio speed stays fixed. Set and cross a loop boundary.
Save/reopen and compare all edits. Export and listen in another player, checking
silence before late clips and exact clip endings. Test 44.1/48 kHz devices, device
switching, Retina scaling, resizing, shutdown during import/export and cold-cache
long-file playback under memory pressure. Compare audio output with the WAV.

Plugin compatibility on the Mac additionally requires native AU build/validation:

```bash
arch -arm64 /usr/bin/auval -v aumf Wgm1 Wgma
```

Run auval against the installed build following `AU-VALIDATION.md`; then open
existing JASWINGMAN PRO sessions in Logic. Linux results cannot establish CoreAudio,
Audio Unit, Logic, Retina or physical M1 playback correctness.

## Deliberate remaining limitations

No sample index/browser, vertical waveform zoom, adjustable dock panels,
metronome, marquee/multi-selection, automation, autosave/unsaved-close prompt,
recording, clip looping/repetition handles, time signature changes, stretch/pitch,
effects, instruments or Magic Arrange are provided by this milestone. Use duplicate
for repetition and project save for persistence. Gain changes are unsmoothed and
the master clamps overloads; avoid overdriving the sum. Linear sample-rate
conversion is retained. Audio-device latency/performance needs hardware audition.

## Executed Linux validation

On Debian 13 x86_64, GCC 14.2, CMake 4.4.4, pinned JUCE 8.0.6, Debug:

- Built JasProStudio, JASWINGMAN PRO Standalone and VST3, and all test targets.
- All 20 registered CTest suites passed (19.56 seconds); reusable environment
  setup was rerun and again passed all 20 (18.94 seconds).
- The new arrangement suite passed 79 assertions. These include a 35-second
  import, source interpolation, movement/trim/split/history, scheduling, fades,
  mute/solo/pan, overlap mixing, loop wrap, BPM anchors, atomic project loading,
  v1 migration, invalid/missing assets, source protection, export cancellation,
  and sample comparison between live and offline renderers.
- GUI self-test exercises drag between tracks, undo/redo, split, duplicate,
  copy/paste, clip gain/fade, edge trim, delete/undo and BPM. It also renders
  layouts at 1100x740, 1400x850 and 2400x1500 with JUCE assertions treated as
  failures. A screenshot uses a synthetic audio fixture, not a commercial sample.

No physical audio device is available in this container. The headless run reports
an unavailable ALSA sequencer; the synthetic rendering and GUI tests pass. The
optional external model-inference suite is not enabled by this configuration and
was not run; no weights are downloaded. No macOS executable, AU or M1 hardware
validation was performed. Existing plugin compiler warnings remain.
