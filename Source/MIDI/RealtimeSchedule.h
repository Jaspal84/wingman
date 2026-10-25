#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include "Host/Transport.h"
namespace wingman {
class RealtimeSchedule {
    std::array<std::array<bool,128>,16> owned{};
    HostClock clock;
    bool syncWas=false,playingWas=false,recover=false;
    std::uint64_t revision=0;
    void chase(const juce::MidiMessageSequence& sequence,double length,double beat,juce::MidiBuffer& output,int offset){
        if(beat<0||length<=0)return;double phase=std::fmod(beat,length);
        for(int i=0;i<sequence.getNumEvents();++i){auto* event=sequence.getEventPointer(i);if(event->message.isNoteOn()&&event->noteOffObject&&event->message.getTimeStamp()<phase-1e-8&&event->noteOffObject->message.getTimeStamp()>phase){
            const int channel=event->message.getChannel();for(int j=i-1;j>=0;--j){auto& prior=sequence.getEventPointer(j)->message;if(prior.isControllerOfType(20)&&prior.getChannel()==channel){output.addEvent(prior,offset);break;}}
            output.addEvent(event->message,offset);owned[static_cast<std::size_t>(channel-1)][static_cast<std::size_t>(event->message.getNoteNumber())]=true;
        }}
    }
    void span(const juce::MidiMessageSequence& sequence,double length,double tempo,double rate,double beat,int offset,int count,juce::MidiBuffer& output){
        if(length<=0||count<=0)return;const double end=beat+count/rate*tempo/60;int emitted=0;
        auto first=static_cast<std::int64_t>(std::floor(beat/length)),last=static_cast<std::int64_t>(std::floor(end/length));
        for(auto cycle=first;cycle<=last&&emitted<16384;++cycle)for(int i=0;i<sequence.getNumEvents()&&emitted<16384;++i){const auto& message=sequence.getEventPointer(i)->message;double at=cycle*length+message.getTimeStamp();if(at<0||at<beat||at>=end)continue;int sample=offset+std::clamp(static_cast<int>(std::llround((at-beat)*60/tempo*rate)),0,count-1);
            if(message.isNoteOff()&&!owned[static_cast<std::size_t>(message.getChannel()-1)][static_cast<std::size_t>(message.getNoteNumber())])continue;output.addEvent(message,sample);++emitted;if(message.isNoteOnOrOff())owned[static_cast<std::size_t>(message.getChannel()-1)][static_cast<std::size_t>(message.getNoteNumber())]=message.isNoteOn();
        }if(emitted>=16384){flush(output,offset+count-1);recover=true;}
    }
public:
    void flush(juce::MidiBuffer& output,int offset=0){for(int c=0;c<16;++c)for(int note=0;note<128;++note)if(owned[static_cast<std::size_t>(c)][static_cast<std::size_t>(note)]){output.addEvent(juce::MidiMessage::noteOff(c+1,note),offset);owned[static_cast<std::size_t>(c)][static_cast<std::size_t>(note)]=false;}}
    bool process(const juce::MidiMessageSequence& sequence,double length,bool generated,double tempo,double rate,double seconds,bool playing,double loopSeconds,bool synced,const std::optional<HostFrame>& frame,std::uint64_t version,bool requested,int count,juce::MidiBuffer& output){
        if(syncWas!=synced)clock.reset();TransportBlock host;if(synced){host=clock.resolve(frame,count,rate);playing=host.playing;if(host.valid)tempo=host.frame.bpm;}
        bool reset=requested||recover||revision!=version||syncWas!=synced||playingWas!=playing||(synced&&host.discontinuity);recover=false;revision=version;syncWas=synced;playingWas=playing;
        double beat=synced&&host.valid?host.frame.ppq:seconds*tempo/60;if(reset)flush(output);if(!playing||!generated||length<=0)return reset;if(reset)chase(sequence,length,beat,output,0);
        if(synced){for(int i=0;i<host.spanCount;++i){const auto& part=host.spans[static_cast<std::size_t>(i)];if(part.restart){flush(output,part.offset);chase(sequence,length,part.beat,output,part.offset);}span(sequence,length,tempo,rate,part.beat,part.offset,part.count,output);}}
        else if(loopSeconds>0){int offset=0;double at=seconds;int loops=0;while(offset<count&&loops++<128){int n=std::min(count-offset,std::max(1,static_cast<int>(std::ceil((loopSeconds-at)*rate))));span(sequence,length,tempo,rate,at*tempo/60,offset,n,output);offset+=n;at+=n/rate;if(at>=loopSeconds){at=std::fmod(at,loopSeconds);if(offset<count){flush(output,offset);chase(sequence,length,at*tempo/60,output,offset);}}}if(offset<count)flush(output,count-1);}
        else span(sequence,length,tempo,rate,beat,0,count,output);return reset;
    }
};
}
