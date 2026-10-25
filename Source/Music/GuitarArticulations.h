#pragma once
#include <array>
namespace wingman {
inline constexpr std::array<const char*,20> guitarArticulations{"Strumming","Downstroke","Upstroke","Fingerpick","Arpeggio","Palm mute","Hybrid","Accents","Dead note","Hammer-on","Pull-off","Slide","Legato","Vibrato","Harmonic","Fret noise","Pick noise","Sustain","String release","Resonance"};
// IDs 24-43 are SFZ sw_last / CC20 mappings. Presence means a mapping exists,
// never proof that the user's sample is an original guitar recording.
}
