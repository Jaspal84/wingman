#pragma once
#include "Generation/Patterns.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace wingman {
inline juce::MidiMessageSequence patternSequence(const std::vector<NoteEvent>& notes) {
    juce::MidiMessageSequence sequence;
    for (const auto& note : notes) {
        auto on=juce::MidiMessage::noteOn(note.channel,note.note,static_cast<juce::uint8>(note.velocity)); on.setTimeStamp(note.beat); sequence.addEvent(on);
        auto off=juce::MidiMessage::noteOff(note.channel,note.note); off.setTimeStamp(note.beat+note.duration); sequence.addEvent(off);
    }
    sequence.updateMatchedPairs(); return sequence;
}
}
