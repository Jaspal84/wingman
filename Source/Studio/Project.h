#pragma once
#include "Audio/PlaybackEngine.h"
namespace wingman::studio {
struct Project {
    std::array<Track,8> tracks;
    std::array<juce::File,8> sources;
    bool loop=false;
};
inline juce::Result decode(const juce::File& file,std::shared_ptr<const Clip>& result){
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader)return juce::Result::fail("Missing or unsupported audio: "+file.getFullPathName());
    if(reader->numChannels<1||reader->numChannels>2||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<1||reader->lengthInSamples>reader->sampleRate*30)
        return juce::Result::fail("Audio requires mono/stereo, 8-192 kHz, maximum 30 seconds");
    auto clip=std::make_shared<Clip>();clip->sampleRate=reader->sampleRate;clip->name=file.getFileName();
    clip->audio.setSize(static_cast<int>(reader->numChannels),static_cast<int>(reader->lengthInSamples));
    if(!reader->read(&clip->audio,0,clip->audio.getNumSamples(),0,true,true))return juce::Result::fail("Could not decode audio");
    for(int c=0;c<clip->audio.getNumChannels();++c)for(int n=0;n<clip->audio.getNumSamples();++n)if(!std::isfinite(clip->audio.getSample(c,n)))return juce::Result::fail("Non-finite audio rejected");
    clip->buildWaveform();result=clip;return juce::Result::ok();
}
// Project files reference user assets; they never copy, delete or modify those assets.
inline juce::Result save(const juce::File& file,const Project& project){
    juce::ValueTree root("JasProProject");root.setProperty("version",1,nullptr);root.setProperty("loop",project.loop,nullptr);
    for(std::size_t i=0;i<8;++i){juce::ValueTree lane("Track");lane.setProperty("source",project.sources[i].getFullPathName(),nullptr);lane.setProperty("gain",project.tracks[i].gain,nullptr);lane.setProperty("pan",project.tracks[i].pan,nullptr);lane.setProperty("mute",project.tracks[i].mute,nullptr);lane.setProperty("solo",project.tracks[i].solo,nullptr);root.addChild(lane,-1,nullptr);}
    juce::TemporaryFile temporary(file);if(!temporary.getFile().replaceWithText(root.toXmlString())||!temporary.overwriteTargetFileWithTemporary())return juce::Result::fail("Project could not be saved");return juce::Result::ok();
}
template<class Cancel> juce::Result load(const juce::File& file,Project& destination,Cancel cancelled){
    if(!file.existsAsFile()||file.getSize()>1024*1024)return juce::Result::fail("Missing or oversized project");
    auto root=juce::ValueTree::fromXml(file.loadFileAsString());
    if(!root.hasType("JasProProject")||static_cast<int>(root["version"])!=1||root.getNumChildren()!=8)return juce::Result::fail("Invalid or unsupported Jas PRO project");
    Project next;next.loop=static_cast<bool>(root["loop"]);
    for(int i=0;i<8;++i){if(cancelled())return juce::Result::fail("Project loading cancelled");auto lane=root.getChild(i);if(!lane.hasType("Track"))return juce::Result::fail("Invalid track");auto& track=next.tracks[static_cast<std::size_t>(i)];
        track.gain=static_cast<float>(lane["gain"]);track.pan=static_cast<float>(lane["pan"]);track.mute=static_cast<bool>(lane["mute"]);track.solo=static_cast<bool>(lane["solo"]);
        if(!std::isfinite(track.gain)||track.gain<0||track.gain>1||!std::isfinite(track.pan)||std::abs(track.pan)>1)return juce::Result::fail("Invalid mixer settings");
        auto source=lane["source"].toString();if(source.isNotEmpty()){if(!juce::File::isAbsolutePath(source))return juce::Result::fail("Project source must be an absolute path");auto& path=next.sources[static_cast<std::size_t>(i)];path=juce::File(source);auto result=decode(path,track.clip);if(result.failed())return result;}
    }
    if(cancelled())return juce::Result::fail("Project loading cancelled");destination=std::move(next);return juce::Result::ok();
}
template<class Cancel> juce::Result renderMix(const juce::File& destination,const std::array<Track,8>& tracks,Cancel cancelled){
    ArrangementPlaybackEngine engine;engine.publish(tracks,false);engine.playing=true;
    double length=0;for(const auto& t:tracks)if(t.clip)length=std::max(length,t.clip->audio.getNumSamples()/t.clip->sampleRate);
    if(length<=0)return juce::Result::fail("Import audio before exporting");
    juce::TemporaryFile staging(destination);juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer;
    if(auto stream=staging.getFile().createOutputStream()){writer.reset(wav.createWriterFor(stream.get(),48000,2,24,{},0));if(writer)stream.release();}
    if(!writer)return juce::Result::fail("Cannot create WAV output");
    juce::AudioBuffer<float> block(2,512);const auto total=static_cast<int64_t>(std::ceil(length*48000));
    for(int64_t at=0;at<total;at+=512){if(cancelled())return juce::Result::fail("Export cancelled");block.clear();engine.begin();engine.render(block,48000);if(!writer->writeFromAudioSampleBuffer(block,0,static_cast<int>(std::min<int64_t>(512,total-at))))return juce::Result::fail("WAV write failed");}
    writer.reset();if(cancelled())return juce::Result::fail("Export cancelled");
    return staging.overwriteTargetFileWithTemporary()?juce::Result::ok():juce::Result::fail("Cannot publish rendered WAV");
}

}
