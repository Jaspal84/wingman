#pragma once
#include "DSP/Music.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace wingman {
inline juce::MidiMessageSequence makeChords(const std::vector<Chord>& chords, int transpose) {
    juce::MidiMessageSequence result;
    double beat = 0;
    for (const auto& chord : chords) {
        for (int note : chord.notes(transpose)) {
            result.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(90)), beat);
            result.addEvent(juce::MidiMessage::noteOff(1, note), beat + chord.beats - std::min(0.1,chord.beats*0.1));
        }
        beat += chord.beats;
    }
    result.updateMatchedPairs();
    return result;
}
}
