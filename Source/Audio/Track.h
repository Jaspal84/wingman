#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <cmath>
namespace wingman {
struct Clip {
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100;
    juce::String name;
    static constexpr int peakHop=256;
    std::vector<float> peaks;
    void buildWaveform(){
        peaks.assign(static_cast<std::size_t>((audio.getNumSamples()+peakHop-1)/peakHop),0);
        if(audio.getNumChannels()==0)return;
        for(int i=0;i<audio.getNumSamples();++i){auto& peak=peaks[static_cast<std::size_t>(i/peakHop)];for(int ch=0;ch<audio.getNumChannels();++ch)peak=std::max(peak,std::abs(audio.getSample(ch,i)));}
    }
};
struct Track {
    std::shared_ptr<const Clip> clip;
    bool mute = false, solo = false;
    float gain = 0.8f, pan = 0.0f;
};
inline float balanceGain(float pan, int channel) { return channel == 0 ? (pan > 0 ? 1-pan : 1) : (pan < 0 ? 1+pan : 1); }
inline float interpolate(const Clip& clip, int channel, double position) {
    if (!std::isfinite(position) || position < 0 || position >= clip.audio.getNumSamples()) return 0;
    const int i = static_cast<int>(position);
    const auto* data = clip.audio.getReadPointer(std::min(channel, clip.audio.getNumChannels()-1));
    const auto next = std::min(i+1, clip.audio.getNumSamples()-1);
    return data[i] + static_cast<float>(position-i) * (data[next]-data[i]);
}
}
