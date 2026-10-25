#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
namespace wingman {
inline juce::Result writeMidi(const juce::File& file,juce::MidiMessageSequence sequence,double bpm,
                             int numerator,int denominator,double beats,const juce::String& name){
    if(!std::isfinite(bpm)||bpm<40||bpm>240||!std::isfinite(beats)||beats<=0)return juce::Result::fail("Invalid MIDI tempo or length");
    sequence.updateMatchedPairs();juce::MidiMessageSequence events;
    events.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(60000000/bpm)));
    events.addEvent(juce::MidiMessage::timeSignatureMetaEvent(numerator,denominator));
    events.addEvent(juce::MidiMessage::textMetaEvent(3,name));
    for(int i=0;i<sequence.getNumEvents();++i){
        const auto* event=sequence.getEventPointer(i);auto message=event->message;
        if(!std::isfinite(message.getTimeStamp())||message.getTimeStamp()<0||message.getTimeStamp()>beats+0.000001)return juce::Result::fail("Invalid MIDI event timestamp");
        if(message.isNoteOn()&&(!event->noteOffObject||event->noteOffObject->message.getTimeStamp()<=message.getTimeStamp()))return juce::Result::fail("Unpaired MIDI attack");
        message.setTimeStamp(message.getTimeStamp()*960);events.addEvent(message);
    }
    auto end=juce::MidiMessage::endOfTrack();end.setTimeStamp(beats*960);events.addEvent(end);
    juce::MidiFile midi;midi.setTicksPerQuarterNote(960);midi.addTrack(events);juce::TemporaryFile temporary(file);
    {juce::FileOutputStream out(temporary.getFile());if(!out.openedOk()||!midi.writeTo(out))return juce::Result::fail("Cannot write MIDI file");out.flush();if(out.getStatus().failed())return out.getStatus();}
    return temporary.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Cannot replace MIDI destination");
}
}
