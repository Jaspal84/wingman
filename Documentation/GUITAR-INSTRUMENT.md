# Guitar instrument development

Development branch: `development/realistic-guitar`.

Baseline inspection (2026-10-25): live 128-voice SFZ playback, velocity layers,
round-robin, release triggers, CC20 directional articulation cues, background loading,
song-follow guitar, MIDI/WAV export and drag controls already exist. Tail streaming
is absent. Baseline sampled_guitar and integration suites pass (2/2, 3.80 seconds).
The baseline does not contain production guitar recordings. The supplied photographs
are visual references, not audio recordings or evidence of sample licensing.

No sample/model download is performed for this work. The permitted user-library
fallback remains necessary until original or explicitly redistributable recordings
are available. Basic Synthesis must remain labeled and must not be described as a
recorded guitar instrument. Native macOS/Logic/M1 tests require the user's Mac.

## Implemented instrument workflow

Load a user-owned SFZ in Guitar Studio. Guitar type selects a separate Steel,
Nylon, 12-string, Clean Electric or Distorted Electric user-library slot. These are
assignments, not bundled recordings or automatic timbre classification. A verified
slot selects sampled playback; an empty slot reports Missing and offers the
explicit Basic Synthesis Fallback. Unload releases the selected slot without deleting
sample files. Total committed decoded bank assets are limited to 256 MiB. A load
can temporarily retain old plus new assets while decoding/publishing; total process
RSS also includes tracks, recording buffers and other application state.

Library Manager reports unique sample count, zones, velocity-limited zones, RR
positions, release zones, paths and named articulation mappings. Verification checks
parsing, decoding, finite/audible data and attack regions, not recording provenance
or redistribution rights. Silent assets and release-only instruments are rejected.
Unsupported SFZ instructions fail explicitly. Supported sample formats are WAV and
AIFF through JUCE's codecs, verified with generated fixtures.

Supported SFZ subset: global/group/region inheritance, numeric key/lokey/hikey,
pitch_keycenter, lovel/hivel, lochan/hichan, seq_length/seq_position, volume, tune,
pan (-100..100 balance), ampeg_attack, ampeg_decay, ampeg_sustain (percent),
ampeg_release, trigger=attack/release, loop_mode=no_loop and sw_last.
No includes, macros, looped sample tails, arbitrary controllers or general SFZ
filter opcodes are claimed. Maximum 2048 zones, 1 MiB SFZ and 120 seconds per asset.

Live preview and offline WAV rendering now use the same SampleVoices engine, with
sample-accurate MIDI offsets, ADSR, pan, velocity layers, RR, releases and a fixed
128-voice pool. Voice stealing fades the removed contribution. Generated string
channels with articulation cues choke prior fretted notes over a short fade; normal
same-channel chord MIDI remains polyphonic. The direct voice-render allocation
probe counts C++ operator new calls, not every possible native allocator or host
MidiBuffer growth. No whole-host zero-allocation claim is made.

MIDI channels 1..6 represent physical strings 6..1, low to high. Capo constrains
physical positions while preserving requested chord pitches. Tuning may be selected
from Standard, Drop D, DADGAD and Open G; saved/API custom tunings retain all six MIDI
pitches. Maximum fret, string mask, open-string preference and prior hand position
influence playable shapes. Positive frets stay within a four-fret span. Sampled
arrangements prefilter key/string coverage; actual velocity/articulation coverage
is checked during rendering. Missing coverage produces an error, not silent audio.

Strum speed, pick direction, mute gate, energy, density, swing, seeded humanization
and four variations affect actual generated notes. Timing remains ordered even on
fast humanized strums. Cached per-chord shapes avoid repeating the voicing search
on every strum. New sessions use Steel, Hollywood Cinematic, Strumming, 65% energy,
60% density, humanization, 4 bars, 4/4 and 48 kHz. Existing saved settings are retained.
The tested example is Am9 | Fmaj7 | Cadd9 | Gsus4.

## Recorded articulation mappings

CC20/sw_last IDs: 24 strumming, 25 down, 26 up, 27 fingerpick, 28 arpeggio, 29 palm
mute, 30 hybrid, 31 accents, 32 dead note, 33 hammer-on, 34 pull-off, 35 slide,
36 legato, 37 vibrato, 38 harmonic, 39 fret noise, 40 pick noise, 41 sustain,
42 string release, 43 resonance. A mapping exists only when the library supplies
corresponding zones. An unavailable explicit advanced articulation fails rather than
being represented by generic sustain. Directional mapping uses available down/up
recordings; generic fallback does not establish recorded directional articulations.

