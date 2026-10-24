# 0.2.0 development snapshot

This is not a validated release or signed distribution.

Incremental commits before final documentation:

- `9cfc5c8`: explicit MIDI-capable Music Effect category and compiled-plist AU installation/validation tools; six fixture tests.
- `f217061`: resizable vector editor, stereo balance and compatible optional UI state.
- `89cf6fc`: cancellable spectral harmonic analysis, eight chord qualities, tuning/key estimates, clickable inversion/duration editing and timeline zoom/scroll.
- `e34e3a4`: deterministic seeded bass/drum/melody generation, settings UI, MIDI export/drag requests, drum steps, phrase variations and persisted generated notes.
- `178bd90`: host playhead clock, seeks/loops/missing-position handling, note ownership/chase, bypass, fixed preview voice pool and finite/clipped live output.

`87cdf17` adds an embedded, pinned Python-worker interface, explicit model-install consent, offline checksum verification, download progress/cancellation and atomic four-stem import validation. It does not establish that actual model inference succeeds. `1208ec4` replaces SDK-sensitive jthread/stop_token usage with tested portable cancellation primitives. Official checkpoint access and all macOS/M1 acceptance remain outstanding.

Compatibility: bundle identifier, manufacturer/subtype and root/version-1 project state are retained. New properties are optional; old state remains readable. Product display name changes to Wingman Pro. Avoid duplicate installed AU bundles with the same identity. Generated settings/notes and inference paths are additive state properties.

Known gaps: no recorded-song chord accuracy corpus, no verified separation model run or M1 benchmarks, no Logic/MainStage/physical audio device tests, no notarized packaging, generators assume 4/4, no full piano-roll drag editing/candidate browser, no calibrated analysis confidence, no lock-free session exchange or complete audio-callback allocation proof. Read the feature-specific documents before claiming production readiness.
