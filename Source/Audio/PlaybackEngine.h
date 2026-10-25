#pragma once
#include "Track.h"
#include <atomic>
#include <array>
#include <optional>
#include <algorithm>
#include <vector>
#include <limits>
namespace wingman {
// One audio reader, serialized non-audio writers. Reclamation occurs only on writers.
class PlaybackEngine {
    struct Snapshot {std::array<Track,5> tracks;bool loop=false;double length=0;};
    std::atomic<const Snapshot*> current{nullptr},hazard{nullptr};
    std::vector<std::unique_ptr<Snapshot>> retained;
    std::atomic<double> requested{0},visible{0};
    double cursor=0;
public:
    std::atomic<bool> playing{false};
    PlaybackEngine(){publish({},false);}
    void publish(const std::array<Track,5>& tracks,bool loop){
        auto next=std::make_unique<Snapshot>();next->tracks=tracks;next->loop=loop;
        for(const auto& t:tracks)if(t.clip)next->length=std::max(next->length,t.clip->audio.getNumSamples()/t.clip->sampleRate);
        auto* pointer=next.get();retained.push_back(std::move(next));current.store(pointer);
        const auto* active=hazard.load();
        std::erase_if(retained,[&](const auto& p){return p.get()!=pointer&&p.get()!=active;});
    }
    void seek(double seconds){requested.store(seconds);visible.store(seconds);}
    double position()const{return visible.load();}
    // Called once per audio block; optional host seconds are authoritative.
    void begin(std::optional<double> hostSeconds={},std::optional<bool> hostPlaying={}){
        const auto request=requested.exchange(std::numeric_limits<double>::quiet_NaN());
        if(std::isfinite(request))cursor=request;
        if(hostSeconds&&std::isfinite(*hostSeconds))cursor=*hostSeconds;
        if(hostPlaying)playing.store(*hostPlaying);
        visible.store(cursor);
    }
    void render(juce::AudioBuffer<float>& output,double rate){
        const Snapshot* snapshot;
        do{snapshot=current.load();hazard.store(snapshot);}while(current.load()!=snapshot);
        if(playing.load()&&rate>0){bool solo=false;for(const auto& t:snapshot->tracks)solo|=t.solo;
            for(const auto& t:snapshot->tracks){if(!t.clip||t.mute||(solo&&!t.solo))continue;
                for(int i=0;i<output.getNumSamples();++i){double at=cursor+i/rate;if(snapshot->loop&&snapshot->length>0)at=std::fmod(at,snapshot->length);
                    for(int c=0;c<output.getNumChannels();++c)output.addSample(c,i,interpolate(*t.clip,c,at*t.clip->sampleRate)*t.gain*balanceGain(t.pan,c));}
            }
            cursor+=output.getNumSamples()/rate;if(snapshot->loop&&snapshot->length>0)cursor=std::fmod(cursor,snapshot->length);visible.store(cursor);
        }hazard.store(nullptr);
    }
};
}
