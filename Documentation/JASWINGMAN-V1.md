# JASWINGMAN PRO v1 development verification

Stage 1: renamed visible product and build artifacts to JASWINGMAN PRO. The CMake
target remains Wingman, bundle ID com.wingmanaudio.wingman, AU manufacturer Wgma,
subtype Wgm1 and MusicEffect type aumf. All three VST3 class IDs were compared
against the prior Wingman Pro Linux bundle and are unchanged. State root Wingman
and existing Python/model paths remain compatible. Imported/recorded WAV lanes
now receive background-prepared 48k stereo PCM24 drag files, independent of any
separation job. Cached drag files use the existing seven-day retention policy.
MIDI held gestures and Export fallbacks are preserved.

Stage 1 Linux: renamed VST3 and standalone built; 8/8 CTest suites passed (13.17s).
Background WAV drag creation/readability is covered by integration tests. This
run did not opt into real inference; prior model validation is separate.
Physical macOS AU, Logic/MainStage behavior and M1 performance are untested here.

Stage 2: five independent mixer/record/import/waveform/export/drag lanes, including
Guitar. State version 2 stores five tracks; version-1 four-track states still
load with an empty Guitar lane. The model importer intentionally remains exactly
four stems and does not overwrite Guitar. Linux build and 8/8 CTest passed
(13.38s), including five-track round trip and generated legacy-state loading.

Stage 3: standard-tuning, fret-constrained, chord-aware guitar MIDI engine with
seeded strums, eight techniques, eight style rules, five synthesis-profile
selections and four variations. Per-string channels and staggered attacks avoid
simultaneous block chords and repeated-note ambiguity. Background generation,
selected Guitar MIDI export/drag, and generated note/config state are connected.
Linux VST3/standalone build and 9/9 CTest passed (13.27s), including guitar SMF
readability, PPQ, release pairing and note/config round trip. Guitar audio is not
implied by this MIDI stage. Musical styles are programmed rules, not an AI model.

Stage 4: original Karplus–Strong-based stereo guitar preview renderer, background
render/cancel, actual Guitar lane audio publication and 44.1/48k PCM24 exports.
This is basic synthesis; the five type selections are timbral profiles, not
realistic sampled instruments. No commercial samples or weights are used.
Linux build and 10/10 CTest passed (15.34s), including finite/nonclipping stereo
rendering at both rates, readable exported WAV and cancelled-render preservation.

Stage 5: automatic background onset/pulse analysis on import, recording, model
stem import and guitar rendering. Results, half/double candidates, heuristic
confidence and beat markers are separate from project BPM; applying the result
is explicit and disallowed during DAW Sync. Existing synchronous detectTempo UI
call was removed. Synthetic clicks and rhythm loops pass at 80/100/120/140 BPM;
silence and cancellation are covered. Linux build and 11/11 CTest passed (19.25s),
including automatic-import analysis and explicit apply. No real-song accuracy or
meter-detection claim. Tap Tempo and detailed controls are integrated in Stage 7.

Stage 6: advanced chord parsing/canonical names, slash bass, extended chord MIDI,
optional voice leading, matching guitar voicings, and eight seeded cinematic
style-rule families. Existing eight-quality chord detection is preserved; it is
not advertised as a detector of all new extensions. Linux build and 12/12 CTest
passed (13.80s), including 16 advanced chord forms, malformed input rejection,
slash bass voicing, template validity across styles/seeds, and readable extended
voice-led MIDI with matched releases. The expanded chord editor includes slash
bass and all supported qualities. No external AI chord model is downloaded.


Stage 7: responsive five-workspace GUI, actual Guitar Studio controls and piano
roll, cinematic editable ideas, measured-tempo/tap/apply controls, key/chord
analysis and settings/About. Six main mixer/MIDI drag controls remain within all
four tested window sizes. GUI callbacks generate actual guitar notes and 48k WAV
audio, and apply real chord data. Waveforms use prepared stereo peak caches;
analysis handles right-only stereo input. New renders cannot overwrite a Guitar
lane changed during the job. Producer jobs are stopped before tempo consumers
on teardown. Guitar note lifetimes terminate before a new pitch on the same
string; ringing overlap remains across strings. Extended ninth-chord inversion
bass/order and standalone preference migration have regression coverage.

Final Linux VST3/standalone builds passed. Full regression: **13/13 suites**,
**1035 integration checks**, 53.94s. JUCE-free portable build: **12/12**, 34.18s.
Actual-model opt-in run: **13/13**, **1068 integration checks**, 86.12s. The latter
was followed by regression reruns for screenshot-file truncation and per-string
MIDI lifetimes; the AI worker/model integration did not change in those fixes.

Real 20-second music/AIFF conversion: four finite, readable, distinct stereo
44100Hz PCM24 WAVs, **882000 frames** each. Independent Guitar remained unchanged.
Worker elapsed **44.9295s**, pure model **42.4208s**, peak worker RSS
**1543168000 bytes**. Cancellation after a real chunk took **150.553ms**. Mixture
reconstruction SNR **31.98dB**, not isolated-source SDR. Quiet vocals/bass warnings
mean this fixture does not establish their semantic/perceptual quality. Raw model
and input audio are not redistributed in Git. Benchmark, quality and acceptance
summaries are in `Validation/jaswingman-v1-*.json`.

The new GUI images in Screenshots are actual JUCE Linux renders using test audio,
not native Mac screenshots. The four pre-existing colored lanes use synthetic
fixtures; Guitar shows actual basic synthesized audio. No realistic sample
library, trained chord generator, CoreML/ONNX acceleration, or guitar-isolating
stem model has been added. Guitar profiles are explicitly basic synthesis and
cinematic ideas are style rules.

macOS AU build/auval, Logic/MainStage external drop acceptance, physical audio,
M1 performance/thermals/RAM, sandbox permissions, security bookmark persistence,
and native Retina/accessibility checks remain unexecuted. Session locking may
skip blocks; legacy synchronous import/export and large embedded state operations
remain limitations. Do not treat version 1.0 metadata as production acceptance.
See JASWINGMAN-MAC-M1.md for exact local commands and acceptance checklist.

Source groups changed: CMake/visible metadata and scripts; five-track processor,
state and editor; AudioDragCache and waveform peaks; Music/GuitarRhythmGenerator,
GuitarRenderer, GuitarState and CinematicIdeas; Analysis/TempoAnalysis; MIDI export
and voice leading; GuitarStudio/Cinematic/Tempo/Settings GUI panels; automated
music/audio/state/UI/AI tests; current docs and screenshots. Seven stable stage
commits preserve the existing target, identifiers and model installation paths.
