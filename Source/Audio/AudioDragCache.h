#pragma once
#include "Track.h"
#include "Files/DragFileStore.h"
#include "Threading/BackgroundThread.h"
namespace wingman {
// Immutable clip exports prepared off the message and audio threads.
class AudioDragCache {
    struct Entry { std::shared_ptr<const Clip> clip; juce::File file; juce::String message; bool busy=false; };
    juce::CriticalSection mutex;
    std::array<Entry,5> entries;
    std::array<BackgroundThread,5> workers;
public:
    ~AudioDragCache(){for(auto& worker:workers)worker.request_stop();for(auto& worker:workers)worker.join();}
    void prepare(int track,std::shared_ptr<const Clip> clip){
        if(track<0||track>=5||!clip)return;
        auto n=static_cast<std::size_t>(track);
        {const juce::ScopedLock lock(mutex);auto& e=entries[n];if(e.busy||(e.clip==clip&&(e.file!=juce::File{}||e.message.isNotEmpty())))return;e={clip,{},"Preparing WAV for drag; please wait",true};}
        workers[n].start([this,n,clip](CancellationToken token){
            juce::File directory,file;juce::String error;
            try{
                auto result=DragFileStore::createDirectory(directory);if(result.failed())throw std::runtime_error(result.getErrorMessage().toStdString());
                file=directory.getChildFile("Jaswingman-track.wav");juce::WavAudioFormat wav;
                std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(file),48000,2,24,{},0));
                if(!writer)throw std::runtime_error("Cannot write drag WAV; use Export WAV");
                juce::AudioBuffer<float> block(2,8192);
                const auto total=static_cast<juce::int64>(std::ceil(clip->audio.getNumSamples()*48000.0/clip->sampleRate));
                for(juce::int64 pos=0;pos<total;pos+=8192){
                    if(token.stop_requested())throw std::runtime_error("WAV preparation cancelled");
                    const auto count=static_cast<int>(std::min<juce::int64>(8192,total-pos));
                    for(int ch=0;ch<2;++ch)for(int s=0;s<count;++s)block.setSample(ch,s,juce::jlimit(-1.0f,1.0f,interpolate(*clip,ch,(pos+s)*clip->sampleRate/48000.0)));
                    if(!writer->writeFromAudioSampleBuffer(block,0,count))throw std::runtime_error("WAV drag write failed; check disk space");
                }
                writer.reset();
            }catch(const std::exception& ex){error=ex.what();directory.deleteRecursively();file=juce::File{};}
            const juce::ScopedLock lock(mutex);entries[n]={clip,file,error,false};
        });
    }
    juce::Result get(int track,const std::shared_ptr<const Clip>& clip,juce::File& file){
        if(!clip)return juce::Result::fail("Import, record or render audio first");
        const juce::ScopedLock lock(mutex);auto& e=entries[static_cast<std::size_t>(track)];
        if(e.clip!=clip||e.busy)return juce::Result::fail("Preparing WAV for drag; wait a moment and drag again");
        if(!e.file.existsAsFile()){
            if(e.message.isNotEmpty())return juce::Result::fail(e.message);
            e.clip.reset();return juce::Result::fail("WAV cache expired; preparing again. Retry the drag or use Export WAV");
        }
        file=e.file;return juce::Result::ok();
    }
};
}
