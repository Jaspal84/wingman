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
