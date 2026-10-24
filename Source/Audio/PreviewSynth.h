#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
namespace wingman {
// Fixed voice pool, owned exclusively by the audio thread between prepare/release.
// No Synthesiser mutex, heap allocation, sample loading or instrument inference.
class PreviewSynth {
    struct Voice {
        bool active=false, drum=false;
        int note=0,channel=1;
        double phase=0,delta=0;
        float level=0,lastNoise=0;
        std::uint32_t noise=1;
        juce::ADSR envelope;
    };
    std::array<Voice,32> voices;
    double rate=44100;
    std::size_t steal=0;
    void event(const juce::MidiMessage& message) {
        if(message.isAllNotesOff()||message.isAllSoundOff()) {
            for(auto& voice:voices)if(voice.channel==message.getChannel()){voice.active=false;voice.envelope.reset();}
        } else if(message.isNoteOff()) {
            for(auto& voice:voices)if(voice.active&&voice.note==message.getNoteNumber()&&voice.channel==message.getChannel())voice.envelope.noteOff();
        } else if(message.isNoteOn()) {
            auto found=std::find_if(voices.begin(),voices.end(),[](const auto& v){return !v.active;});
            auto& voice=found!=voices.end()?*found:voices[steal++%voices.size()];
            voice.active=true;voice.note=message.getNoteNumber();voice.channel=message.getChannel();voice.drum=voice.channel==10;
            voice.phase=0;voice.level=message.getFloatVelocity()*0.12f;voice.noise=static_cast<std::uint32_t>(voice.note+1);voice.lastNoise=0;
            const double frequency=voice.drum? (voice.note==36||voice.note==35?55:voice.note==38||voice.note==39?180:110):juce::MidiMessage::getMidiNoteInHertz(voice.note);
            voice.delta=juce::MathConstants<double>::twoPi*std::min(frequency,rate*0.45)/rate;
            voice.envelope.reset();voice.envelope.setSampleRate(rate);
            voice.envelope.setParameters(voice.drum?juce::ADSR::Parameters{0.001f,0.1f,0,0.02f}:juce::ADSR::Parameters{0.01f,0.12f,0.65f,0.02f});
            voice.envelope.noteOn();
        }
    }
    void span(juce::AudioBuffer<float>& output,int begin,int end) {
        for(auto& voice:voices)if(voice.active)for(int sample=begin;sample<end;++sample){
            float value=static_cast<float>(std::sin(voice.phase));
            voice.phase+=voice.delta;if(voice.phase>=juce::MathConstants<double>::twoPi)voice.phase-=juce::MathConstants<double>::twoPi;
            if(voice.drum&&voice.note!=36&&voice.note!=35){
                voice.noise=voice.noise*1664525u+1013904223u;const float noise=static_cast<float>(voice.noise)/2147483648.0f-1;
                value=voice.note==42||voice.note==46?(noise-voice.lastNoise)*0.5f:noise*0.7f+value*0.3f;voice.lastNoise=noise;
            }
            value*=voice.level*voice.envelope.getNextSample();
            for(int channel=0;channel<output.getNumChannels();++channel)output.addSample(channel,sample,value);
            if(!voice.envelope.isActive()){voice.active=false;break;}
        }
    }
public:
    void prepare(double sampleRate){rate=sampleRate;allNotesOff();}
    void allNotesOff(){for(auto& voice:voices){voice.active=false;voice.envelope.reset();}}
    void renderNextBlock(juce::AudioBuffer<float>& output,const juce::MidiBuffer& midi,int start,int count){
        int cursor=start;
        for(const auto metadata:midi){
            if(metadata.numBytes>3)continue; // Preview does not consume SysEx.
            const int at=juce::jlimit(start,start+count,metadata.samplePosition);
            span(output,cursor,at);event(metadata.getMessage());cursor=at;
        }
        span(output,cursor,start+count);
    }
};
}
