# Product workflow matrix and dependency audit

This matrix maps the workflows requested in the product directive, not a claim of
certified equivalence with a proprietary product. Starting point: commit a20d84e.
The app is an early standalone editor; a working timeline is not a complete DAW.
Status is evidence-based and will be updated with the implementation below.

| Module | Requested workflows | Existing reusable implementation | Missing at audit |
|---|---|---|---|
| A Discovery | Local/cloud library, index, preview, search, match, purchase | WAV/AIFF/FLAC import and waveform peaks | SQLite catalogue, browser, annotations, matching, provider integration; no semantic model |
| B Editor | Multiclip/slice/stretch/pitch/loop/reverse/fades/automation/FX/export | Eight-track editing, trim, split, clipboard, undo, BPM, snap, loop range, gain/fades, v2 projects, WAV bounce | Transient slicing, independent stretch/pitch, reverse, automation, track FX, MIDI arrangement export |
| C Harmonic Match | BPM/key/chords/rhythm, compatibility, Listen | Background tempo and harmonic analyzers in existing plugin | Standalone analysis UI, library ranking, separate Listen plugin/IPC |
| D Inspire | Genre/mood/energy/tempo/key, reproducible arrangements, AI ranking | Seeded pattern generators in existing plugin | Local indexed sample selection, editable generation, validated ML ranking |
| E Flip | Slicing, MIDI triggers, probability/swing/patterns | Tempo onset detection and MIDI infrastructure | Standalone slice model/generator/editor and triggering |
| F Play | MIDI sampler, polyphony, ADSR/layers/maps/presets | SFZ loading/voices and Guitar Studio in existing plugin | General standalone sampler, MIDI device routing and UI |
| G Drum | 16 pads, sequencer, grooves/swing/MIDI export | Deterministic note/pattern/MIDI utilities | Standalone sample pads, sequencing, sample mapping, UI |
| H Effects | Gator/Looper/Utility/Chorus/Bitcrush/EQ/compression/delay/reverb/saturation/distortion | JUCE modules available; track gain/pan/mute/solo | Rack, audio DSP, automation/presets/offline consistency |
| I Bridge/Listen | AU/VST3, host sync, analysis, IPC, drag export | Existing JASWINGMAN PRO telemetry/host synchronization/MIDI drag | New plugin IDs, timestamped IPC protocol, new plugin builds/host tests |
| J Cloud | Accounts, metadata/library/purchase/sync | No backend or licensed sample provider configured | Service contract, authentication, provider licenses, conflict/offline semantics |
| K Jas PRO | Magic Arrange/guitar/strumming/Indian styles/stems/MIDI/accompaniment | Guitar/SFZ, chord-aware patterns, pinned HTDemucs worker in existing plugin | Integration into standalone; actual sample-based arrange/accompaniment |
| UI/Distribution | Navigable panels, production usability, Mac packages | Resizable timeline/inspector, native standalone target | Browser/analysis/generator/instrument/FX navigation, signed/notarized installer, hardware validation |

## Dependency and license decisions

- JUCE 8 is pinned at 51a8a6d7aeae7326956d747737ccf1575e61e209. Its AGPL/commercial
  terms require an appropriate distribution license. Preserve JASWINGMAN PRO
  `com.wingmanaudio.wingman`, `Wgma/Wgm1`, AU `aumf`; new products need distinct IDs.
- C++20, CMake 3.22+, system SQLite3 (public domain) for the local catalogue.
  SQLite uses parameterized statements, versioned schema, WAL, busy timeout and
  separate connections for background writes and GUI reads. No network required.
- Existing FFT/chroma/tempo algorithms are deterministic heuristics, not trained
  AI. Detected values need confidence/provenance and manual correction. Instrument,
  genre and mood annotations must be user supplied unless validated detection is
  later added. No natural-language embedding or genre model is currently present.
- Similarity can initially use documented audio descriptors and key/tempo distance;
  call it descriptor matching rather than learned semantic similarity.
- Independent pitch/time requires a reviewed processing dependency (e.g. a JUCE
  compatible permissive or commercially licensed processor); resampling changes
  pitch and speed together and must be labelled accordingly. Rubber Band's
  GPL/commercial terms and SDK distribution obligations need explicit review.
- Sample assets are user imported/licensed. Do not bundle commercial samples or
  imply a provider catalogue exists. Original generated patterns may use supplied
  sounds. SFZ libraries retain their own recording/redistribution requirements.
- HTDemucs: existing explicit model/license consent, pinned hash, Python runtime,
  PyTorch/Demucs and MIT notices remain required. Do not download weights silently.
  Model inference is separate from ordinary library/editor operation.
- AU/Logic/Apple Silicon validation requires macOS hardware, Xcode SDK, selected
  CoreAudio devices, installation permissions and host testing. Linux cannot
  produce or validate native Apple installers, Audio Units or signed Mac binaries.
- Signed/notarized distribution needs the owner's Apple Developer signing identity,
  team ID and notarization credentials through secure configuration. An unsigned
  package can be produced on a Mac without inventing those credentials.
- Cloud purchasing/sync needs a specified licensed provider and account backend,
  API/auth contracts, entitlements and secure credential configuration. None is
  configured. No accounts, remote entitlement data or purchase endpoints may be
  fabricated. No secret values belong in source or chat.

## Dependency order and acceptance gates

1. Catalogue + local browser: persistent search/annotations, indexing/cancellation,
   duplicates, visible waveform and real audition/insertion.
2. Standalone analysis + matching + deterministic Inspire/Magic Arrange: confidence,
   manual metadata, actual imported assets, reproducibility, undo and persistence.
