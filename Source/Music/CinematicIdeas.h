#pragma once
#include "DSP/Music.h"
#include <random>
#include <stdexcept>
namespace wingman {
// Transparent style rules and seeded templates; not a trained generative AI model.
inline std::vector<Chord> cinematicIdeas(int style,int bars,std::uint32_t seed,bool advanced=true){
    if(style<0||style>7||bars<1||bars>32)throw std::invalid_argument("Invalid cinematic style or length");
    const std::array<std::array<std::string,2>,8> templates{{
        {{"Am9 Fmaj7 Cadd9 Gsus4","Dm(add9) Bbmaj7 Fmaj9 C/E"}},
        {{"Cmadd9 Abmaj7 F#dim G7(b9)","Em9 Cmaj7 Am11 B7(#9)"}},
        {{"Dm9 Bbmaj7 Fadd9 C/E","Cm9 Abmaj9 Ebmaj7 Bb/D"}},
        {{"Cmaj9 Am9 Dm11 G13","Fmaj9 Dm9 Gm11 C7(b9)"}},
        {{"Am9 Gadd9 Fmaj7 E7(b9)","Dm9 Cadd9 Bbmaj7 A7(b9)"}},
        {{"Cadd9 G/B Am9 Fmaj7","Dadd9 A/C# Bm9 Gmaj7"}},
        {{"Dm9 G13 Cmaj9 A7(#9)","Am11 D7(#11) Gmaj13 E7(b13)"}},
        {{"Cmaj9 Ebmaj7 Abmaj9 Fm11","Dadd9 Fmaj9 Bbmaj7 Gm9"}}
    }};
    std::mt19937 random(seed);std::vector<Chord> result;
    for(int bar=0;bar<bars;){
        const auto& text=templates[static_cast<std::size_t>(style)][random()%2];
        std::size_t start=0;
        while(start<text.size()&&bar<bars){const auto end=text.find(' ',start);auto chord=parseChord(text.substr(start,end==std::string::npos?end:end-start));if(!chord)throw std::runtime_error("Invalid cinematic template");
            if(!advanced){chord->quality=chord->minor?ChordQuality::Minor:ChordQuality::Major;chord->inversion=0;}
            result.push_back(*chord);++bar;if(end==std::string::npos)break;start=end+1;
        }
    }
    return result;
}
}
