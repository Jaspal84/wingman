#pragma once
#include "ModelTypes.h"
#include "WingmanWorkerData.h"
#include "Threading/BackgroundThread.h"
#include <atomic>
#include <filesystem>
#include <juce_audio_formats/juce_audio_formats.h>
namespace wingman {
class ModelJob {
    mutable juce::CriticalSection statusMutex;
    ModelJobInfo info;
    std::atomic<bool> busy{false};
    BackgroundThread worker;
    void status(double progress,const juce::String& message) { const juce::ScopedLock lock(statusMutex);info.progress=progress;info.message=message; }
public:
    ~ModelJob(){cancel();if(worker.joinable())worker.join();}
    void cancel(){worker.request_stop();}
    ModelJobInfo snapshot() const {const juce::ScopedLock lock(statusMutex);auto copy=info;copy.running=busy;return copy;}
    juce::Result start(const juce::File& python,const juce::File& models,const juce::String& action,const juce::File& input,
                       std::function<juce::Result(const juce::File&)> onStems={}) {
        if(!python.existsAsFile())return juce::Result::fail("Select the Python executable from the pinned inference runtime");
        if(action!="check"&&action!="install"&&action!="separate")return juce::Result::fail("Invalid model action");
        if(action=="separate"&&!input.existsAsFile())return juce::Result::fail("Choose a WAV mix to separate");
        if(busy.exchange(true))return juce::Result::fail("A model operation is already running");
        if(worker.joinable())worker.join();
        status(0,"Starting "+action+"...");
        worker.start([this,python,models,action,input,onStems](CancellationToken token){
            auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("wingman-job-"+juce::Uuid().toString());
            struct Cleanup {juce::File path,staging;bool keep=false;~Cleanup(){if(!keep){path.deleteRecursively();if(staging!=juce::File{})staging.deleteRecursively();}}} cleanup{directory,{}};
            try {
                if(directory.createDirectory().failed())throw std::runtime_error("Cannot create private worker directory");
                std::filesystem::permissions(directory.getFullPathName().toStdString(),std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
                int size=0;const auto* code=WingmanWorkerData::getNamedResource("stem_worker_py",size);
                const auto script=directory.getChildFile("worker.py"),statusFile=directory.getChildFile("status.json"),output=directory.getChildFile("output");
                if(!code||!script.replaceWithData(code,static_cast<std::size_t>(size)))throw std::runtime_error("Cannot stage embedded worker");
                juce::StringArray arguments{python.getFullPathName(),script.getFullPathName(),action,"--models",models.getFullPathName(),"--status",statusFile.getFullPathName()};
                if(action=="install"){
                    cleanup.staging=models.getChildFile(".wingman-install-"+juce::Uuid().toString());
                    if(cleanup.staging.createDirectory().failed())throw std::runtime_error("Cannot create model installation staging directory");
                    arguments.addArray({"--allow-download","--accept-mit-license","--staging",cleanup.staging.getFullPathName()});
                }
                if(action=="separate"){
                    status(0,"Decoding input audio...");
                    juce::AudioFormatManager formats; formats.registerBasicFormats();
                    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(input));
                    if(!reader || reader->numChannels<1 || reader->numChannels>2 || reader->sampleRate<8000 || reader->sampleRate>192000 || reader->lengthInSamples<=0 || reader->lengthInSamples>reader->sampleRate*120)
                        throw std::runtime_error("Choose readable mono/stereo audio, 8-192 kHz, up to 120 seconds");
                    const auto decoded=directory.getChildFile("input.wav");
                    juce::WavAudioFormat wav;
                    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(decoded),reader->sampleRate,reader->numChannels,24,{},0));
                    if(!writer)throw std::runtime_error("Cannot create decoded input WAV");
                    juce::AudioBuffer<float> block(static_cast<int>(reader->numChannels),8192);
                    for(juce::int64 pos=0;pos<reader->lengthInSamples;pos+=8192){
                        if(token.stop_requested())throw std::runtime_error("Model operation cancelled");
                        const auto count=static_cast<int>(std::min<juce::int64>(8192,reader->lengthInSamples-pos));
                        if(!reader->read(&block,0,count,pos,true,true))throw std::runtime_error("Cannot decode input audio");
                        for(int ch=0;ch<block.getNumChannels();++ch)for(int i=0;i<count;++i)
                            if(!std::isfinite(block.getSample(ch,i)))throw std::runtime_error("Nonfinite input audio");
                        if(!writer->writeFromAudioSampleBuffer(block,0,count))throw std::runtime_error("Cannot write decoded input WAV");
                    }
                    writer.reset();
                    arguments.addArray({"--input",decoded.getFullPathName(),"--output",output.getFullPathName(),"--cancel",directory.getChildFile("cancel.flag").getFullPathName()});}
                juce::ChildProcess child;
                if(!child.start(arguments,0))throw std::runtime_error("Could not start inference Python process");
                while(child.isRunning()){
                    if(token.stop_requested()){
                        directory.getChildFile("cancel.flag").replaceWithText("cancel");
                        child.kill();
                        if(!child.waitForProcessToFinish(5000)){cleanup.keep=true;throw std::runtime_error("Worker did not stop; retained its temporary directory: "+directory.getFullPathName().toStdString());}
                        throw std::runtime_error("Model operation cancelled");
                    }
                    if(statusFile.existsAsFile()){
                        auto update=juce::JSON::parse(statusFile.loadFileAsString());
                        status(static_cast<double>(update.getProperty("progress",0)),update.getProperty("message",update.getProperty("state","Working")).toString());
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                auto result=juce::JSON::parse(statusFile.loadFileAsString());
                if(child.getExitCode()!=0||result.getProperty("state","").toString()!="done")throw std::runtime_error(result.getProperty("message","Worker failed; check runtime dependencies and model installation").toString().toStdString());
                if(token.stop_requested())throw std::runtime_error("Model operation cancelled");
                if(action=="separate"){
                    const auto frames=static_cast<juce::int64>(result.getProperty("frames",0));
                    for(const auto& name:juce::StringArray{"vocals.wav","drums.wav","bass.wav","other.wav"}){
                        juce::WavAudioFormat wav;
                        auto stream=output.getChildFile(name).createInputStream();
                        if(!stream)throw std::runtime_error("Missing worker WAV output");
                        std::unique_ptr<juce::AudioFormatReader> stem(wav.createReaderFor(stream.release(),true));
                        if(!stem || stem->numChannels!=2 || stem->sampleRate!=44100 || stem->bitsPerSample!=24 || stem->lengthInSamples!=frames || frames<=0)
                            throw std::runtime_error("Invalid, missing or misaligned worker WAV output");
                    }
                }
                if(action=="separate"&&onStems){auto imported=onStems(output);if(imported.failed())throw std::runtime_error(imported.getErrorMessage().toStdString());}
                if(action=="separate") {
                    // Keep raw validated files beyond drag completion: hosts may import lazily.
                    const auto retained=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("Wingman-DragFiles").getChildFile(juce::Uuid().toString());
                    if(retained.getParentDirectory().createDirectory().failed() || !output.moveFileTo(retained))
                        throw std::runtime_error("Stems imported but cannot retain drag files; use Export WAV");
                    std::filesystem::permissions(retained.getFullPathName().toStdString(),std::filesystem::perms::owner_all,std::filesystem::perm_options::replace);
                    {const juce::ScopedLock lock(statusMutex);info.outputDirectory=retained;}
                    const auto reports=models.getChildFile("reports");
                    if(reports.createDirectory().wasOk())reports.getChildFile("separation-"+juce::Uuid().toString()+".json").replaceWithText(juce::JSON::toString(result));
                }
                status(1,action=="separate"?"Four stems imported | "+juce::String(static_cast<double>(result.getProperty("seconds",0)),1)+" s | "+juce::String(static_cast<double>(result.getProperty("peak_rss_bytes",0))/(1024*1024),0)+" MiB peak":action=="install"?"Model installed and checksum recorded":"Local model checksum verified");
            }catch(const std::exception& error){status(0,juce::String::fromUTF8(error.what()));}
            busy=false;
        });
        return juce::Result::ok();
    }
};
}
