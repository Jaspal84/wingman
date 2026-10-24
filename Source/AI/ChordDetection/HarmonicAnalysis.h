#pragma once
#include "DSP/Music.h"
#include <complex>
#include <numeric>
#include <functional>
#include <stdexcept>

namespace wingman {
struct ChordRegion {
    double start = 0, end = 0, confidence = 0;
    Chord chord;
};
struct HarmonicResult {
    int keyRoot = 0;
    bool minor = false;
    double keyConfidence = 0, tuningCents = 0;
    std::vector<ChordRegion> regions;
};
namespace harmonic {
constexpr int fftSize = 8192;
inline void fft(std::vector<std::complex<double>>& data) {
    const auto n = data.size();
    for (std::size_t i = 1,j = 0; i < n; ++i) {
        auto bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(data[i],data[j]);
    }
    for (std::size_t length = 2; length <= n; length <<= 1) {
        const std::complex<double> step = std::polar(1.0,-2.0*3.141592653589793/length);
        for (std::size_t start = 0; start < n; start += length) {
            std::complex<double> rotation{1,0};
            for (std::size_t j = 0; j < length/2; ++j) {
                const auto a = data[start+j], b = data[start+j+length/2]*rotation;
                data[start+j] = a+b; data[start+j+length/2] = a-b; rotation *= step;
            }
        }
    }
}
struct Peak { double midi, weight; };
inline std::vector<Peak> peaks(const std::vector<float>& mono, int centre, double rate) {
    std::vector<std::complex<double>> spectrum(fftSize);
    for (int i = 0; i < fftSize; ++i) {
        const int index = centre+i-fftSize/2;
        const double value = index >= 0 && index < static_cast<int>(mono.size()) ? mono[static_cast<std::size_t>(index)] : 0;
        spectrum[static_cast<std::size_t>(i)] = value*(0.5-0.5*std::cos(2*3.141592653589793*i/(fftSize-1)));
    }
    fft(spectrum);
    double maximum = 0;
    for (int i = 1; i < fftSize/2; ++i) maximum = std::max(maximum,std::abs(spectrum[static_cast<std::size_t>(i)]));
    std::vector<Peak> result;
    if (maximum < 1.0e-5) return result;
    for (int i = 2; i < fftSize/2-1; ++i) {
        const double a = std::abs(spectrum[static_cast<std::size_t>(i-1)]);
        const double b = std::abs(spectrum[static_cast<std::size_t>(i)]);
        const double c = std::abs(spectrum[static_cast<std::size_t>(i+1)]);
        if (b < maximum*0.035 || b < a || b <= c) continue;
        const double la = std::log(a+1.0e-20), lb = std::log(b+1.0e-20), lc = std::log(c+1.0e-20);
        const double offset = std::clamp(0.5*(la-lc)/(la-2*lb+lc),-0.5,0.5);
        const double frequency = (i+offset)*rate/fftSize;
        if (frequency < 55 || frequency > 3500) continue;
        result.push_back({69+12*std::log2(frequency/440.0),b/(1+std::pow(frequency/1500.0,2))});
    }
    return result;
}
inline std::array<double,12> chroma(const std::vector<Peak>& peaksIn, double tuning) {
    std::array<double,12> result{};
    for (auto peak : peaksIn) {
        const auto midi = static_cast<int>(std::lround(peak.midi-tuning/100));
        result[static_cast<std::size_t>((midi%12+12)%12)] += peak.weight;
    }
    const double total = std::accumulate(result.begin(),result.end(),0.0);
    if (total > 0) for (auto& value : result) value /= total;
    return result;
}
inline std::pair<Chord,double> classify(const std::array<double,12>& input, int bassPitch) {
    Chord bestChord;
    double best = -1, second = -1;
    const double norm = std::sqrt(std::inner_product(input.begin(),input.end(),input.begin(),0.0));
    if (norm < 1.0e-8) return {bestChord,0};
    for (int root = 0; root < 12; ++root) for (int q = 0; q < 8; ++q) {
        Chord chord; chord.root = root; chord.quality = static_cast<ChordQuality>(q); chord.minor = q == 1 || q == 4;
        const auto notes = chord.notes();
        double value = 0;
        for (auto note : notes) value += input[static_cast<std::size_t>(note%12)];
        value /= norm*std::sqrt(static_cast<double>(notes.size()));
        // Slight preference for roots supported by low-register energy.
        value += input[static_cast<std::size_t>(root)]*0.015 + (root == bassPitch ? 0.04 : 0.0);
        if (value > best) { second = best; best = value; bestChord = chord; }
        else second = std::max(second,value);
    }
    return {bestChord,std::clamp((best-second)*5.0,0.0,1.0)};
}
}
// Background/offline only. 22.05-kHz analysis, Hann STFT peaks, tuning-adjusted
// chroma templates, three-frame majority smoothing and duration-weighted key profiles.
inline HarmonicResult analyseHarmony(const float* input, int count, double sourceRate,
                                      const std::function<bool(double)>& progress = {}) {
    if (!input || !std::isfinite(sourceRate) || sourceRate < 8000 || sourceRate > 192000 || count <= 0 || count > sourceRate*120)
        throw std::invalid_argument("Analysis needs 0-120 seconds of finite 8-192 kHz audio");
    constexpr double rate = 22050;
    const int outputCount = static_cast<int>(count*rate/sourceRate);
    std::vector<float> mono(static_cast<std::size_t>(outputCount));
    // Area averaging on downsampling avoids simple point-sampling aliasing.
    for (int i = 0; i < outputCount; ++i) {
        const int start = static_cast<int>(i*sourceRate/rate);
        const int end = std::min(count,std::max(start+1,static_cast<int>((i+1)*sourceRate/rate)));
        double sum = 0;
        for (int j = start; j < end; ++j) { if (!std::isfinite(input[j])) throw std::invalid_argument("Nonfinite audio"); sum += input[j]; }
        mono[static_cast<std::size_t>(i)] = static_cast<float>(sum/std::max(1,end-start));
    }
    constexpr int hop = 2205;
    std::vector<std::vector<harmonic::Peak>> frames;
    std::array<double,101> tuning{};
    for (int centre = hop/2; centre < outputCount; centre += hop) {
        if (progress && !progress(0.7*centre/std::max(1,outputCount))) throw std::runtime_error("Analysis cancelled");
        auto peaks = harmonic::peaks(mono,centre,rate);
        for (auto peak : peaks) {
            const double cents = (peak.midi-std::round(peak.midi))*100;
            tuning[static_cast<std::size_t>(std::clamp(static_cast<int>(std::lround(cents))+50,0,100))] += peak.weight;
        }
        frames.push_back(std::move(peaks));
    }
    HarmonicResult result;
    result.tuningCents = static_cast<double>(std::max_element(tuning.begin(),tuning.end())-tuning.begin())-50;
    if (std::accumulate(tuning.begin(),tuning.end(),0.0) == 0) { result.tuningCents = 0; return result; }
    std::vector<std::pair<Chord,double>> labels;
    std::array<double,12> aggregate{};
    for (const auto& peaks : frames) {
        const auto chroma = harmonic::chroma(peaks,result.tuningCents);
        int bass = -1;
        if (!peaks.empty()) bass = (static_cast<int>(std::lround(std::min_element(peaks.begin(),peaks.end(),[](const auto& a,const auto& b){return a.midi<b.midi;})->midi-result.tuningCents/100))+1200)%12;
        labels.push_back(harmonic::classify(chroma,bass));
        for (int i = 0; i < 12; ++i) aggregate[static_cast<std::size_t>(i)] += chroma[static_cast<std::size_t>(i)];
    }
    auto smoothed = labels;
    for (std::size_t i = 1; i+1 < labels.size(); ++i)
        if (labels[i-1].first.root == labels[i+1].first.root && labels[i-1].first.quality == labels[i+1].first.quality) smoothed[i] = labels[i-1];
    for (std::size_t i = 0; i < smoothed.size(); ++i) {
        auto [chord,confidence] = smoothed[i];
        if (confidence <= 0.001) continue;
        const double start = i*hop/rate, end = std::min((i+1)*hop/rate,count/sourceRate);
        if (!result.regions.empty() && result.regions.back().chord.root == chord.root && result.regions.back().chord.quality == chord.quality
            && std::abs(result.regions.back().end-start)<0.01) {
            auto& region = result.regions.back(); const double old = region.end-region.start;
            region.confidence = (region.confidence*old+confidence*(end-start))/(old+end-start); region.end = end;
        } else result.regions.push_back({start,end,confidence,chord});
    }
    const std::array<double,12> major{6.35,2.23,3.48,2.33,4.38,4.09,2.52,5.19,2.39,3.66,2.29,2.88};
    const std::array<double,12> minor{6.33,2.68,3.52,5.38,2.60,3.53,2.54,4.75,3.98,2.69,3.34,3.17};
    double best = -2, second = -2;
    const double mean = std::accumulate(aggregate.begin(),aggregate.end(),0.0)/12;
    for (int root = 0; root < 12; ++root) for (int mode = 0; mode < 2; ++mode) {
        const auto& profile = mode == 0 ? major : minor;
        const double pm = std::accumulate(profile.begin(),profile.end(),0.0)/12;
        double dot = 0, aa = 0, pp = 0;
        for (int i = 0; i < 12; ++i) {
            const double a = aggregate[static_cast<std::size_t>((root+i)%12)]-mean, p = profile[static_cast<std::size_t>(i)]-pm;
            dot += a*p; aa += a*a; pp += p*p;
        }
        const double score = aa > 0 ? dot/std::sqrt(aa*pp) : 0;
        if (score > best) { second = best; best = score; result.keyRoot = root; result.minor = mode == 1; }
        else second = std::max(second,score);
    }
    result.keyConfidence = std::clamp((best-second)*3.0,0.0,1.0);
    if (progress && !progress(1)) throw std::runtime_error("Analysis cancelled");
    return result;
}
}