3. Flip/editing and audio transformations: source bounds, audible output, timing,
   exact project round trips and render parity.
4. Drum/Play and Guitar integration: real MIDI/audio, polyphony/voice lifetimes,
   reproducible patterns, MIDI export and complete saved state.
5. Effects/automation: tested impulse/signal behavior, bounded callback work and
   real-time/offline consistency, then UI/presets.
6. Bridge/Listen: isolated product identifiers, protocol/transport timing tests,
   native AU/VST3 builds, Logic session and disconnection tests.
7. Provider-backed cloud, Mac packaging/signing, large-library benchmarking and
   physical Apple Silicon end-to-end acceptance.

Every stage requires executable tests and an integrated user workflow. A library
class or decorative panel does not satisfy a module's acceptance gate. Do not
mark the ecosystem complete while any requested workflow remains unimplemented.

## Current delivered matrix (0.3, after the initial audit)

The audit table above records the starting point. This table records the current
implementation; **partial** is not a completed acceptance gate.

| Module | Implemented in this delivery | Status / remaining |
|---|---|---|
| A | SQLite/FTS5/WAL migration; cancellable incremental folder index; hashes/duplicates; full-text and BPM/key/mode/favorite filters; editable tags/collection; waveform; real audition; native source drag; resizable browser/timeline dock | Partial: no embedding/semantic model, detected genre/mood/instrument, ordered playlists, licensed provider/purchase adapter; large-library performance unmeasured |
| B | Milestone A retained; source-bounded slices, linked speed, reverse; v3 persistence; effects in the shared renderer; Flip/Drum/Guitar MIDI output | Partial: independent stretch/pitch/key preview, automation and sample-level/transient-marker editing remain missing |
| C | Standalone BPM/key/confidence/chord/onset inspection, manual BPM/key correction, deterministic harmonic/descriptor ranking | Partial: 30-second heuristic analysis, no standalone Listen plugin or rhythmic structure model |
| D | Seeded genre/mood/energy/tempo/key sample selection and editable Inspire/Magic Arrange; actual reference audio; per-track regeneration with explanations | Partial: no ML ranking, chord-progression-following arrangement or musical quality acceptance on recorded material |
| E | Uniform/onset slicing, seed, probability, swing, reversal, editable source regions and paired MIDI trigger export | Partial: no dedicated slice-marker editor, live MIDI-triggered slice instrument or preset bank |
| F | 16 mappings, two velocity layers, chromatic pad-1 Play, 64 voices, ADSR, block-quantized MIDI input, persistent presets/project state | Partial: default root/split UI, no sophisticated multisample/SFZ mapping editor, sustain/bend or sample-exact scheduling; physical MIDI untested |
| G | 16 pads/16 steps, three original groove families, swing, commit real audio regions, MIDI export and persistence | Partial: no live transport-linked sequencer (commit then play), arbitrary-length pattern editor or broad traditional rhythm coverage |
| H | Eleven original/JUCE processors, per-track enable/amount, four rack presets, meters, project persistence and tested offline/live renderer parity | Partial: low-pass EQ only, fixed topology/time mappings, unsmoothed changes, fixed tails; no automation/sidechain/limiter or production performance validation |
| I | Existing JASWINGMAN PRO host sync and identities preserved | Unfinished: no new Bridge/Listen plugin products, IPC transport or Logic host validation in this delivery |
| J | Configurable Supabase auth/refresh/logout client, private audio storage abstraction/checksums/consent, persistent per-account revision outbox, cloud panel, RLS/storage migration, mocked client integration tests and real disposable PostgreSQL owner/RLS tests; future provider interface | Prepared/partial: no configured/live project or hosted auth/storage validation, signup/recovery/OAuth/MFA, automatic conflict merge, remote audio browser, commercial adapter or purchasing |
| K | Existing chord-aware guitar generator/basic original renderer integrated with retained WAV/MIDI; real reference/sample-based Magic Arrange and drum accompaniment | Partial: complete SFZ/Guitar Studio UI, HTDemucs standalone integration, intelligent generated bass and chord-progression-aware arrangement remain unfinished |
| UI/package | Seven working navigation views, adjustable library dock, existing native targets, unsigned macOS packaging script and exact build/audition instructions | Partial: no finished commercial UI claim, native pkg execution, signing/notarization, M1/CoreAudio/AU/Retina/Logic acceptance or measured production performance |

## Dependencies added and boundaries retained

SQLite3 with FTS5 and libcurl/TLS are required by the standalone. JUCE DSP and
cryptography are used for audio processing and SHA-256. No new inference model,
commercial sample SDK or plugin identity change is included. Supabase is optional:
SUPABASE_URL and SUPABASE_PUBLISHABLE_KEY remain unset until the owner creates a
project. See `SUPABASE.md` for deployment and RLS acceptance. No service-role key
belongs in a desktop app. `PRODUCT-WORKFLOWS.md` documents exact UI behavior and
limits; `STUDIO-MAC-DISTRIBUTION.md` documents native packaging/validation.

## 0.4 interface checkpoint

`CREATE-WORKSPACE.md` supersedes the earlier default navigation: functional Library,
Discovery and AI Create columns; explicit eight musical roles; exact layer locks and
replacement; direct editor/WAV/sample-trigger MIDI transfer; separate Arrangement,
Flip, Play, Drum, Effects, Mixer and Settings tabs. The underlying unfinished product
modules and hardware/cloud validation requirements above remain unfinished. The UI
uses no proprietary reference assets or catalogue.
