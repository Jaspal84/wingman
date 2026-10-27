#pragma once
#include "Arrangement.h"
#include "Library/Library.h"
#include "MIDI/MidiExport.h"
#include <random>
namespace wingman::studio {
struct InspireSettings {
    double bpm = 120, energy = .7;
    int key = -1, mode = 0, bars = 8;
    uint32_t seed = 42;
    juce::String genre, mood;
};
struct Generated { Arrangement arrangement; juce::String explanation; };
inline int instrumentLane(const juce::String& tag) {
    auto s = tag.toLowerCase();
    if (s.contains("kick")) return 0;
    if (s.contains("snare") || s.contains("clap")) return 1;
    if (s.contains("hat") || s.contains("dhol") || s.contains("tabla") || s.contains("drum") || s.contains("perc")) return 2;
    if (s.contains("bass")) return 3;
    if (s.contains("chord") || s.contains("guitar") || s.contains("pad")) return 4;
    if (s.contains("melody") || s.contains("lead")) return 5;
    if (s.contains("vocal")) return 6;
    if (s.contains("fx") || s.contains("effect")) return 7;
    return -1;
}
inline Generated inspire(const std::vector<Sample>& samples, const InspireSettings& settings, const Cancel& cancel,
                         const std::optional<Region>& reference = {}, int onlyLane = -1) {
    if (settings.bpm < 40 || settings.bpm > 240 || settings.bars < 1 || settings.bars > 16 || settings.energy < 0 || settings.energy > 1 || settings.key < -1 || settings.key > 11)
        throw std::runtime_error("Invalid generation controls");
    Generated result; result.arrangement.bpm = settings.bpm; result.arrangement.loopEnd = settings.bars * 4;
    if (reference) { auto r = *reference; r.id = juce::Uuid().toString(); r.beat = 0; r.duration = std::min(r.duration, settings.bars * 4 * result.arrangement.secondsPerBeat()); result.arrangement.lanes[6].clips.push_back(std::move(r)); }
    std::mt19937 random(settings.seed); std::unordered_map<std::string, std::shared_ptr<const Asset>> assets;
    int populated = 0;
    for (int lane = 0; lane < 8; ++lane) {
        if (cancel()) throw std::runtime_error("Generation cancelled");
        if ((onlyLane >= 0 && onlyLane != lane) || (lane == 6 && reference)) continue;
        std::vector<Sample> candidates;
        for (const auto& s : samples) {
            if (instrumentLane(s.instrument) != lane || (settings.genre.isNotEmpty() && !s.genre.equalsIgnoreCase(settings.genre)) || (settings.mood.isNotEmpty() && !s.mood.equalsIgnoreCase(settings.mood))) continue;
            if (lane >= 3 && settings.key >= 0 && s.key >= 0 && compatibility(s, 0, settings.key, settings.mode) < 0) continue;
            if (s.bpm > 0 && (settings.bpm / s.bpm < .8 || settings.bpm / s.bpm > 1.25)) continue;
            candidates.push_back(s);
        }
        if (candidates.empty()) { result.explanation += "Track " + juce::String(lane + 1) + ": no compatible tagged audio; left empty.\n"; continue; }
        auto score = [&](const Sample& s) { return compatibility(s, settings.bpm, settings.key, settings.mode) - std::abs(std::min(1.0, s.rms * 4) - settings.energy); };
        std::stable_sort(candidates.begin(), candidates.end(), [&](const auto& a, const auto& b) { auto sa = score(a), sb = score(b); return sa == sb ? a.path < b.path : sa > sb; });
        const auto& chosen = candidates[static_cast<std::size_t>(random() % std::min<std::size_t>(3, candidates.size()))];
        auto key = chosen.path.toStdString(); if (!assets.count(key)) { std::shared_ptr<const Asset> asset; auto ok = importAsset(juce::File(chosen.path), asset, cancel); if (ok.failed()) throw std::runtime_error(ok.getErrorMessage().toStdString()); assets[key] = asset; }
        auto asset = assets[key]; double speed = chosen.bpm > 0 ? settings.bpm / chosen.bpm : 1;
        double duration = asset->seconds() / speed; const double spb = 60 / settings.bpm;
        // One-shots use original, seed-varied rhythmic placements; loops repeat
        // their actual audio span. Speed matching is linked resampling, not stretch.
        double interval = chosen.duration < .8 ? (lane < 2 ? 2.0 : .5) : duration / spb;
        int inserted = 0;
        for (double beat = 0; beat < settings.bars * 4 && inserted < 64; beat += std::max(.25, interval)) {
            if (chosen.duration < .8 && settings.energy < .5 && (random() % 3 == 0)) continue;
            Region c; c.asset = asset; c.beat = beat; c.speed = speed; c.duration = std::min(duration, (settings.bars * 4 - beat) * spb);
            c.fadeIn = std::min(.003, c.duration / 4); c.fadeOut = std::min(.01, c.duration / 4);
            if (lane == 1 && chosen.duration < .8) c.beat += 1;
            if (c.beat + c.duration / spb > settings.bars * 4) c.duration = (settings.bars * 4 - c.beat) * spb;
            if (c.duration > 0 && valid(c)) { result.arrangement.lanes[static_cast<std::size_t>(lane)].clips.push_back(c); ++inserted; }
        }
        if (inserted) ++populated;
        result.explanation += "Track " + juce::String(lane + 1) + ": " + juce::File(chosen.path).getFileName() + " | score " + juce::String(score(chosen), 2) + " | " + juce::String(inserted) + " regions | rate " + juce::String(speed, 3) + " (pitch linked).\n";
    }
    if (!populated) throw std::runtime_error("No compatible instrument-tagged samples. Index audio and save instrument/genre/mood metadata first.");
    result.explanation = "Deterministic local selection | seed " + juce::String(settings.seed) + " | no ML model\n" + result.explanation;
    return result;
}
struct FlipSettings { int slices = 8; double probability = 1, swing = 0; uint32_t seed = 42; bool reverse = false; };
inline std::vector<Region> flip(const Region& source, double bpm, const FlipSettings& s, const std::vector<double>& transients = {}) {
    if (!valid(source) || bpm < 30 || bpm > 300 || s.slices < 2 || s.slices > 32 || s.probability < 0 || s.probability > 1 || s.swing < 0 || s.swing > .49) throw std::runtime_error("Invalid Flip controls");
    std::vector<double> boundaries{0};
    if (!transients.empty()) {
        for (double t : transients) { double local = (t - source.offset) / source.speed; if (local > .01 && local < source.duration - .01 && boundaries.size() < static_cast<std::size_t>(s.slices)) boundaries.push_back(local); }
    } else for (int i = 1; i < s.slices; ++i) boundaries.push_back(source.duration * i / s.slices);
    boundaries.push_back(source.duration); std::sort(boundaries.begin(), boundaries.end());
    std::vector<int> order; for (int i = 0; i < static_cast<int>(boundaries.size()) - 1; ++i) order.push_back(i);
    std::mt19937 random(s.seed); std::shuffle(order.begin(), order.end(), random); double at = source.beat;
    std::vector<Region> result;
    for (std::size_t step = 0; step < order.size(); ++step) {
        int slice = order[step]; double length = boundaries[static_cast<std::size_t>(slice + 1)] - boundaries[static_cast<std::size_t>(slice)];
        if (static_cast<double>(random()) / std::mt19937::max() <= s.probability) {
            Region c = source; c.id = juce::Uuid().toString(); c.offset = source.reverse ? source.offset + (source.duration - boundaries[static_cast<std::size_t>(slice + 1)]) * source.speed : source.offset + boundaries[static_cast<std::size_t>(slice)] * source.speed;
            c.duration = length; c.beat = at + (step % 2 ? s.swing * .5 : 0); c.reverse = source.reverse != s.reverse;
            c.fadeIn = std::min(.003, length / 4); c.fadeOut = std::min(.005, length / 4); if (valid(c)) result.push_back(c);
        }
        at += length * bpm / 60;
    }
    return result;
}
inline juce::MidiMessageSequence regionMidi(const std::vector<Region>& clips, int baseNote = 36) {
    juce::MidiMessageSequence result; int note = baseNote;
    for (const auto& c : clips) { auto on = juce::MidiMessage::noteOn(1, note, static_cast<juce::uint8>(100)); on.setTimeStamp(c.beat); result.addEvent(on); auto off = juce::MidiMessage::noteOff(1, note); off.setTimeStamp(c.beat + .1); result.addEvent(off); note = baseNote + (note - baseNote + 1) % 16; }
    result.updateMatchedPairs(); return result;
}
}
