#pragma once
#include "Music/SampledGuitar.h"
#include <juce_audio_basics/juce_audio_basics.h>
namespace wingman {
// Fixed voices, predecoded samples: no heap allocation, file reads or locks here.
class SampleVoices {
    struct Voice {bool active=false,released=false;int zone=0,note=0,channel=0,velocity=0;double position=0,step=1,age=0,releaseAge=0,gain=0;int articulation=24;double releaseLevel=1,releaseDuration=0;};
    std::array<Voice,128> voices{};
    std::array<int,16> sequence{},articulations{};
    const SampledGuitar* library=nullptr;
    std::uint64_t generation=0;
    std::array<double,2> last{},fade{};
    int fadeRemaining=0,stealRemaining=0;
    std::array<double,3> layers{};
    std::array<double,2> stealFade{};
    static double envelope(const GuitarZone& z,double age){if(z.attack>0&&age<z.attack)return age/z.attack;if(z.decay>0&&age<z.attack+z.decay)return 1-(1-z.sustain)*(age-z.attack)/z.decay;return z.sustain;}
    static double gain(const GuitarZone& z,int channel){return channel==0?(z.pan>0?1-z.pan:1):(z.pan<0?1+z.pan:1);}
    void start(int zone,int note,int channel,int velocity,double rate,int articulation=24,double scale=1){
        auto* voice=&voices[0];for(auto& candidate:voices)if(!candidate.active){voice=&candidate;break;}else if(candidate.age>voice->age)voice=&candidate;
        if(voice->active){const auto& old=library->zones[static_cast<std::size_t>(voice->zone)];int at=std::min(static_cast<int>(voice->position),old.audio->getNumSamples()-1);double level=voice->released?voice->releaseLevel*std::max(0.0,1-voice->releaseAge/old.release):envelope(old,voice->age);for(int c=0;c<2;++c)stealFade[static_cast<std::size_t>(c)]=stealFade[static_cast<std::size_t>(c)]*stealRemaining/128.0+old.audio->getSample(std::min(c,old.audio->getNumChannels()-1),at)*voice->gain*level*gain(old,c);stealRemaining=128;}
        const auto& z=library->zones[static_cast<std::size_t>(zone)];*voice={true,false,zone,note,channel,velocity,0,z.rate/rate*std::pow(2.0,(note-z.root+z.tune/100)/12),0,0,z.volume*velocity/127.0*scale,articulation};
    }
    void attack(int note,int channel,int velocity,double rate,int articulation){
        if(!library)return;
        if(articulations[static_cast<std::size_t>(channel-1)]>0)for(auto& voice:voices)if(voice.active&&voice.channel==channel){const auto& z=library->zones[static_cast<std::size_t>(voice.zone)];if(z.onRelease||z.articulation==39||z.articulation==40||z.articulation==43)continue;double duration=voice.releaseDuration>0?voice.releaseDuration:z.release;voice.releaseLevel=voice.released?voice.releaseLevel*std::max(0.0,1-voice.releaseAge/duration):envelope(z,voice.age);voice.released=true;voice.releaseAge=0;voice.releaseDuration=std::min(.008,z.release);}
        NoteEvent event;event.note=note;event.channel=channel;event.articulation=articulations[static_cast<std::size_t>(channel-1)];articulation=library->articulationFor(event,articulation,velocity);if(event.articulation>=32&&event.articulation!=41){bool supported=false;for(const auto& zone:library->zones)if(!zone.onRelease&&zone.articulation==event.articulation&&matches(zone,note,channel,velocity,event.articulation))supported=true;if(!supported){++unmapped;return;}}const int position=sequence[static_cast<std::size_t>(channel-1)]++;bool matched=false;
        for(std::size_t i=0;i<library->zones.size();++i){const auto& z=library->zones[i];if(!z.onRelease&&matches(z,note,channel,velocity,articulation)&&position%z.seqLength+1==z.seqPosition){start(static_cast<int>(i),note,channel,velocity,rate,articulation);matched=true;}}if(!matched)++unmapped;
        if(matched)for(std::size_t layer=0;layer<3;++layer){int id=layer==0?40:layer==1?39:43;if(layers[layer]<=0||id==articulation)continue;for(std::size_t i=0;i<library->zones.size();++i){const auto& z=library->zones[i];if(!z.onRelease&&z.articulation==id&&matches(z,note,channel,velocity,id)&&position%z.seqLength+1==z.seqPosition)start(static_cast<int>(i),note,channel,velocity,rate,id,layers[layer]);}}

    }
    static bool matches(const GuitarZone& z,int note,int channel,int velocity,int articulation){return note>=z.low&&note<=z.high&&velocity>=z.loVel&&velocity<=z.hiVel&&channel>=z.loChan&&channel<=z.hiChan&&(z.articulation<0||z.articulation==articulation);}
    void release(int note,int channel,double rate,int articulation){
        int velocity=0;for(auto& voice:voices)if(voice.active&&voice.note==note&&voice.channel==channel&&!voice.released&&!library->zones[static_cast<std::size_t>(voice.zone)].onRelease){voice.released=true;voice.releaseAge=0;voice.releaseDuration=library->zones[static_cast<std::size_t>(voice.zone)].release;voice.releaseLevel=envelope(library->zones[static_cast<std::size_t>(voice.zone)],voice.age);if(!velocity||voice.articulation<39||voice.articulation==41||voice.articulation==42){velocity=voice.velocity;articulation=voice.articulation;}}
        if(!velocity)return;for(std::size_t i=0;i<library->zones.size();++i){const auto& z=library->zones[i];if(z.onRelease&&matches(z,note,channel,velocity,articulation)&&(sequence[static_cast<std::size_t>(channel-1)]-1)%z.seqLength+1==z.seqPosition)start(static_cast<int>(i),note,channel,velocity,rate,articulation);}
    }
    void segment(juce::AudioBuffer<float>& output,int first,int end,double rate){
        std::array<double,2> ending{};
        for(auto& voice:voices){if(!voice.active)continue;const auto& z=library->zones[static_cast<std::size_t>(voice.zone)];
            for(int i=first;i<end;++i){if(voice.position>=z.audio->getNumSamples()-1||((voice.released||z.onRelease)&&voice.releaseAge>=(voice.releaseDuration>0?voice.releaseDuration:z.release))){voice.active=false;break;}
                double level=voice.released?voice.releaseLevel:envelope(z,voice.age);if(voice.released||z.onRelease)level*=std::max(0.0,1-voice.releaseAge/(voice.releaseDuration>0?voice.releaseDuration:z.release));
                int at=static_cast<int>(voice.position);double fraction=voice.position-at;
                for(int c=0;c<output.getNumChannels();++c){int ch=std::min(c,z.audio->getNumChannels()-1);double v=z.audio->getSample(ch,at)*(1-fraction)+z.audio->getSample(ch,at+1)*fraction;double sample=v*voice.gain*level*gain(z,c);output.addSample(c,i,static_cast<float>(sample));if(i==end-1&&c<2)ending[static_cast<std::size_t>(c)]+=sample;}
                voice.position+=voice.step;voice.age+=1/rate;if(voice.released||z.onRelease)voice.releaseAge+=1/rate;
            }
        }
        for(int i=first;i<end&&fadeRemaining>0;++i){for(int c=0;c<std::min(2,output.getNumChannels());++c){double value=fade[static_cast<std::size_t>(c)]*fadeRemaining/128.0;output.addSample(c,i,static_cast<float>(value));if(i==end-1)ending[static_cast<std::size_t>(c)]+=value;}--fadeRemaining;}
        for(int i=first;i<end&&stealRemaining>0;++i){for(int c=0;c<std::min(2,output.getNumChannels());++c){double value=stealFade[static_cast<std::size_t>(c)]*stealRemaining/128.0;output.addSample(c,i,static_cast<float>(value));if(i==end-1)ending[static_cast<std::size_t>(c)]+=value;}--stealRemaining;}
        if(end>first)last=ending;
    }
public:
    int unmapped=0;
    void reset(){fade=last;fadeRemaining=128;stealRemaining=0;last.fill(0);for(auto& voice:voices)voice.active=false;sequence.fill(0);articulations.fill(0);}
    void bind(const SampledGuitar* next,std::uint64_t revision){if(next!=library||revision!=generation){reset();unmapped=0;library=next;generation=revision;}}
    void renderTail(juce::AudioBuffer<float>& output,double rate){segment(output,0,output.getNumSamples(),rate);}
    int activeVoices()const{int n=0;for(const auto& v:voices)n+=v.active;return n;}
    void render(juce::AudioBuffer<float>& output,const juce::MidiBuffer& midi,double rate,int articulation,double curve,double pickNoise=0,double fretNoise=0,double resonance=0){
        layers={pickNoise,fretNoise,resonance};
        if(rate<=0)return;int position=0;
        for(const auto event:midi){const int at=std::clamp(event.samplePosition,position,output.getNumSamples());segment(output,position,at,rate);position=at;
            if(event.numBytes<3)continue;const auto* data=event.data;int kind=data[0]&0xf0,channel=(data[0]&15)+1,note=data[1]&127,velocity=data[2]&127;
            if(kind==0xb0&&note==20){articulations[static_cast<std::size_t>(channel-1)]=velocity;continue;}
            if(kind==0x90&&velocity>0)attack(note,channel,std::clamp(static_cast<int>(std::round(127*std::pow(velocity/127.0,curve))),1,127),rate,articulation);
            else if(kind==0x80||(kind==0x90&&velocity==0))release(note,channel,rate,articulation);
            else if(kind==0xb0&&(note==120||note==123))reset();
        }segment(output,position,output.getNumSamples(),rate);
    }
};
}
