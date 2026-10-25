#pragma once
#include "DSP/Music.h"
#include <cstdint>
#include <random>
#include <stdexcept>
#include <limits>

namespace wingman {
enum class PatternKind { Bass, Drums, Melody };
enum class Style { PunjabiPop, Bhangra, Bollywood, HipHop, Trap, House, Funk, AcousticPop };
struct NoteEvent {
    double beat = 0, duration = 0.25;
    int note = 60, velocity = 90, channel = 1;
    int articulation = 0; // Guitar SFZ keyswitch metadata, emitted as CC20 when nonzero.
    bool operator==(const NoteEvent&) const = default;
};
struct PatternSettings {
    std::uint32_t seed = 1;
    int variation = 0, bars = 4, octave = 0, velocity = 95, hatSubdivision = 4;
    int lowest = 60, highest = 84;
    double density = 0.5, swing = 0, humanize = 0, noteLength = 0.8, complexity = 0.35;
    double rootEmphasis = 0.8, syncopation = 0.2, kickDensity = 0.5, snareVariation = 0.2, fill = 0.3, repetition = 0.6;
    bool ghostNotes = true, minor = false;
    int keyRoot = 0;
    Style style = Style::PunjabiPop;
    // Kick, snare, clap, closed hat, open hat, percussion, low/mid/high tom.
    std::array<int,9> drumMap{36,38,39,42,46,75,45,47,50};
};
inline void validateSettings(const PatternSettings& s) {
    if (s.bars != 4 && s.bars != 8 && s.bars != 16 && s.bars != 32) throw std::invalid_argument("Pattern length must be 4, 8, 16 or 32 bars");
    for (double v : {s.density,s.swing,s.humanize,s.noteLength,s.complexity,s.rootEmphasis,s.syncopation,s.kickDensity,s.snareVariation,s.fill,s.repetition})
        if (!std::isfinite(v) || v < 0 || v > 1) throw std::invalid_argument("Pattern parameters must be within 0-1");
    if (s.velocity < 1 || s.velocity > 127 || s.lowest < 0 || s.highest > 127 || s.lowest > s.highest
        || s.octave < -2 || s.octave > 2 || s.keyRoot < 0 || s.keyRoot > 11 || s.variation < 0
        || (s.hatSubdivision != 2 && s.hatSubdivision != 4 && s.hatSubdivision != 8)) throw std::invalid_argument("Invalid MIDI range or subdivision");
    if(static_cast<int>(s.style)<0||static_cast<int>(s.style)>7) throw std::invalid_argument("Invalid style");
    for (int note : s.drumMap) if (note < 0 || note > 127) throw std::invalid_argument("Invalid drum map");
}
inline bool inScale(int note, int root, bool minor) {
    const int pc = (note-root+1200)%12;
    const std::array<int,7> major{0,2,4,5,7,9,11}, naturalMinor{0,2,3,5,7,8,10};
    const auto& scale = minor ? naturalMinor : major;
    return std::find(scale.begin(),scale.end(),pc) != scale.end();
}
inline std::vector<NoteEvent> generatePattern(PatternKind kind, const std::vector<Chord>& chords,
                                              int transpose, const PatternSettings& s) {
    validateSettings(s);
    if (chords.empty()) throw std::invalid_argument("A chord progression is required");
    std::vector<NoteEvent> result;
    int previous = kind == PatternKind::Bass ? 36 : (s.lowest+s.highest)/2;
    for (int bar = 0; bar < s.bars; ++bar) {
        // Per-bar generators localise changes; a phrase variation can be spliced by the caller.
        std::mt19937 random(s.seed ^ (static_cast<std::uint32_t>(bar)*2654435761u) ^ (static_cast<std::uint32_t>(s.variation)*2246822519u));
        auto unit = [&] { return static_cast<double>(random())/std::mt19937::max(); };
        auto add = [&](int note, double beat, double duration, int velocity, int channel) {
            const double shifted = beat + (unit()-0.5)*s.humanize*0.06;
            const double at = std::clamp(shifted,bar*4.0,bar*4.0+3.99);
            const int v = std::clamp(velocity+static_cast<int>((unit()-0.5)*s.humanize*20),1,127);
            result.push_back({at,std::min(duration,bar*4.0+4-at),note,v,channel});
        };
        if (kind == PatternKind::Drums) {
            const auto hit = [&](int instrument, double beat, int strength) {
                add(s.drumMap[static_cast<std::size_t>(instrument)],bar*4+beat,0.08,strength,10);
            };
            hit(0,0,s.velocity);
            if (s.style == Style::House) for (int b = 1; b < 4; ++b) hit(0,b,s.velocity);
            else {
                hit(0,2,s.velocity);
                if (unit()<s.kickDensity) hit(0,s.style == Style::Trap ? 2.75 : 1.5,s.velocity-12);
                if (unit()<s.kickDensity*s.complexity) hit(0,3.5,s.velocity-18);
            }
            if (s.style == Style::Bhangra) {
                // Dhol-inspired low/high call-response; GM toms are an approximation, not a dhol sampler.
                for (double at : {0.0,1.5,2.0,3.5}) hit(6,at,s.velocity-5);
                for (double at : {0.5,1.0,2.5,3.0}) hit(8,at,s.velocity-14);
                for (double at : {0.75,2.75}) hit(5,at,s.velocity-25);
            }
            if (s.style == Style::PunjabiPop || s.style == Style::Bollywood) {
                hit(5,0.75,s.velocity-25); hit(5,2.5,s.velocity-18); hit(8,3.75,s.velocity-22);
            }
            const bool halftime = s.style == Style::Trap || s.style == Style::HipHop;
            hit(1,halftime ? 2.0 : 1.0,s.velocity);
            if (!halftime) hit(1,3,s.velocity);
            if (s.style == Style::House || s.style == Style::PunjabiPop) { hit(2,1,s.velocity-12); hit(2,3,s.velocity-12); }
            if (s.ghostNotes && unit()<s.snareVariation) hit(1,2.75,static_cast<int>(s.velocity*0.4));
            const int divisions = s.hatSubdivision;
            for (int step = 0; step < 4*divisions; ++step) {
                if (step%divisions != 0 && unit()>0.25+s.density*0.75) continue;
                double beat = static_cast<double>(step)/divisions;
                if (step%2) beat += s.swing*0.5/divisions;
                hit(step%(divisions*2) == divisions*2-1 && s.complexity>0.55 ? 4 : 3,beat,s.velocity-(step%2 ? 28 : 15));
            }
            if ((bar+1)%4 == 0 && s.fill>0) for (int step = 0; step < 4; ++step)
                if (unit()<s.fill) hit(6+step%3,3+step*0.25,s.velocity-10+step*2);
            continue;
        }
        const int divisions = 4;
        const std::array<std::vector<int>,8> accents{{{0,3,4,6,8,11,12,14},{0,2,3,4,6,7,8,10,11,12,14,15},{0,3,6,8,10,12,15},
            {0,6,8,14},{0,7,8,11,14},{0,4,8,12,14},{0,3,6,7,8,10,13,15},{0,4,8,12}}};
        const auto& rhythm = accents[static_cast<std::size_t>(s.style)];
        for (int step = 0; step < 4*divisions; ++step) {
            const bool accent = std::find(rhythm.begin(),rhythm.end(),step) != rhythm.end();
            const double probability = s.density*(accent ? 1.0 : s.complexity*0.4);
            if (step != 0 && unit()>probability) continue;
            const double within = static_cast<double>(step)/divisions;
            const double at = bar*4+within;
            double harmonyLength = 0;
            for (const auto& c : chords) harmonyLength += c.beats;
            double cursor = std::fmod(at,harmonyLength);
            const Chord* chord = &chords.back();
            for (const auto& c : chords) { if (cursor < c.beats) { chord = &c; break; } cursor -= c.beats; }
            const auto chordNotes = chord->notes(transpose);
            const int root = (chord->root+transpose)%12;
            const int low = kind == PatternKind::Bass ? 28+12*s.octave : s.lowest;
            const int high = kind == PatternKind::Bass ? 52+12*s.octave : s.highest;
            std::vector<int> candidates;
            const bool strong = step%divisions == 0;
            for (int note = std::max(0,low); note <= std::min(127,high); ++note) {
                bool chordTone = false;
                for (auto tone : chordNotes) chordTone |= note%12 == tone%12;
                const bool allowed = inScale(note,s.keyRoot,s.minor);
                if (kind == PatternKind::Bass) {
                    if (chordTone && allowed && (step != 0 || note%12 == root)) candidates.push_back(note);
                } else if (allowed && (!strong || chordTone)) candidates.push_back(note);
            }
            if (candidates.empty()) throw std::invalid_argument("Chord and selected scale/range have no compatible notes");
            int chosen = candidates.front(); double best = std::numeric_limits<double>::max();
            for (int note : candidates) {
                double cost = std::abs(note-previous)*(1.0-s.complexity*0.5)+unit()*s.complexity*8;
                if (note%12 == root) cost -= s.rootEmphasis*5;
                if (kind == PatternKind::Melody && note == previous) cost -= s.repetition*3;
                if (cost < best) { best = cost; chosen = note; }
            }
            double beat = at+(step%2 ? s.swing*0.25 : 0);
            if (step && unit()<s.syncopation) beat += 0.125;
            add(chosen,beat,(1.0/divisions)*(0.15+0.8*s.noteLength),s.velocity,strong ? 1 : 1);
            previous = chosen;
        }
    }
    std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b) { return a.beat < b.beat; });
    if (kind != PatternKind::Drums) for (std::size_t i = 0; i+1 < result.size(); ++i)
        result[i].duration = std::min(result[i].duration,std::max(0.001,result[i+1].beat-result[i].beat-0.002));
    return result;
}
}
