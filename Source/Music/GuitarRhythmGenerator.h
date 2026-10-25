#pragma once
#include "Generation/Patterns.h"
#include <functional>
namespace wingman {
enum class GuitarTechnique { Strumming, Downstroke, Upstroke, Fingerpicking, Arpeggio, PalmMute, Hybrid, Accents };
enum class GuitarStyle { Hollywood, PunjabiPop, Bollywood, Indie, Folk, Rock, Jazz, Ambient };
enum class GuitarType { Steel, Nylon, TwelveString, CleanElectric, DistortedElectric };
struct GuitarSettings {
    GuitarTechnique technique=GuitarTechnique::Strumming;
    GuitarStyle style=GuitarStyle::Hollywood;
    GuitarType type=GuitarType::Steel;
    double energy=0.65,density=0.75,swing=0.15;
    bool humanize=true;
    int bars=4,variation=0,numerator=4,denominator=4;
    std::uint32_t seed=42;
};
inline constexpr std::array<int,6> guitarTuning{40,45,50,55,59,64};
// -1 means muted string. Open strings or frets within a four-fret hand span.
std::array<int,6> guitarVoicing(const Chord&,int transpose=0);
void validateGuitarSettings(const GuitarSettings&);
std::vector<NoteEvent> generateGuitar(const std::vector<Chord>&,int transpose,const GuitarSettings&,const std::function<bool()>& next={});
}
