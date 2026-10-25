# User-licensed sampled guitar instruments

No guitar recordings or third-party sample libraries are bundled or downloaded.
Supply an SFZ and recordings you are licensed to use. Loading verifies decoding,
finite audio, numeric zone ranges and resource limits; it does NOT verify ownership
or grant redistribution rights. Keep the library's original license, attribution,
source and purchase information with the library. Steel, nylon, twelve-string,
clean and distorted electric sounds require corresponding recordings. The existing
Guitar Type selector changes only the explicitly labeled basic synthesizer.

## Supported SFZ subset

UTF-8 text, `<global>`, `<group>`, `<region>`, `//` comments; sample paths relative
to the SFZ (spaces and quoted paths supported). Numeric MIDI key values only.
Opcodes: sample, key, lokey, hikey, pitch_keycenter, lovel, hivel, lochan, hichan,
seq_length, seq_position, ampeg_release (seconds), volume (dB), tune (cents),
trigger=attack/release, loop_mode=no_loop, sw_last.
Unsupported opcodes/sections fail verification rather than silently changing a library.
This is NOT a general-purpose SFZ player. Includes, macros, loops, controllers,
filters, streaming, disk sample compression and general live keyswitches are unsupported.

Guitar MIDI channels 1–6 identify strings low to high. lochan/hichan can map
string-specific recordings. Overlapping zones layer; lovel/hivel select velocity
layers. Sequence selection advances once per generated note per string, resets at
each render, and selects `(counter % seq_length) + 1`. Use identical sequence
lengths within a round-robin group. Missing attack coverage fails the whole render.

`sw_last` optionally selects recorded articulations from Guitar Studio Technique:
24 strumming, 25 downstrokes, 26 upstrokes, 27 fingerpicking, 28 arpeggio,
29 palm mute, 30 hybrid, 31 rhythmic accents. This is a render-time selection,
not a claim that down/up directions are encoded in exported MIDI. Automatic mixed
strum direction sample switching is not implemented. Pick, fret/finger noise and
release sounds require actual recordings supplied as layered/release zones; no
synthetic noise is labeled as a recorded articulation.

Example (provide your own licensed WAVs):

```sfz
<group> lokey=40 hikey=88 pitch_keycenter=64 ampeg_release=0.4
<region> sample=soft.wav lovel=1 hivel=64
<region> sample=hard1.wav lovel=65 hivel=127 seq_length=2 seq_position=1
<region> sample=hard2.wav lovel=65 hivel=127 seq_length=2 seq_position=2
<region> sample=release.wav trigger=release volume=-18
```

## Use and limits

In Guitar Studio choose Load / Verify SFZ. A background worker decodes and validates
all unique files before publishing an immutable library. Cancel preserves the last
successful library. Sampled Guitar is displayed only after successful verification.
Select Sampled, generate MIDI, then Preview or Render WAV. Preview renders off-thread
and plays the Guitar lane with existing transport/mixer; it is not a live MIDI
sample instrument. Export WAV uses the existing stereo 24-bit export at 44.1/48 kHz;
Drag WAV and MIDI retain the existing host file drag implementation.

All samples are preloaded (no streaming): maximum 256 MiB decoded assets, 2048 zones,
1 MiB SFZ, 120 seconds per sample and rendered lane including release. Oversized
libraries fail with an explanation. Rendering uses linear interpolation for sample
rate/pitch conversion (not a band-limited resampler), 2 ms onset ramp, linear release,
recorded natural sample decay, and peak attenuation above 0.95. Native stereo is
preserved; mono is duplicated. This is a bounded initial sampled engine, not a claim
of production-library parity or measured M1 optimization.

State retains engine choice, velocity response and library path, but deliberately
does not open sample files in host state callbacks. After restoring a sampled
session, use Load / Verify SFZ again. Existing rendered Guitar lane and MIDI state
remain compatible. Missing samples produce errors, never an automatic silent
synthesis fallback. Select Basic Synthesis explicitly if desired.

## Build and manual verification

On the M1 Mac, close hosts before installing:

```bash
cd "$HOME/Developer/wingman"
./Scripts/build-macos.sh
./Scripts/install-macos.sh "build-macos/Wingman_artefacts/Release/AU/JASWINGMAN PRO.component" --adhoc
./Scripts/validate-au.sh "$HOME/Library/Audio/Plug-Ins/Components/JASWINGMAN PRO.component"
ctest --test-dir build-macos --output-on-failure
```

AU IDs remain aumf/Wgm1/Wgma. A metadata PASS alone is not successful AU validation;
OpenAComponent, rendering and final AU validation must also pass. Previous user
output showed Wingman Pro 0.2.0 failing cold open; resolution is still unverified.
Test the new installed version, architecture and duplicates before attributing this
to any particular cause.

Manual Logic/MainStage checks: load AU, import a licensed multisampled instrument,
compare velocity/round-robin/string mapping, render and audition, drag exported MIDI
and WAV to compatible tracks, compare region starts/note lengths, save/reopen session,
reload SFZ, remove a sample and confirm a useful error, cancel load/render, monitor
M1 memory and audio dropouts. None of these native-host checks has run on Linux.