Pick/fret/resonance controls layer actual attack samples mapped to 40/39/43 and are
disabled when those assets are missing. They adjust recorded layers, not synthesized
noise presented as recordings. Palm mute intensity also changes MIDI note gating;
without mute recordings that gate is a DSP/performance approximation. Advanced
mapping triggers supplied recordings; it is not a complete interval-dependent
hammer/slide/legato transition model. That model and production listening tests
remain future work. No parity with Ample Guitar is claimed.

Generate MIDI applies performance changes; Preview MIDI auditions through the active
instrument. Preview Audio renders then plays the Guitar lane; Solo Guitar if desired.
Stop follows internal transport, while DAW sync leaves transport ownership to the
host. Render/Export WAV supports stereo PCM24 at 44.1/48 kHz with release tails and
peak protection; MIDI/WAV drag and export remain available. Native Logic dropping
requires manual verification. Follow Imported Song retains detected chord starts,
ends and gaps; corrections remain editable and do not replace detections with random
progressions. Tempo/key/chord analysis remains heuristic, not a newly trained model.

## Licensing and missing assets

`Resources/Guitar/manifest.json` declares an empty bundled-sample inventory. Neither
production recordings nor the supplied photos are bundled. Built-in realistic Steel
and Nylon are blocked by missing original/public-domain/explicitly redistributable
recordings and their license documents. Other instrument types likewise require
appropriate licensed recordings. Test-generated tones are not production guitars.

For user libraries, the optional inventory helper records hashes and supplied legal
documentation. It does not grant or automatically approve distribution rights:

```bash
python3 Scripts/guitar-library-manifest.py \
  "$HOME/Documents/MyGuitarLibrary" \
  --license-file "$HOME/Documents/MyGuitarLibrary/LICENSE.txt" \
  --license-name "Your actual license or original-recording ownership statement" \
  --source "Original recordings or the actual authorized source" \
  --output "$HOME/Documents/MyGuitarLibrary-manifest.json"
```

This inventories files under that directory only. External SFZ references are not
audited. Do not publish commercial user samples or infer redistribution rights from
a successful load. No sample/model download or Demucs-runtime change occurs here.

## Mac build, verify, then install

The development branch is intentionally separate from main. Close hosts before
replacing a component; preserve the currently working installation until tests pass.

```bash
cd "$HOME/Developer/wingman"
git fetch origin
git switch development/realistic-guitar
git pull --ff-only origin development/realistic-guitar
./Scripts/build-macos.sh
open "build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app"
```

First audition the app directly from the build. Configure an output device, load a
licensed Steel SFZ, verify Ready, Generate, Preview MIDI, Preview Audio, Stop, volume,
WAV export and transport. Repeat with a Nylon slot, switching banks and reopening
sessions. The JUCE input-muted feedback banner concerns input, not evidence of an
output mute. Physical device initialization/output has not been tested in Linux.

After successful build-app checks, install and launch the standalone:

```bash
python3 Scripts/install-standalone-macos.py \
  "build-macos/Wingman_artefacts/Release/Standalone/JASWINGMAN PRO.app"
./Scripts/launch-standalone-macos.sh
```

Before replacing an existing AU, keep a full external backup:

```bash
wingman_au="$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component"
wingman_backup="$(mktemp -d "$HOME/Documents/JASWINGMAN-Guitar-Backup.XXXXXX")"
if [ -d "$wingman_au" ]; then
  /usr/bin/ditto "$wingman_au" "$wingman_backup/JASWINGMAN PRO.component"
fi
./Scripts/install-macos.sh \
  "build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component" --adhoc
arch -arm64 /usr/bin/auval -v aumf Wgm1 Wgma
```

Keep the backup until cold-open/render validation and Logic/MainStage listening tests
pass. If validation fails, close hosts, move the failed component into the backup
folder, and restore the backed-up component with ditto. Do not delete user libraries,
Demucs runtimes/model weights or AU caches as a generic fix. For older differently
named AU duplicates, follow the existing [M1 runbook](PLAYBACK-SONG-GUITAR-M1.md).

In Logic/MainStage test AU loading, internal/DAW sync, all five track mixes, chord
follow, six-channel guitar MIDI, recorded articulations, RR, release tails, MIDI
Piano Roll editing and WAV import alignment. Test missing files, cancellation and
session reopen with manual re-verification. AU IDs remain aumf/Wgm1/Wgma.

