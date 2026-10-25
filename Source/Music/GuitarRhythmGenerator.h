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
    double energy=0.65,density=0.60,swing=0.15;
    bool humanize=true;
    int bars=4,variation=0,numerator=4,denominator=4;
    std::uint32_t seed=42;
    std::array<int,6> tuning{40,45,50,55,59,64};
    int capo=0,maxFret=12,stringMask=63,pickDirection=0,articulationOverride=0;
    bool preferOpen=true;
    double strumSpeed=.5,palmMute=.5;
};
inline constexpr std::array<int,6> guitarTuning{40,45,50,55,59,64};
// -1 means muted string. Open strings or frets within a four-fret hand span.
std::array<int,6> guitarVoicing(const Chord&,int transpose=0);
std::array<int,6> guitarVoicing(const Chord&,int,const GuitarSettings&,const std::array<int,6>* previous=nullptr);
void validateGuitarSettings(const GuitarSettings&);
std::vector<NoteEvent> generateGuitar(const std::vector<Chord>&,int transpose,const GuitarSettings&,const std::function<bool()>& next={},const std::array<int,6>* previous=nullptr);
}
