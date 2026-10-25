#pragma once
#include "DSP/Music.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace wingman {
inline juce::MidiMessageSequence makeChords(const std::vector<Chord>& chords, int transpose,bool voiceLeading=false) {
    juce::MidiMessageSequence result;
    double beat = 0;
    std::vector<int> previous;
    for (const auto& chord : chords) {
        auto voicing=chord.notes(transpose);
        if(voiceLeading&&!previous.empty()&&chord.bass<0){
            double best=1e9;auto chosen=voicing;
            for(int inversion=0;inversion<static_cast<int>(voicing.size());++inversion)for(int shift:{-12,0,12}){
                auto c=chord;c.inversion=inversion;auto candidate=c.notes(transpose);for(auto& n:candidate)n+=shift;
                if(candidate.front()<36||candidate.back()>88)continue;
                double score=0;for(int n:candidate){int nearest=127;for(int old:previous)nearest=std::min(nearest,std::abs(old-n));score+=nearest;}score+=std::abs(candidate.front()-previous.front())*0.5;
                if(score<best){best=score;chosen=candidate;}
            }voicing=chosen;
        }
        previous=voicing;
        for (int note : voicing) {
            result.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(90)), beat);
            result.addEvent(juce::MidiMessage::noteOff(1, note), beat + chord.beats - std::min(0.1,chord.beats*0.1));
        }
        beat += chord.beats;
    }
    result.updateMatchedPairs();
    return result;
}
}