M1 fixture measurements (not production-library performance) can be collected with:

```bash
/usr/bin/time -l ./build-macos/wingman_guitar_realtime_tests
/usr/bin/time -l ./build-macos/wingman_sampled_guitar_tests
```

For actual library performance, record sample count/decoded size, sample rate,
buffer size, simultaneous strings/layers, CPU/RSS and dropouts in Activity Monitor
while playing sustained strums in standalone and AUHostingService/Logic. Include
battery/thermal conditions. No M1 CPU, RAM or thermal figures are inferred from Linux.

## Remaining engineering limits

Tails are fully preloaded; streaming is deferred and decoded-bank limits are enforced.
Pitch/sample-rate conversion uses interpolation, not a band-limited production
resampler. Optional EQ/compressor/reverb effects and interval-aware guitar transitions
are not implemented. Recorded resonance layers are not a DSP body model. Maximum
rendered lane length including release remains 120 seconds. Native AU validation,
Logic/MainStage routing/dragging, physical devices, sandbox permissions, M1 sustained
polyphony and production listening tests remain unexecuted for this revision.

## Validation evidence

Linux VST3 and standalone compiled successfully. Final full CTest: **19/19 passed**
in **20.89 seconds**. Portable plugin-off configuration: **16/16 passed**, 16.74 s.
The sampler suites include a 512-frame live/offline equality check, mapped noise
layer gain, unsupported advanced articulation rejection, WAV/AIFF decoding, octave
pitch/rate conversion, pan/ADSR, RR, missing files and release-only rejection.
The realtime stress probe exercises 200 simultaneous note attacks, a 128-voice cap,
voice stealing, per-string choke and release completion while counting C++ allocation
calls. Its guarded render calls observed zero C++ operator new allocations.

ASan/UBSan: **3/3** mapping/generation suites and **2/2** sampler/realtime suites passed.
Leak detection was disabled; these runs do not establish native host allocator,
CoreAudio, Logic or M1 performance behavior. The final verifier/GUI refinements were
rebuilt, and the sampler sanitizer suites were rerun. No real production guitar
listening test or new Demucs inference was performed. No AU validation was possible
on this Linux executor. Existing model/worker/metadata tests passing are not auval.

Machine-readable suite timings/assertion counts:
[guitar-instrument-tests.json](Validation/guitar-instrument-tests.json).
Actual current JUCE UI: [Linux Guitar Studio](Screenshots/guitar-instrument-linux.png).
It explicitly shows Basic Synthesis and unavailable recorded layers; it is not a
screenshot of Logic, a production sample library or a physical audio test.

Working development stages: 78832d4 (inspection/baseline), a0c9709 (unified sampler,
pan/ADSR/stealing), c8fa3a8 (tuning/capo/strum mapping), be230d2 (banks, layers,
performance GUI and licensing workflow). Final QA also rejects silent/release-only
instruments and labels missing recorded-layer controls. Native stage-9 checks remain
for execution on the Mac, not marked passed.

## Changed files

- `CMakeLists.txt`
- `Documentation/GUITAR-INSTRUMENT.md`
- `Documentation/Samples/README.md`
- `Documentation/Screenshots/guitar-instrument-linux.png`
- `Documentation/Validation/guitar-instrument-tests.json`
- `README.md`
- `Resources/Guitar/manifest.json`
- `Scripts/guitar-library-manifest.py`
- `Source/Audio/PlaybackEngine.h`
- `Source/Audio/SampleVoices.h`
- `Source/GUI/GuitarStudioPanel.h`
- `Source/Music/GuitarArticulations.h`
- `Source/Music/GuitarRenderer.cpp`
- `Source/Music/GuitarRhythmGenerator.cpp`
- `Source/Music/GuitarRhythmGenerator.h`
- `Source/Music/GuitarState.h`
- `Source/Music/SampledGuitar.cpp`
- `Source/Music/SampledGuitar.h`
- `Source/Music/SongGuitar.h`
- `Source/PluginProcessor.cpp`
- `Source/PluginProcessor.h`
- `Tests/GuitarPerformanceTests.cpp`
- `Tests/GuitarRealtimeSafetyTests.cpp`
- `Tests/IntegrationTests.cpp`
- `Tests/SampledGuitarTests.cpp`
- `Tests/test_guitar_manifest.py`
