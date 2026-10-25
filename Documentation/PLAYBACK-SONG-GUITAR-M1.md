# Playback, song analysis and guitar workflow — Mac M1 runbook

This update preserves AU type `aumf`, subtype `Wgm1`, manufacturer `Wgma` and bundle
ID `com.wingmanaudio.wingman`. Native validation of this new build must be performed
on a Mac. The user reports an earlier 1.0.0 AU passed; that is not validation of
these changes. Linux cannot execute AU, Logic Pro, MainStage or CoreAudio tests.

## Root causes and implementation

1. The old processor cleared audio and MIDI when its session try-lock failed. A
   new five-lane playback test reproduced a silent block; automatic analysis also
   exposed a lost one-block recording. Immutable published mixer/MIDI/sample
   snapshots now feed the audio thread. Snapshot/clip/sample reclamation happens
   on non-audio writers using a single-reader hazard pointer. The callback no longer
   takes the session mutex. Input recording uses its preallocated buffer before
   transport/mixing and is independently synchronized with Finish Recording.
2. Import left the transport at its previous position; Play could resume past the
   audio endpoint. Import resets independent position; Play at the end restarts.
   Audio-only transport stops at the end, and Loop wraps to the longest lane.
   Clicking an input waveform seeks. MIDI generation continues its sequence when
   enabled; the Loop switch specifically loops imported audio and aligned MIDI.
3. Host sync required PPQ and overwrote the independent BPM. Seconds-only and
   sample-position timing now work; absent timeline fields use the running position
   while following play/stop. Missing host timing stops synchronized playback safely.
   Seeks cannot be detected if the host supplies no position field. Host BPM/meter
   telemetry is published independently of session edits. Internal BPM is retained
   when sync is disabled. Standalone refuses DAW sync and uses internal transport.
4. Harmonic analysis averaged stereo channels, cancelling anti-phase recordings,
   and silently replaced editable chords. Analysis now selects the more energetic
   channel and queues the latest import. Detected key/chords remain separate until
   Apply. Detecting and exporting chord regions uses actual timestamps; uncertain
   results need manual correction, not invented extensions.
5. Guitar generation previously used only the editable repeating progression.
   Follow Imported Song uses detected region start/end times, preserves gaps, clips
   notes at chord boundaries and uses playable string/fret voicings. The detected
   strip in Guitar Studio can be clicked to correct a chord without moving it.
6. SFZ preview previously required offline rendering. A fixed 128-voice preloaded
   sample player now handles live MIDI sample offsets, velocities, note releases,
   string-channel zones, round-robin and available recorded articulations. Generated
   guitar MIDI carries per-string CC20 articulation cues (24–31); SFZ playback uses
   directional recordings when mapped and falls back to the selected technique's
   sample mapping. Existing MIDI pitches, durations, velocities and exports remain.
7. Building the AU never installed the standalone. The Mac build now checks that
   the `.app` executable exists and prints its architecture. Separate install and
   launch tools copy with ditto, verify arm64 and apply/verify local ad-hoc signing.
   Installation defaults to `~/Applications`; `/Applications` is optional.

## Build on the MacBook Air M1

Close Logic, MainStage and the old standalone before installing replacements.
Paste commands only, without a Terminal prompt or the word "Run".

```bash
mkdir -p "$HOME/Developer"
if [ -d "$HOME/Developer/wingman/.git" ]; then
  cd "$HOME/Developer/wingman"
  git pull --ff-only origin main
else
  git clone https://github.com/Jaspal84/wingman.git "$HOME/Developer/wingman"
  cd "$HOME/Developer/wingman"
fi
./Scripts/build-macos.sh
```

Requires Xcode Command Line Tools, CMake and working JUCE build dependencies, as in
previous builds. The script builds arm64 Release AU, VST3 and standalone and runs
CTest. Optional real model tests require the existing pinned runtime/model settings;
this change never installs or downloads models or guitar samples.

## Install and launch standalone separately

```bash
cd "$HOME/Developer/wingman"
python3 Scripts/install-standalone-macos.py \
  "build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app"
./Scripts/launch-standalone-macos.sh
```

Default installed application: `~/Applications/JASWINGMAN PRO.app`.
The installer stages, signs and verifies before replacing the old app, with rollback
on publication failure. Launch requests do not prove that physical audio playback
succeeded. For optional system installation into `/Applications`, use `--system`
with appropriate write permissions, then pass that installed path to the launcher.

## Install and independently validate the new AU

If older Wingman bundles with the same identifier remain, move those copies outside
Components first. Preserve backups; do not delete Audio Unit caches blindly.

```bash
cd "$HOME/Developer/wingman"
wingman_backup="$(mktemp -d "$HOME/Documents/JASWINGMAN-AU-Backup.XXXXXX")"
for wingman_name in "Wingman.component" "Wingman Pro.component"; do
  wingman_old="$HOME/Library/Audio/Plug-Ins/Components/$wingman_name"
  if [ -e "$wingman_old" ]; then mv "$wingman_old" "$wingman_backup/"; fi
done
./Scripts/install-macos.sh \
  "build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component" --adhoc
python3 Scripts/macos_au.py inspect \
  "$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component"
arch -arm64 /usr/bin/auval -v aumf Wgm1 Wgma
```

