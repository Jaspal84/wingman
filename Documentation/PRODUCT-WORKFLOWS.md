# Offline product workflows (0.3 development)

For the current 0.4 default three-column UI and separate tabs, read
[Create workspace](CREATE-WORKSPACE.md). This document retains the earlier
checkpoint controls and product limitations as background.

This extends Milestone A; it is not feature parity with a finished commercial
workstation. All audio comes from user-imported licensed files or explicitly
labelled original guitar synthesis. No commercial catalogue, ML classifier,
embedding model, account or sample entitlement is fabricated.

## Build and launch

Linux needs C++20, CMake >=3.22, pinned JUCE 8, SQLite3 with FTS5, libcurl with TLS,
ALSA, FreeType/fontconfig and X11 development packages. In this managed environment:

```bash
. /workspace/tooling/activate.sh
cd /workspace/wingman
cmake --build build-cloud --parallel 3
xvfb-run -a ctest --test-dir build-cloud --output-on-failure
```

The saved environment installer prepares the native dependencies without system
writes. JASPRO_DATA_DIR selects the persistent local data directory; the cloud
uses `/workspace/tooling/jaspro-data`. Without it, the native app uses the user's
application data directory under `JasProStudio`. It stores SQLite/WAL files,
explicitly generated guitar WAV/MIDI and per-account synchronization outboxes.
Source audio remains external. The app does not need Supabase to start or edit.

For a graphical desktop, launch the `JasProStudio` artefact without `--self-test`.
The headless self-test drives controls and renders all seven views at three sizes;
it does not exercise a physical audio or MIDI device.

## Local library and Harmonic Match

Choose **Library + Match**, Index folder and select a licensed source directory.
WAV, AIFF and FLAC are indexed recursively without following directory symlinks.
Only mono/stereo 8-192 kHz audio is supported. Indexing has cancellation, incremental
size/mtime checks, full source SHA-256, waveform summaries, tempo/confidence,
key/confidence, onset timing, chord estimates and RMS/zero-crossing descriptors.
A cancelled scan retains completed records; rejected files are counted and the
last error is displayed. Missing/renamed files are not automatically pruned.

SQLite schema migration includes FTS5 with triggers. Search is a conjunction of
filename and user-tag tokens, with favorite, duplicate, BPM (+/-6), key and mode
filters. Results page in batches of 100. This is indexed lexical search, not an
embedding-based semantic engine. Find similar ranks the current candidate page
using BPM/key compatibility, RMS, zero crossings and duration; it is not a global
nearest-neighbor index or a trained sound model. Large-library timing is unmeasured.

Select a row to inspect the waveform and actual analysis evidence. BPM/key/chord
analysis uses at most the first 30 seconds at 22.05 kHz; later changes in a long
song are not analyzed. Confidence is heuristic, not a calibrated probability.
Genre/mood/instrument are **user tags**. No genre/mood/instrument classifier runs.
Save corrected BPM/key and tags, favorite and a single collection name locally.
Collection tags provide grouping; ordered playlists are not implemented.

Audition or double-click plays real audio; Stop preview stops it. Tempo match uses
linked sample-rate conversion, which changes pitch together with tempo. It is not
independent time stretching or key-matched pitch shifting. Add to track inserts at
the playhead on the last selected timeline track. Drag source to DAW uses native
file dragging (host support needs native validation).

Choose **Library + Timeline** for a compact dock beside the editor. Drag its divider
to resize the browser. Drag a catalogue row into the visible timeline at the desired
beat/track. Full metadata editing remains in Library + Match. No preview clips or
silent placeholders are generated when no sources are available.

## Inspire and Magic Arrange

Tag actual samples with instrument names such as Kick, Snare, Hi-Hat, Bass, Chords,
Guitar, Melody, Vocals, FX, Dhol or Tabla. Dhol/Tabla/percussion map to the percussion
lane. Choose genre/mood filters only when the library has matching user tags.

Use as reference transfers a selected sample's corrected BPM/key to **Inspire /
Flip**. Set key/mode, genre/mood, energy (RMS preference), bar count and seed.
Inspire uses the project tempo; Magic Arrange uses the reference tempo when known
and retains actual reference audio on track 7, non-destructively limited to the selected bar span. Both select compatible local assets
and generate editable regions. Unknown instrument categories and missing matches
remain empty with an explanation. The initial ranking uses global key/relative or
fifth relationships, tempo distance and RMS; it does not follow a detected chord
progression or promise musical quality on arbitrary songs. The selection pool is
currently capped at the first 2000 indexed candidates.

Tempo ratios beyond 0.8-1.25 are rejected for selected loops; accepted ratios use
linked pitch/rate. One-shots use original rhythmic placements and seeded density.
The explanation identifies selected files, score, region count and rate. Identical
inputs/settings/library produce identical musical timing and sample choices (new
region UUIDs differ). New arrangement generation replaces the current arrangement
in one undo transaction. Regenerate one track replaces that track's regions while
preserving its mixer and effects. Inspect/listen and save before relying on output.

The guitar action reuses the existing chord-aware generator and original basic
plucked-string renderer. Enter chord symbols such as `C Am F G` (optional `:beats`),
choose bars/energy/seed/genre and Generate guitar audio + MIDI. It writes retained
files under `generated`, inserts the actual WAV on track 5 and reports the MIDI
path. This is basic synthesis, not realistic sampled guitar. Existing SFZ/Guitar
Studio and HTDemucs remain available in JASWINGMAN PRO; their complete integration
into this standalone is unfinished. No inference weights are downloaded.

## Flip and rate/reverse editing

