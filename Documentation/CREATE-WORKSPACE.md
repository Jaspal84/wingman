# Jas PRO Studio 0.4: Library / Discovery / AI Create

The default workspace follows the user's reference screenshot's general three-column
workflow. All graphics, styling, labels and code are original Jas PRO work. No Splice
branding, assets, sample catalogue, accounts, purchasing data or proprietary code is
used. This is a local-first interface to the existing sample database and Inspire
engine, not a claim of a new trained generation model or a finished music ecosystem.

## Default workspace

**Library (left):** Browse and My Sounds show the indexed local catalogue (there is
no commercial provider). Collections filters actual collection tags; its selector
lists persisted collections. Favorites uses the database favorite flag. Drum Loops
uses percussion instrument tags plus duration >=0.8 seconds; One-shots uses shorter
sources. Instruments excludes drums, vocals and FX; Vocals uses voice/vocal tags.
Local folders scopes results to the selected indexed directory and its descendants.
Folder roots persist in an additive side table; existing sample/FTS data is retained.

Index folder recursively scans licensed WAV/AIFF/FLAC, preserving metadata and existing
source files. Cancel scan retains completed entries. Missing or unsuitable files are
reported rather than replaced with synthetic content. Instrument/genre tags are user
annotations, not fabricated classifier output. Source paths and collection tags can
be inspected/corrected in Settings / Sample metadata.

**Discovery (center):** the large search field uses existing SQLite/FTS5 token search.
Rows draw actual indexed waveform summaries and show filename, instrument/genre,
BPM and original musical key. Row play previews real audio; a second click stops it.
The star updates favorites without changing tempo/key provenance. Similar sounds
uses the existing descriptor/key/tempo ranker over the current candidate page; it is
not an embedding-based search engine. Results paginate in groups of 100.

Match BPM filters +/-6 BPM and enables linked-rate audition. Match key filters the
original source key selected in AI Create. If both are selected, the BPM filter is
restricted to +/-0.01 BPM, so a shifted-tempo preview is not misleadingly presented
as key-preserving. Independent time stretch/pitch shift is not implemented. Match key
requires an explicit musical key; unknown-key sources are not called a match.

Drag a row onto a Create card to assign real audio, or hover the Arrangement tab and
continue onto the desired timeline track. Dropping onto that tab inserts at the
playhead. To track is the direct insertion alternative. Drag to DAW starts native
file dragging of the original source; its destination host must support the gesture.
Edit tags opens that exact source in Settings metadata, where BPM/key and instrument,
genre, mood, favorites and collections can be corrected. Instrument assignment and
cloud upload use the most recently selected library source across these views.

**AI Create (right):** select genre, style, BPM, musical key, seed and bars. Balanced,
Sparse and Driving change Inspire's energy/density preference; no genre/mood model
is implied. Generate uses the current seed. Regenerate increments it and prefers
eligible alternatives when available. Candidate selection is capped at 10,000;
refine genre/tags if the matching pool exceeds that limit.

The eight actual musical roles are Drums, Bass, Chords, Keys, Guitar, Synth, FX and
Vocals. They adapt the existing Inspire decoder/ranker/placement engine into eight
musical layers. No changes were made to the existing five-track plugin audio engine.
Missing roles remain visibly empty, with a message naming the required content.
Decoded silent regions are rejected; no silent placeholders or bundled catalog
sounds are inserted. Full combinations can be partial when some roles lack content.

When a key is selected, pitched sources must have known compatible key metadata.
Pitched loops with a known tempo must already be at the target BPM; one-shots keep
original pitch. This avoids claiming that linked resampling preserves harmonic key.
With Any key, the existing pitch-linked tempo behavior remains available. Unpitched
Drums/FX are not assigned a fake harmonic key. Harmonic compatibility follows the
existing deterministic tonic/relative/fifth rules; estimates and tags need audition.

A populated card names its actual source(s). Lock preserves its exact regions and
shared assets across generation. Replace targets only that unlocked layer and excludes
current sources and known duplicate hashes. If no alternative exists, the existing
audio stays in place with an informative message. Locked layers are never silently
replaced. Global BPM/key changes can alter compatibility; lock is not a time-stretch
or pitch-correction operation. Empty cards cannot be locked.

Preview combination plays the actual mixed layers using the existing TimelineEngine.
Stop combination, the bottom Pause/Stop and normal timeline transport operate real
playback. A Create preview stops timeline playback and lingering instrument notes.
The bottom BPM control follows AI Create in Discover and follows the editor in other
views. Arrangement loop controls are disabled in Discover to avoid changing a hidden
editor loop during sample creation.

To Arrangement copies all eight layers into the editor in one undo transaction;
prior timeline content can be restored with Undo. Track names become the musical
role names. Save editor persists that arrangement, effects and instrument state in
the existing v3 project schema. Optional track-name attributes are backward compatible:
v1/v2/v3 projects without names still use their original lane labels. The scratch
Create session's locks and seed are not saved in the editor project; send/save the
actual arrangement before closing. Source audio is referenced, not embedded.

