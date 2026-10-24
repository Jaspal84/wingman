#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace wingman {
struct Chord {
    int root = 0;
    bool minor = false;
    std::array<int, 3> notes(int transpose = 0) const {
        return {48 + root + transpose, 48 + root + transpose + (minor ? 3 : 4), 55 + root + transpose};
    }
};
inline std::optional<Chord> parseChord(const std::string& s) {
    if (s.empty()) return {};
    const std::string names = "C D EF G A B";
    const auto index = names.find(s[0]);
    if (index == std::string::npos || s[0] == ' ') return {};
    int root = static_cast<int>(index);
    std::size_t p = 1;
    if (p < s.size() && (s[p] == '#' || s[p] == 'b')) root += s[p++] == '#' ? 1 : -1;
    const bool minor = p < s.size() && s[p] == 'm';
    if (minor) ++p;
    if (p != s.size()) return {};
    return Chord{(root + 12) % 12, minor};
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