Select a timeline region, then choose slices, seed, probability, swing and reverse
in Inspire / Flip. Flip replaces it with actual source-bounded regions in one undo
transaction. Detected onset slicing uses available onsets from the source's first
30 seconds; if none are found it falls back to uniform slicing. Uniform slicing
works over the selected source span. Probability can leave silent gaps; zero-result
patterns retain the original region with an error. Reverse and linked speed are
non-destructive region properties persisted in v3 projects; independent stretch and
pitch remain unimplemented. Fade and waveform display follow the source mapping.

Export slice MIDI writes paired trigger timing (notes 36-51), not embedded audio or
an automatic DAW sampler map. Map the original samples/slices in the destination
sampler. MIDI-triggered live slice performance and a dedicated transient-marker
editor are unfinished.

## Play and Drum

Select a sample in the library, choose Play / Drum, select a pad and assign it.
The high-velocity layer is optional (default split 90). Empty pads are labelled;
no commercial recordings are bundled. Drum mode uses MIDI notes 36-51. Play mode
maps pad 1 chromatically around C4, with sample-rate pitch transposition, 64-voice
polyphony, velocity and ADSR. Hold the on-screen keys or select an available MIDI
input. All notes off stops voices. Events cross into audio through a bounded
256-event multi-producer queue; overflow reports failure internally and safely
silences pending voices. Events are quantized to the next audio block; sample-exact
MIDI timestamps, sustain/pitchbend, SFZ mapping UI and sophisticated voice stealing
remain future work. A velocity layer and root mapping can be stored in presets;
the UI currently uses the default C4 root and split of 90.

The 16 steps edit the selected pad. Choose original modern, Bhangra/dhol-inspired
or tabla-inspired grooves and swing. These are original patterns, not recordings
or a claim of exhaustive traditional rhythmic accuracy. Assign your own appropriate
sounds. Commit Drum to timeline inserts only assigned sample hits as editable audio
regions; use timeline transport to preview the sequence. Export Drum MIDI writes
channel 10, notes 36-51. Live pad ADSR and committed timeline fades are different
envelope representations; this is not a parity claim between those two modes.

Save/Open preset writes/loads all pad references, velocity/root mapping, ADSR,
pattern/swing. Save project includes the instrument preset as well as the complete
arrangement/effects. Missing instrument assets abort project replacement. Keep
original audio and generated files in place. Instruments and step edits are not
currently included in arrangement undo history. UI MIDI performance requires a
physical device/host to validate; automated tests use synthetic events.

## Effects / Mixer

Select a track. Enable an effect and set its normalized amount, or choose Dry,
Dub echo, Lo-fi or Space. Eleven functional processors are included: Gator,
feedback-repeat Looper, Utility, Chorus, Bitcrush, low-pass EQ, Compressor, Delay,
Reverb, Saturation and Distortion. JUCE DSP supplies filtering/compression and
JUCE supplies reverb; the remaining transformations are original simple DSP.
Utility amount 0.5 is unity. Gator follows project beats; delay/repeat times are
fixed mappings, not an editable tempo-synchronized delay designer. Meters show
actual post-track output peaks. Use the timeline gain/pan/mute/solo controls.

Project files persist effects. Timeline playback and offline WAV use the same
processors; delay/reverb/chorus/repeat allow a fixed two-second export tail.
Tails can be truncated at that limit. Parameters and bypass changes are not
smoothed; avoid changing them during critical playback. EQ is a low-pass filter,
not a parametric multi-band suite. Effect order is fixed. No automation lanes,
side-chain routing, limiter, plugin hosting or independently saved FX presets exist.
The master clamps overloads. Physical latency, throughput and cold-cache dropouts
remain unvalidated. Audio memory maps can fault even though no decoder runs in the
callback.

## Cloud and remaining acceptance

Cloud account actions are visibly unavailable until SUPABASE_URL and
SUPABASE_PUBLISHABLE_KEY are set. See `SUPABASE.md` for schema, auth, private storage,
explicit metadata sync/remote-wins behavior and live RLS tests. No project exists
for this delivery, so no live cloud claim is made. Commercial sample adapters,
purchases and entitlements remain unimplemented.

The full requested ecosystem is unfinished. Remaining work includes validated local
semantic/genre/mood models; independent stretch/pitch/key preview; automation;
advanced instruments/complete SFZ and stem integration; new Bridge/Listen AU/VST3
plugins and timestamped standalone communication; real Logic validation; live
Supabase/RLS tests; production-scale performance and signed/notarized distribution.
Existing JASWINGMAN PRO identifiers and its plugin behavior are preserved.

## Executed verification

Debian 13 x86_64, GCC 14.2, Debug, pinned JUCE 8.0.6, SQLite 3.46.1/FTS5:
all build targets succeeded, including the existing standalone/VST3. All 22 CTest
suites passed in 24.68 seconds, with zero skipped suites. The original arrangement
suite passed 79 assertions and the expanded offline/mocked-client suite passed
360 assertions. The latter includes queue capacity/safety checks; this count is
not a count of independently completed product features.

The GUI test drives editor operations and library/generator/instrument/FX handlers,
checks navigation bounds and renders all seven views at 1100x740, 1400x850 and
2400x1500. Assertions fail the runner. Screenshots use synthetic audio and an empty
user catalogue; no commercial sounds are staged. A real temporary PostgreSQL 17.11
cluster passes the migration/idempotence/owner/RLS/revision/storage-path tests with
fixture auth/storage contracts, then stops and is removed. The reusable setup was
rerun successfully with all 22 suites. Machine-readable JUnit/JSON results are in
`Documentation/Validation/studio-product-linux-tests.*`.

The Mac packaging script passed shell syntax, help and Linux platform-guard checks;
it was not executed on macOS and no native pkg was produced. No physical output,
controller, M1, CoreAudio, AU, Logic, hosted Supabase or model inference was tested.
Those are separate acceptance requirements, not implied by these Linux results.
