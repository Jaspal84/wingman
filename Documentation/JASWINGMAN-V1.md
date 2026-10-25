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
