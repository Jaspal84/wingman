#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
namespace wingman::studio {
inline constexpr std::array<const char*, 11> effectNames{"Gator", "Looper", "Utility", "Chorus", "Bitcrush", "EQ low-pass", "Compression", "Delay", "Reverb", "Saturation", "Distortion"};
struct Effects {
    std::array<bool, 11> enabled{};
    std::array<float, 11> amount{.5f, .5f, .5f, .3f, .3f, .8f, .5f, .3f, .3f, .3f, .3f};
};
class EffectsProcessor {
    double rate = 48000, phase = 0;
    std::array<std::vector<float>, 2> history;
    std::size_t head = 0;
    juce::dsp::StateVariableTPTFilter<float> filter;
    juce::dsp::Compressor<float> compressor;
    juce::Reverb reverb;
public:
    EffectsProcessor() { prepare(48000); }
    void prepare(double sampleRate) {
        rate = sampleRate; phase = 0; head = 0; for (auto& channel : history) channel.assign(static_cast<std::size_t>(rate * 2.1) + 4, 0);
        juce::dsp::ProcessSpec spec{rate, 1, 2}; filter.prepare(spec); filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass); compressor.prepare(spec); reverb.setSampleRate(rate); reset();
    }
    void reset() { for (auto& c : history) std::fill(c.begin(), c.end(), 0); head = 0; phase = 0; filter.reset(); compressor.reset(); reverb.reset(); }
    void configure(const Effects& p) {
        filter.setCutoffFrequency(static_cast<float>(std::min(rate * .45, 40 * std::pow(400.0, p.amount[5]))));
        compressor.setThreshold(-36 * p.amount[6]); compressor.setRatio(1 + 11 * p.amount[6]); compressor.setAttack(10); compressor.setRelease(100);
        juce::Reverb::Parameters r; r.roomSize = .25f + .65f * p.amount[8]; r.damping = .5f; r.wetLevel = p.enabled[8] ? p.amount[8] * .5f : 0; r.dryLevel = 1; r.width = 1; reverb.setParameters(r);
    }
    void process(float& left, float& right, const Effects& p, double beat) {
        float values[]{left, right};
        for (int c = 0; c < 2; ++c) {
            float v = values[c];
            if (p.enabled[0] && static_cast<int64_t>(std::floor(beat * 4)) % 2) v *= 1 - p.amount[0];
            if (p.enabled[2]) v *= std::pow(4.0f, (p.amount[2] - .5f) * 2);
            if (p.enabled[5]) v = filter.processSample(c, v);
            if (p.enabled[6]) v = compressor.processSample(c, v);
            if (p.enabled[9]) { float drive = 1 + 12 * p.amount[9]; v = std::tanh(v * drive) / std::tanh(drive); }
            if (p.enabled[10]) v = std::clamp(v * (1 + 15 * p.amount[10]), -1.f, 1.f);
            if (p.enabled[4]) { float levels = std::pow(2.f, 16 - 13 * p.amount[4]); v = std::round(v * levels) / levels; }
            auto tap = [&](double seconds) { auto delay = static_cast<std::size_t>(std::clamp(seconds * rate, 1.0, static_cast<double>(history[c].size() - 1))); return history[c][(head + history[c].size() - delay) % history[c].size()]; };
            float chorus = p.enabled[3] ? tap(.015 + .008 * (1 + std::sin(phase + c * 1.3))) * p.amount[3] * .5f : 0;
            float delay = p.enabled[7] ? tap(.375) * p.amount[7] * .65f : 0;
            float loop = p.enabled[1] ? tap(.25 + .75 * p.amount[1]) * .7f : 0;
            history[c][head] = std::clamp(v + delay * .55f + loop * .5f, -2.f, 2.f);
            values[c] = v + chorus + delay + loop;
        }
        if (p.enabled[8]) reverb.processStereo(&values[0], &values[1], 1);
        left = values[0]; right = values[1]; head = (head + 1) % history[0].size(); phase += 2 * juce::MathConstants<double>::pi * .6 / rate;
        if (phase > 2 * juce::MathConstants<double>::pi) phase -= 2 * juce::MathConstants<double>::pi;
    }
};
}
