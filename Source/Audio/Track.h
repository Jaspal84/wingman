#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <memory>
#include <cmath>
namespace wingman {
struct Clip {
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100;
    juce::String name;
};
struct Track {
    std::shared_ptr<const Clip> clip;
    bool mute = false, solo = false;
    float gain = 0.8f;
};
inline float interpolate(const Clip& clip, int channel, double position) {
    if (!std::isfinite(position) || position < 0 || position >= clip.audio.getNumSamples()) return 0;
    const int i = static_cast<int>(position);
    const auto* data = clip.audio.getReadPointer(std::min(channel, clip.audio.getNumChannels()-1));
    const auto next = std::min(i+1, clip.audio.getNumSamples()-1);
    return data[i] + static_cast<float>(position-i) * (data[next]-data[i]);
}
}
