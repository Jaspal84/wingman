#pragma once
#include "GuitarRhythmGenerator.h"
#include <juce_data_structures/juce_data_structures.h>
namespace wingman {
inline juce::var saveGuitar(const GuitarSettings& s,const std::vector<NoteEvent>& notes){
    juce::ValueTree tree("Guitar");
    tree.setProperty("technique",static_cast<int>(s.technique),nullptr);tree.setProperty("style",static_cast<int>(s.style),nullptr);tree.setProperty("type",static_cast<int>(s.type),nullptr);
    tree.setProperty("energy",s.energy,nullptr);tree.setProperty("density",s.density,nullptr);tree.setProperty("swing",s.swing,nullptr);tree.setProperty("humanize",s.humanize,nullptr);
    tree.setProperty("bars",s.bars,nullptr);tree.setProperty("variation",s.variation,nullptr);tree.setProperty("numerator",s.numerator,nullptr);tree.setProperty("denominator",s.denominator,nullptr);tree.setProperty("seed",static_cast<juce::int64>(s.seed),nullptr);
    tree.setProperty("capo",s.capo,nullptr);tree.setProperty("maxFret",s.maxFret,nullptr);tree.setProperty("stringMask",s.stringMask,nullptr);tree.setProperty("preferOpen",s.preferOpen,nullptr);tree.setProperty("strumSpeed",s.strumSpeed,nullptr);tree.setProperty("palmMute",s.palmMute,nullptr);tree.setProperty("pickDirection",s.pickDirection,nullptr);tree.setProperty("articulationOverride",s.articulationOverride,nullptr);for(int i=0;i<6;++i)tree.setProperty("tuning"+juce::String(i),s.tuning[static_cast<std::size_t>(i)],nullptr);
    for(const auto& note:notes){juce::ValueTree n("Note");n.setProperty("beat",note.beat,nullptr);n.setProperty("duration",note.duration,nullptr);n.setProperty("pitch",note.note,nullptr);n.setProperty("velocity",note.velocity,nullptr);n.setProperty("channel",note.channel,nullptr);n.setProperty("articulation",note.articulation,nullptr);tree.addChild(n,-1,nullptr);}
    juce::MemoryOutputStream out;tree.writeToStream(out);return juce::var(out.getMemoryBlock());
}
inline void loadGuitar(const juce::var& data,GuitarSettings& settings,std::vector<NoteEvent>& notes){
    settings={};notes.clear();if(const auto* bytes=data.getBinaryData()){
        auto tree=juce::ValueTree::readFromData(bytes->getData(),bytes->getSize());if(!tree.hasType("Guitar")||tree.getNumChildren()>8192)return;
        GuitarSettings s;s.technique=static_cast<GuitarTechnique>(static_cast<int>(tree["technique"]));s.style=static_cast<GuitarStyle>(static_cast<int>(tree["style"]));s.type=static_cast<GuitarType>(static_cast<int>(tree["type"]));
        s.energy=tree["energy"];s.density=tree["density"];s.swing=tree["swing"];s.humanize=tree["humanize"];s.bars=tree["bars"];s.variation=tree["variation"];s.numerator=tree["numerator"];s.denominator=tree["denominator"];s.seed=static_cast<std::uint32_t>(static_cast<juce::int64>(tree["seed"]));
        s.capo=tree.getProperty("capo",0);s.maxFret=tree.getProperty("maxFret",12);s.stringMask=tree.getProperty("stringMask",63);s.preferOpen=tree.getProperty("preferOpen",true);s.strumSpeed=tree.getProperty("strumSpeed",.5);s.palmMute=tree.getProperty("palmMute",.5);s.pickDirection=tree.getProperty("pickDirection",0);s.articulationOverride=tree.getProperty("articulationOverride",0);for(int i=0;i<6;++i)s.tuning[static_cast<std::size_t>(i)]=tree.getProperty("tuning"+juce::String(i),guitarTuning[static_cast<std::size_t>(i)]);
        // Validate configuration without trusting serialized enum/range values.
        validateGuitarSettings(s);
        const double length=s.bars*s.numerator*4.0/s.denominator;std::vector<NoteEvent> decoded;
        for(auto n:tree){NoteEvent note{static_cast<double>(n["beat"]),static_cast<double>(n["duration"]),static_cast<int>(n["pitch"]),static_cast<int>(n["velocity"]),static_cast<int>(n["channel"])};
            note.articulation=n.getProperty("articulation",0);if(note.articulation!=0&&(note.articulation<24||note.articulation>43))throw std::invalid_argument("Invalid guitar articulation");
            if(!std::isfinite(note.beat)||!std::isfinite(note.duration)||note.beat<0||note.duration<=0||note.beat+note.duration>length+1e-6||note.note<0||note.note>127||note.velocity<1||note.velocity>127||note.channel<1||note.channel>6)throw std::invalid_argument("Invalid saved guitar note");decoded.push_back(note);
        }
        std::stable_sort(decoded.begin(),decoded.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});settings=s;notes=std::move(decoded);
    }
}
}
