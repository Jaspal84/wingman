#pragma once
#include "Track.h"
#include "SampleVoices.h"
#include "PreviewSynth.h"
#include "MIDI/RealtimeSchedule.h"
#include "Host/Telemetry.h"
#include <atomic>
#include <array>
#include <optional>
#include <algorithm>
#include <vector>
#include <limits>
namespace wingman {
// One audio reader, serialized non-audio writers. Reclamation occurs only on writers.
class PlaybackEngine {
    struct Snapshot {std::array<Track,5> tracks;bool loop=false;double length=0;std::shared_ptr<const SampledGuitar> library;bool sampledPreview=false;double curve=1;int articulation=24;std::uint64_t libraryRevision=0;bool generated=false;juce::MidiMessageSequence sequence;double sequenceLength=0,tempo=120;std::uint64_t midiRevision=0;bool basicPreview=false;};
    SampleVoices sampleVoices;
    RealtimeSchedule midiSchedule;
    std::optional<HostFrame> frame;
    bool synchronized=false,seeked=false;
    static_assert(std::atomic<const Snapshot*>::is_always_lock_free);
    static_assert(std::atomic<double>::is_always_lock_free);
    std::atomic<const Snapshot*> current{nullptr},hazard{nullptr};
    std::vector<std::unique_ptr<Snapshot>> retained;
    std::atomic<double> requested{0},visible{0};
    double cursor=0,hostLoopStart=0,hostLoopEnd=0;
    bool hostMode=false;
public:
    HostTelemetry telemetry;
    std::atomic<int> unmappedSamples{0};
    std::atomic<bool> resetSampleNotes{false};
    std::atomic<bool> playing{false};
    PlaybackEngine(){publish({},false);}
    void publish(const std::array<Track,5>& tracks,bool loop,std::shared_ptr<const SampledGuitar> library={},bool sampledPreview=false,double curve=1,int articulation=24,std::uint64_t revision=0,bool generated=false,const juce::MidiMessageSequence& sequence={},double sequenceLength=0,double tempo=120,std::uint64_t midiRevision=0,bool basicPreview=false){
        auto next=std::make_unique<Snapshot>();next->tracks=tracks;next->loop=loop;next->library=std::move(library);next->sampledPreview=sampledPreview;next->curve=curve;next->articulation=articulation;next->libraryRevision=revision;next->generated=generated;next->sequence=sequence;next->sequenceLength=sequenceLength;next->tempo=tempo;next->midiRevision=midiRevision;next->basicPreview=basicPreview;
        for(const auto& t:tracks)if(t.clip)next->length=std::max(next->length,t.clip->audio.getNumSamples()/t.clip->sampleRate);
        auto* pointer=next.get();retained.push_back(std::move(next));current.store(pointer);
        const auto* active=hazard.load();
        std::erase_if(retained,[&](const auto& p){return p.get()!=pointer&&p.get()!=active;});
    }
    void bypass(juce::MidiBuffer& midi){midiSchedule.flush(midi);resetSampleNotes=true;}
    void seek(double seconds){requested.store(seconds);visible.store(seconds);}
    double position()const{return visible.load();}
    // Called once per audio block; optional host seconds are authoritative.
    void begin(std::optional<double> hostSeconds={},std::optional<bool> hostPlaying={},double loopStart=0,double loopEnd=0,std::optional<HostFrame> hostFrame={},bool sync=false){
        frame=hostFrame;synchronized=sync;telemetry.publish(sync?hostFrame:std::nullopt);
        hostMode=hostSeconds.has_value();hostLoopStart=loopStart;hostLoopEnd=loopEnd;
        const auto request=requested.exchange(std::numeric_limits<double>::quiet_NaN());
        if(std::isfinite(request)){seeked=true;cursor=request;}
        if(hostSeconds&&std::isfinite(*hostSeconds))cursor=*hostSeconds;
        if(hostPlaying)playing.store(*hostPlaying);
        visible.store(cursor);
    }
    void render(juce::AudioBuffer<float>& output,double rate,juce::MidiBuffer* midi=nullptr,PreviewSynth* synth=nullptr){
        const Snapshot* snapshot;
        do{snapshot=current.load();hazard.store(snapshot);}while(current.load()!=snapshot);
        bool reset=resetSampleNotes.exchange(false);if(midi)reset=midiSchedule.process(snapshot->sequence,snapshot->sequenceLength,snapshot->generated,snapshot->tempo,rate,cursor,playing.load(),!hostMode&&snapshot->loop?snapshot->length:0,synchronized,frame,snapshot->midiRevision,reset||seeked,output.getNumSamples(),*midi)||reset;seeked=false;
        sampleVoices.bind(snapshot->library.get(),snapshot->libraryRevision);
        if(reset){sampleVoices.reset();if(synth)synth->allNotesOff();}
        if(snapshot->sampledPreview&&midi)sampleVoices.render(output,*midi,rate,snapshot->articulation,snapshot->curve);else{sampleVoices.reset();sampleVoices.renderTail(output,rate);}unmappedSamples=sampleVoices.unmapped;if(snapshot->basicPreview&&midi&&synth)synth->renderNextBlock(output,*midi,0,output.getNumSamples());
        if(playing.load()&&rate>0){bool solo=false;for(const auto& t:snapshot->tracks)solo|=t.solo;
            for(const auto& t:snapshot->tracks){if(!t.clip||t.mute||(solo&&!t.solo))continue;
                for(int i=0;i<output.getNumSamples();++i){double at=cursor+i/rate;if(hostMode&&hostLoopEnd>hostLoopStart&&at>=hostLoopEnd)at=hostLoopStart+std::fmod(at-hostLoopStart,hostLoopEnd-hostLoopStart);else if(!hostMode&&snapshot->loop&&snapshot->length>0)at=std::fmod(at,snapshot->length);
                    for(int c=0;c<output.getNumChannels();++c)output.addSample(c,i,interpolate(*t.clip,c,at*t.clip->sampleRate)*t.gain*balanceGain(t.pan,c));}
            }
            cursor+=output.getNumSamples()/rate;if(hostMode&&hostLoopEnd>hostLoopStart&&cursor>=hostLoopEnd)cursor=hostLoopStart+std::fmod(cursor-hostLoopStart,hostLoopEnd-hostLoopStart);else if(!hostMode&&snapshot->loop&&snapshot->length>0)cursor=std::fmod(cursor,snapshot->length);if(!hostMode&&!snapshot->loop&&!snapshot->generated&&snapshot->length>0&&cursor>=snapshot->length){cursor=snapshot->length;playing=false;}visible.store(cursor);
        }for(int c=0;c<output.getNumChannels();++c)for(int i=0;i<output.getNumSamples();++i){auto v=output.getSample(c,i);output.setSample(c,i,std::isfinite(v)?std::clamp(v,-1.0f,1.0f):0);}
        hazard.store(nullptr);
    }
};
}
