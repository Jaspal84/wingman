#pragma once
#include "Patterns.h"
#include <juce_data_structures/juce_data_structures.h>
namespace wingman {
inline juce::ValueTree saveSettings(const PatternSettings& s) {
    juce::ValueTree tree("Pattern");
    tree.setProperty("seed",static_cast<juce::int64>(s.seed),nullptr);
#define WINGMAN_SAVE(field) tree.setProperty(#field,s.field,nullptr)
    WINGMAN_SAVE(variation); WINGMAN_SAVE(bars); WINGMAN_SAVE(octave); WINGMAN_SAVE(velocity); WINGMAN_SAVE(hatSubdivision);
    WINGMAN_SAVE(lowest); WINGMAN_SAVE(highest); WINGMAN_SAVE(density); WINGMAN_SAVE(swing); WINGMAN_SAVE(humanize);
    WINGMAN_SAVE(noteLength); WINGMAN_SAVE(complexity); WINGMAN_SAVE(rootEmphasis); WINGMAN_SAVE(syncopation);
    WINGMAN_SAVE(kickDensity); WINGMAN_SAVE(snareVariation); WINGMAN_SAVE(fill); WINGMAN_SAVE(repetition);
    WINGMAN_SAVE(ghostNotes); WINGMAN_SAVE(minor); WINGMAN_SAVE(keyRoot);
#undef WINGMAN_SAVE
    tree.setProperty("style",static_cast<int>(s.style),nullptr);
    for (int i=0;i<9;++i) tree.setProperty("map"+juce::String(i),s.drumMap[static_cast<std::size_t>(i)],nullptr);
    return tree;
}
inline PatternSettings loadSettings(const juce::ValueTree& tree) {
    PatternSettings s;
    s.seed=static_cast<std::uint32_t>(static_cast<juce::int64>(tree.getProperty("seed",static_cast<juce::int64>(1))));
#define WINGMAN_LOAD(field) s.field=tree.getProperty(#field,s.field)
    WINGMAN_LOAD(variation); WINGMAN_LOAD(bars); WINGMAN_LOAD(octave); WINGMAN_LOAD(velocity); WINGMAN_LOAD(hatSubdivision);
    WINGMAN_LOAD(lowest); WINGMAN_LOAD(highest); WINGMAN_LOAD(density); WINGMAN_LOAD(swing); WINGMAN_LOAD(humanize);
    WINGMAN_LOAD(noteLength); WINGMAN_LOAD(complexity); WINGMAN_LOAD(rootEmphasis); WINGMAN_LOAD(syncopation);
    WINGMAN_LOAD(kickDensity); WINGMAN_LOAD(snareVariation); WINGMAN_LOAD(fill); WINGMAN_LOAD(repetition);
    WINGMAN_LOAD(ghostNotes); WINGMAN_LOAD(minor); WINGMAN_LOAD(keyRoot);
#undef WINGMAN_LOAD
    const int style=tree.getProperty("style",0); if(style<0 || style>7) throw std::invalid_argument("Invalid style"); s.style=static_cast<Style>(style);
    for(int i=0;i<9;++i) s.drumMap[static_cast<std::size_t>(i)]=tree.getProperty("map"+juce::String(i),s.drumMap[static_cast<std::size_t>(i)]);
    validateSettings(s); return s;
}
}
