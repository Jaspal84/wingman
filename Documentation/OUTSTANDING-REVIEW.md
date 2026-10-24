# Outstanding GUI, callback and host review

Reviewed during the successful Linux HTDemucs validation increment. Inference and new failure/quality tests do not resolve the following independent release limitations.

| Area | Existing behavior / risk | Required follow-up |
| --- | --- | --- |
| Session exchange | Callback uses a nonblocking try-lock; edits, clip swaps and state serialization can silence blocks. Owned-note recovery prevents indefinite stuck generated notes. | Immutable lock-free snapshots with deferred destruction; stress under real audio and record discontinuities. |
| MIDI buffer | Generated scheduling is bounded, but JUCE's host-owned MidiBuffer may grow when events are appended. | Allocation instrumentation and bounded output strategy across real wrappers; no zero-allocation proof is claimed. |
| File/state/UI | Import, WAV/MIDI export, waveform construction and large embedded-state copies still run synchronously on their callers. | Background jobs, cached/downsampled waveform data, progress/cancellation and measured memory limits. |
| Model process | Actual CPU inference and supervised cancellation now pass on Linux. HTDemucs still pads to its 7.8-second trained context, despite four-second outer chunks. | Native M1 memory pressure and DAW responsiveness; separate worker RSS from total app/DAW memory; no real-time inference claim. |
| Model quality | Four synchronized, finite/nonempty/distinct outputs pass. Current real fixture has very quiet vocal/bass estimates; no isolated sources or listening assessment. | Vocal-rich licensed recordings, source references and perceptual evaluation; mixture consistency is not source SDR. |
| GUI | Original resizable vector layout, real controls, settings panels, timeline/step/note editing and persisted state render in tests. | Native Retina/keyboard/screen-reader testing; painted timeline/grid accessibility; hide irrelevant shared generator fields; richer candidate/piano-roll UX. |
| Musical analysis | Synthetic chord/key sets pass; confidence margins are uncalibrated. Silence gaps, inversion ambiguity and dense arrangements remain difficult. | Annotated real recordings, beat-aware segmentation, calibrated confidence and rest representation. |
| Meter / host clock | Host BPM/PPQ/meter/loops and discontinuities pass mock tests. Patterns remain 4/4; seconds fallback cannot reconstruct historical tempo maps. | Non-4/4 generation, tempo-ramp/map tests, real host positions and wrapper MIDI timing. |
| AU / routing | Type aumf and identity are preserved; install/validator reads actual compiled metadata. | Actual macOS build/auval, correct Logic MIDI-controlled-effect slot, input routing and live MIDI-out limitations; MainStage tested separately. |
| Release | Native runtime/build/signing/M1 performance are unrun. | MACBOOK-M1-TESTING.md, licence/distribution choices, signing/notarization and packaged-release acceptance. |

No trained-model work occurs on the audio callback: the model runs in an owned external process. That architectural separation does not itself guarantee dropout-free host audio while CPU, disk or session locks are under load.
