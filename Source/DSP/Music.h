#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace wingman {
enum class ChordQuality { Major, Minor, Dominant7, Major7, Minor7, Sus2, Sus4, Diminished };
struct Chord {
    int root = 0;
    bool minor = false;
    ChordQuality quality = ChordQuality::Major;
    int inversion = 0;
    double beats = 4;
    std::vector<int> notes(int transpose = 0) const {
        const auto q = quality == ChordQuality::Major && minor ? ChordQuality::Minor : quality;
        std::vector<int> intervals;
        switch (q) {
            case ChordQuality::Major: intervals = {0,4,7}; break;
            case ChordQuality::Minor: intervals = {0,3,7}; break;
            case ChordQuality::Dominant7: intervals = {0,4,7,10}; break;
            case ChordQuality::Major7: intervals = {0,4,7,11}; break;
            case ChordQuality::Minor7: intervals = {0,3,7,10}; break;
            case ChordQuality::Sus2: intervals = {0,2,7}; break;
            case ChordQuality::Sus4: intervals = {0,5,7}; break;
            case ChordQuality::Diminished: intervals = {0,3,6}; break;
        }
        for (auto& note : intervals) note += 48+root+transpose;
        for (int i = 0; i < inversion && i < static_cast<int>(intervals.size()); ++i) {
            const int first = intervals.front()+12; intervals.erase(intervals.begin()); intervals.push_back(first);
        }
        return intervals;
    }
};
inline std::optional<Chord> parseChord(const std::string& token) {
    if (token.empty()) return {};
    std::string s = token;
    Chord chord;
    try {
        if (auto p = s.find(':'); p != std::string::npos) {
            std::size_t used = 0; auto duration = s.substr(p+1); chord.beats = std::stod(duration,&used);
            if (used != duration.size() || !std::isfinite(chord.beats) || chord.beats < 0.25 || chord.beats > 32) return {};
            s.resize(p);
        }
        if (auto p = s.find('^'); p != std::string::npos) {
            const auto value = s.substr(p+1);
            if (value.size() != 1 || value[0] < '0' || value[0] > '3') return {};
            chord.inversion = value[0]-'0'; s.resize(p);
        }
    } catch (...) { return {}; }
    if (s.empty()) return {};
    const auto index = std::string("C D EF G A B").find(s[0]);
    if (index == std::string::npos || s[0] == ' ') return {};
    int root = static_cast<int>(index); std::size_t p = 1;
    if (p < s.size() && (s[p] == '#' || s[p] == 'b')) root += s[p++] == '#' ? 1 : -1;
    chord.root = (root+12)%12;
    const auto suffix = s.substr(p);
    if (suffix.empty()) chord.quality = ChordQuality::Major;
    else if (suffix == "m") chord.quality = ChordQuality::Minor;
    else if (suffix == "7") chord.quality = ChordQuality::Dominant7;
    else if (suffix == "maj7") chord.quality = ChordQuality::Major7;
    else if (suffix == "m7") chord.quality = ChordQuality::Minor7;
    else if (suffix == "sus2") chord.quality = ChordQuality::Sus2;
    else if (suffix == "sus4") chord.quality = ChordQuality::Sus4;
    else if (suffix == "dim") chord.quality = ChordQuality::Diminished;
    else return {};
    chord.minor = chord.quality == ChordQuality::Minor || chord.quality == ChordQuality::Minor7;
    if (chord.inversion >= static_cast<int>(chord.notes().size())) return {};
    return chord;
}
inline double progressionBeats(const std::vector<Chord>& chords) { double total = 0; for (const auto& c : chords) total += c.beats; return total; }
inline std::string chordName(const Chord& chord) {
    const std::array<std::string,12> roots{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    const std::array<std::string,8> suffix{"","m","7","maj7","m7","sus2","sus4","dim"};
    return roots[static_cast<std::size_t>(chord.root)]+suffix[static_cast<std::size_t>(chord.quality)];
}
// Onset-energy autocorrelation. Confidence reflects periodicity, not harmonic analysis.
inline std::optional<double> detectBpm(const float* samples, int count, double sampleRate) {
    if (!samples || !std::isfinite(sampleRate) || sampleRate <= 0 || count < sampleRate * 3) return {};
    const int hop = std::max(1, static_cast<int>(sampleRate / 200.0));
    std::vector<double> onset;
    double previous = 0;
    for (int i = 0; i + hop <= count; i += hop) {
        double energy = 0;
        for (int j = 0; j < hop; ++j) energy += static_cast<double>(samples[i+j]) * samples[i+j];
        energy = std::sqrt(energy / hop);
        onset.push_back(std::max(0.0, energy - previous));
        previous = energy;
    }
    double power = 0;
    for (auto x : onset) power += x * x;
    if (power < 1.0e-8) return {};
    const double rate = sampleRate / hop;
    int bestLag = 0;
    double best = 0;
    for (int lag = static_cast<int>(rate * 60 / 180); lag <= static_cast<int>(rate * 60 / 60); ++lag) {
        double sum = 0;
        for (std::size_t i = static_cast<std::size_t>(lag); i < onset.size(); ++i) sum += onset[i] * onset[i-static_cast<std::size_t>(lag)];
        if (sum > best) { best = sum; bestLag = lag; }
    }
    if (bestLag == 0 || best / power < 0.12) return {};
    return rate * 60 / bestLag;
}
}
