#pragma once
#include "DSP/Music.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace wingman {
inline juce::MidiMessageSequence makeChords(const std::vector<Chord>& chords, int transpose) {
    juce::MidiMessageSequence result;
    for (std::size_t bar = 0; bar < chords.size(); ++bar)
        for (int note : chords[bar].notes(transpose)) {
            result.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(90)), bar * 4.0);
            result.addEvent(juce::MidiMessage::noteOff(1, note), bar * 4.0 + 3.9);
        }
    result.updateMatchedPairs();
    return result;
}
}