Export WAV renders the actual composition to 48 kHz PCM24 stereo with the shared
renderer. MIDI triggers exports a tempo track and eight named tracks with paired
sample-trigger events (channel 10, notes 36-43). It is timing for sampler mapping,
not audio transcription or embedded recordings. Match source mapping/slices in the
destination DAW; generated audio loops do not contain authored instrument note data.

## Other tabs

- Arrangement retains multiclip movement, trim/split, copy/paste, snap, waveform
  zoom, BPM, loop, gain/fades, undo/redo, save/open and WAV bounce.
- Flip opens working slicing/probability/swing/reverse and MIDI trigger export.
- Play opens the chromatic pad-1 sampler, ADSR, velocity layers, MIDI input and
  instrument presets. Drum opens the 16 pads/steps, grooves, swing, MIDI and timeline
  commit separately. Both reuse the existing instrument state/engine.
- Effects retains the working per-track rack and meters.
- Mixer has eight gain/pan/mute/solo strips connected to editor state, undo and
  persistence, plus actual track/master meters.
- Settings includes audio-device configuration, exact-source library metadata,
  optional cloud account and the retained advanced Inspire/Guitar controls.

The left and right column dividers resize independently, preserving a usable center
list. Layer cards scroll on shorter windows. Layouts are checked at 1100x740,
1400x850 and 2400x1500, plus a 2x component render. JUCE vector graphics/fonts follow
native display scaling. Physical macOS Retina, CoreAudio and DAW drag behavior still
require the native tests below; a Linux screenshot is not proof of those behaviors.

## MacBook Air M1 build and validation

Use the existing checkout of `development/omniloop-eight`; preserve personal edits.
Install Xcode Command Line Tools and CMake >=3.22 (`xcode-select --install`, then
`brew install cmake` if needed). Use a native arm64 terminal (`uname -m`).

```bash
cd "$HOME/Developer/wingman"
git fetch origin
git switch development/omniloop-eight
git pull --ff-only origin development/omniloop-eight
cmake -S . -B build-create-m1 -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DWINGMAN_BUILD_STUDIO=ON
cmake --build build-create-m1 --parallel 2
ctest --test-dir build-create-m1 --output-on-failure
open "build-create-m1/JasProStudio_artefacts/Debug/Jas PRO Ultimate Loop Studio.app"
```

SQLite/FTS5 normally comes from the macOS SDK. If your SDK lookup fails, use the
SQLite setup described in `STUDIO-MAC-DISTRIBUTION.md`. No inference weights or
Supabase variables are needed. On a multi-config generator, build with `--config
Debug` and test with `-C Debug`. For Release use a separate Release build directory;
`Scripts/package-studio-macos.sh` creates the optional native 0.4.0 package on macOS.
No Mac package was created or installed by this Linux task.

On the M1:

1. Confirm Discover is the default, resize both dividers, scroll all eight cards
   and check Retina text/waveforms at the minimum window size.
2. Index a licensed folder. Add appropriate instrument/genre/BPM/key metadata.
   Verify category/collection/folder/favorite filters, similar results and row
   preview/stop. Test Match BPM and Match key with known source fixtures.
3. Generate audible layers. Lock Drums, regenerate and confirm its source/timing
   stay intact. Replace Bass with a genuinely different compatible source. With
   only one eligible source, verify the current layer is retained with a message.
4. Preview, pause and stop the complete combination through the chosen CoreAudio
   output. Export WAV, listen in another player, and inspect the MIDI trigger tracks.
5. Send To Arrangement, undo/redo the transfer, save/reopen and compare every source
   and role name. Confirm existing older projects still load and playback correctly.
6. Drag from Discovery over Arrangement onto a track; test native source dragging
   into Logic. Test Play, Drum, Flip, Effects, Mixer and audio settings. Verify real
   controller input, device switching, loop boundaries and close during background
   generation/index/export.
7. Measure CPU, latency and cold-cache behavior with your actual files and buffer
   sizes. Memory-mapped PCM can still fault; parameter changes remain unsmoothed.
   Run existing AU validation separately for JASWINGMAN PRO. No new Listen/Bridge
   plugin or full commercial product completion is claimed by this UI checkpoint.

## Linux acceptance checkpoint

After the final catalogue fix, all 24 configured CTest suites passed (26.72s),
including 109 Create/discovery/export assertions, 79 arrangement assertions and
360 offline workflow/mocked-cloud assertions. The legacy catalogue fixture migrates
v1 to v2 with every sample field and favorite intact. Three further reopenings
preserve exact search identities, FTS contents, folders, schema objects and both
SQLite schema versions. Opening a current catalogue no longer writes user_version.

The GUI runner checks empty-library messaging, unavailable replacement feedback,
real audio generation/preview and project transfer/reopening, three layout sizes
and 2× offscreen rendering. Screenshots in Documentation/Screenshots use explicitly
labelled synthetic integration-test audio, not a shipped sample catalogue.
JUnit and JSON evidence are in Documentation/Validation/create-workspace-linux-tests.*.
See [native M1 workflow](MACBOOK-M1-VALIDATION.md) for automated build/install/AU
validation and the manual Logic Pro/Retina acceptance checklist. None of those
Apple-specific results were executed on Linux.