The entire AU validation must pass, including cold opening and rendering. A metadata
PASS alone is insufficient. If opening fails, collect the whole log, installed
version, architecture, code-signature result, duplicate registrations (`auval -a`)
and macOS crash report. Do not assume a stale cache is the cause.

## Functional checks in standalone, Logic Pro and MainStage

1. Standalone: choose a physical output device under Options/Audio Settings. Import
   a known audible WAV into each lane. With DAW off, test Play, Stop, waveform seek,
   natural end, Loop, all five lanes together, solo, mute, volume and pan at 44.1/48
   kHz. Confirm the cursor and audible output agree. The JUCE input-muted feedback
   banner concerns input recording; it is not a proof that output is muted. Use
   headphones and intentionally enable/configure input before physical recording.
2. Logic/MainStage: load the new AU and route its output to an audible bus. Test
   independent transport first, then DAW sync with start/stop, timeline seeks, loop
   boundaries, tempo changes and 3/4 or 6/8. Disable sync and confirm independent BPM
   returns. AU validation does not prove these routing or host behaviors work.
3. Import a licensed song. Tempo analysis starts in the background; automatic analysis follows the imported lane. Four-stem imports analyze Drums
   for tempo and Other for harmony; Drums can also be selected explicitly in the
   analysis panel. Harmonic analysis follows the imported source. Review periodicity confidence and half/double candidates;
   Apply Detected Tempo only while DAW sync is off. It does not edit Logic's tempo.
4. In Guitar Studio select the correct source and Analyze Song if needed. Review
   key/chord detection, use the explicit Apply controls as desired, enable Follow
   Imported Song, and Generate from Song. Click the detected chord strip to correct
   uncertain chords. Gaps remain silent. Manual progression generation is still
   available with Follow Imported Song off. Use Export Detected MIDI in Tempo &
   Analysis for timestamp-aligned chord MIDI; Apply Detected Chords produces an
   editable chart, which does not encode silent gaps.
5. Preview MIDI through the labeled basic synth or a verified SFZ instrument. For
   sampled audio, Load / Verify SFZ using your licensed library. Preview Audio renders
   off-thread and plays the Guitar lane; Solo Guitar if other lanes obscure it.
   Render WAV, Export WAV and Drag WAV remain available. Change tempo/harmony only
   with subsequent regeneration/rendering; audio is not automatically time-stretched.
6. Drag MIDI into a Logic Software Instrument track and inspect editable Piano Roll
   notes, channels, velocities and lengths. Drag WAV into an audio track, align its
   region start with the MIDI and compare attacks/releases. Export MIDI/WAV remains
   the fallback. Native dragging and host MIDI output require actual host tests.
7. Save/reopen a session. Verify five audio lanes, mix, generated guitar MIDI and
   rendered WAV. SFZ path/engine settings persist; Load / Verify SFZ again after
   restoration. Detected results are recomputed from restored audio rather than
   claimed to remain valid. Test missing samples, cancellation and oversized-library
   rejection. No automatic library/model downloads occur.
8. On the M1 record CPU, process RSS, audio dropouts, buffer size, sample rate and
   elapsed render time with a named library/song; repeat on battery and sustained
   load. No M1 performance figure is established by Linux results.

## Format, licensing and remaining limits

See [sample format/licensing](Samples/README.md). No recorded guitar assets are
bundled. Realism and available articulations depend on the user's licensed recordings.
Supported SFZ is intentionally a strict subset, not arbitrary library compatibility.
Libraries are fully preloaded with a 256 MiB decoded budget; sample-tail streaming
is not implemented. Sample pitch/rate conversion is linear interpolation, not a
band-limited production resampler. Technique-controlled fret/pick/release noise needs
actual mapped recordings; automatic fret-transition noise remains unimplemented.

Audio, MIDI scheduling and sample voices use immutable snapshots/fixed pools without
session locks, disk reads or sample-buffer allocation in the callback. Host-owned
JUCE MidiBuffer storage may grow when generated events are appended; end-to-end
allocator instrumentation under native hosts is still required before claiming
zero allocations for the entire host callback. State serialization, file import
and WAV export retain synchronous caller work; sample loading, analysis, guitar
rendering and separation run in background workers.

Detection uses onset periodicity, chroma/template chords and major/minor profiles,
not a newly trained AI model. Key confidence is a heuristic margin. The chord detector
supports its existing eight qualities; extended manual chord syntax does not imply
extended-chord recognition. Modulation, difficult mixes, tempo ambiguity, tempo maps
and source-separation quality need real-recording evaluation. Maximum audio lane is
120 seconds. Song-follow MIDI can span up to 128 bars, while WAV render including
release must fit the existing 120-second lane limit. Guitar rendering no longer
replaces source tempo analysis or invalidates unchanged-source harmonic results.

Native AU, Logic/MainStage, sandbox permissions, physical devices, MIDI/WAV dragging
and M1 performance require execution on the Mac. No successful native result is
claimed for this revision merely because portable tests pass.
