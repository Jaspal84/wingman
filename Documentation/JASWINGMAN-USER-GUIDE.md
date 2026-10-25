# JASWINGMAN PRO user guide

Development build: five-track audio/MIDI workstation and local AI stem worker.
Mac/Logic acceptance is pending. Start with a saved copy of an existing project.

## Stems & Tracks

Import Audio into Vocals, Drums, Bass, Other or Guitar. WAV, AIFF, FLAC and MP3
are decoded by registered JUCE formats; M4A depends on the platform decoder.
Inputs are mono/stereo, 8–192kHz and at most 120 seconds. Recording goes into the
selected lane. In standalone, use headphones and configure/unmute input through
the JUCE Options/Settings controls before recording to avoid feedback.

Play/Stop, S (solo), M (mute), rotary gain and horizontal stereo balance are real
mixer controls. Export individual WAV or a mix. Track Drag prepares a raw
48kHz stereo PCM24 WAV in a background cache; wait and retry if preparation is
not finished. It works for imported/recorded/rendered lanes, not only model
outputs. Drag is before lane gain/pan; Export WAV includes mixer settings.

For HTDemucs, configure the existing local Python and model directory in Model
Manager, then Verify local model. Import/drop one mixed audio file in the splitter
and click Separate Stems. Progress/cancel/error status are displayed. Successful
jobs replace Vocals/Drums/Bass/Other transactionally; Guitar stays independent.
The four-stem model does **not** isolate Guitar. No silent model download occurs.

MIDI controls below the mixer preserve the existing chord, bass, drum and melody
generators, parameter panels and editing. Guitar MIDI is an additional choice.
Hold a blue Drag control and move at least six pixels before dropping. Use Export
MIDI if the DAW rejects a file drag. External host acceptance has not been tested
in Logic here. Select a matching instrument in your DAW; MIDI contains notes,
not the instrument samples. Drag caches expire after seven days through bounded
background cleanup; copy audio assets into the DAW project for permanent storage.

## Guitar Studio

Default: steel-string timbral profile, Hollywood/Cinematic style rules, strumming,
65% energy, 4 bars, 4/4, humanization and project tempo/chords/key. Guitar type,
style, technique, variation, length, meter, energy/density/swing and seed affect
actual generation or rendering. Generated settings and notes are saved in plugin
state. A UI draft becomes project data when you generate/render it.

Generate MIDI produces standard-tuning, playable-fret performances. String-specific
channels preserve overlapping strums; attacks are staggered. Four variations and
fixed seeds are reproducible. Export/Drag MIDI preserve tempo, meter, velocities,
attacks and releases at 960 PPQ. MIDI-only generation does not create audio.

Render WAV generates notes and performs offline stereo plucked-string synthesis,
then fills the actual Guitar lane. Preview renders first and starts session
playback; solo Guitar for isolated listening. The rate selector supports 44.1
and 48kHz PCM24 export. Cancel preserves the previous audio. At slow tempos,
choose fewer bars to fit the lane's 120-second limit (including release tail).

**All five type choices are basic original synthesis profiles**, not realistic
sampled steel/nylon/12-string/electric instruments. No commercial sample library
is included. Sound-quality listening and production guitar realism remain work.

## Chords & Ideas

Choose one of eight cinematic style-rule families, bars, complexity and seed.
Generate/Regenerate fills an editable progression; Apply to Project creates the
actual harmony and chord MIDI. Optional smooth voice leading reduces pitch travel.
Templates are transparent seeded rules, not a trained AI chord-generation model.

Examples: `Am9 Fmaj7 Cadd9 Gsus4` and `Dm(add9) Bbmaj7 Fmaj9 C/E`.
Supported qualities include major/minor triads, sevenths, ninths, add9, sus2/sus4,
elevenths, thirteenths, altered dominants and sixths. `/E` sets slash bass,
`^1` sets inversion and `:2` sets two quarter-note beats. For example,
`Cmaj9/E^1:2`. Use spaces or bars between chords. Invalid tokens are rejected.

Detect from Audio invokes the existing background harmonic detector. It still
models the original eight qualities; it does not identify every new extension.

## Tempo & Analysis

Audio import/record/render/separation starts background onset/pulse analysis.
Select the desired lane to re-analyse it. Measured BPM, heuristic confidence,
half/double alternatives and beat markers are distinct from project BPM. Silence
or insufficient rhythmic evidence shows no measured result. Apply Detected Tempo
changes project tempo explicitly. Tap Tempo and the header BPM provide manual
correction. DAW Sync follows host transport/tempo and disables local tempo apply.
Guitar meter is manually selected; automatic meter estimation is not implemented.

Detect Key / Chords shares the existing harmonic worker. Reported key margins and
BPM confidence are heuristics, not calibrated probabilities. Synthetic rhythm
checks do not guarantee correct beat/octave choices on arbitrary recordings.

## Settings, state and Mac testing

Settings exposes the existing local-model configuration and an About dialog.
The rename preserves bundle/AU/VST3 IDs and the `Wingman` state root. Version-2
state stores five tracks, guitar notes/settings and voice-leading selection;
version-1 four-track sessions load with an empty Guitar lane. Audio remains
embedded, so large states can be expensive. Old standalone preferences can be
copied with the non-overwriting migration script before first launch.

See [Mac M1 runbook](JASWINGMAN-MAC-M1.md) for exact update/build/install/auval,
model verification and manual Logic/MainStage checks. Native host drag acceptance,
AU sandbox behavior, physical device playback, Retina/keyboard accessibility and
M1 CPU/RAM performance still need local verification. Session locking, synchronous
legacy import/export and large state operations remain engineering limitations.
