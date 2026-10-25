#pragma once
#include "GuitarRhythmGenerator.h"
#include "AI/ChordDetection/HarmonicAnalysis.h"
namespace wingman {
// Region starts and ends are absolute song seconds; gaps remain silent.
inline std::vector<NoteEvent> guitarFromSong(const HarmonicResult& song,GuitarSettings& settings,double bpm,const std::function<bool()>& next={}){
    if(song.regions.empty()||!std::isfinite(bpm)||bpm<40||bpm>240)throw std::invalid_argument("Analyze imported audio before generating a song arrangement");
    for(const auto& r:song.regions)if(!std::isfinite(r.start)||!std::isfinite(r.end)||r.start<0||r.end>120||r.end<=r.start)throw std::invalid_argument("Invalid song region duration");
    auto checked=settings;checked.bars=1;validateGuitarSettings(checked);
    const double bar=settings.numerator*4.0/settings.denominator;
    const double total=song.regions.back().end*bpm/60;
    settings.bars=static_cast<int>(std::ceil(total/bar));validateGuitarSettings(settings);
    std::vector<NoteEvent> result;double previousEnd=0;std::size_t index=0;
    for(const auto& region:song.regions){
        if(!std::isfinite(region.start)||!std::isfinite(region.end)||region.start<previousEnd-.0001||region.end<=region.start)throw std::invalid_argument("Invalid or overlapping detected chord timeline");previousEnd=region.end;
        if(region.confidence<.001)continue;
        const double length=(region.end-region.start)*bpm/60,offset=region.start*bpm/60;
        auto local=settings;local.bars=std::max(1,static_cast<int>(std::ceil(length/bar)));local.seed+=static_cast<std::uint32_t>(index++*2654435761u);
        auto chord=region.chord;chord.beats=length;
        for(auto note:generateGuitar({chord},0,local,next)){if(note.beat>=length)continue;note.duration=std::min(note.duration,length-note.beat);note.beat+=offset;if(note.duration>.001)result.push_back(note);}
    }
    if(result.empty())throw std::runtime_error("No sufficiently supported chord regions for guitar arrangement");
    return result;
}
}
