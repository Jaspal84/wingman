#pragma once
#include "Arrangement.h"
#include "MIDI/MidiExport.h"
namespace wingman::studio {
struct Pad { std::shared_ptr<const Asset> soft, hard; int root = 60, splitVelocity = 90; double gain = 1; bool oneShot = true; };
struct InstrumentState {
    std::array<Pad, 16> pads;
    double attack = .005, decay = .1, sustain = .8, release = .15;
    bool chromatic = false;
};
class InstrumentEngine {
    struct Snapshot { InstrumentState state; uint64_t revision = 0; };
    struct Voice { const Asset* asset = nullptr; double at = 0, step = 1, envelope = 0, age = 0, releasedAt = 0, releaseLevel = 0; int note = 0; float velocity = 0; bool released = false, oneShot = false; };
    std::array<Voice, 64> voices;
    struct Event { int note = 0, velocity = 0; };
    struct Slot { std::atomic<std::size_t> sequence{0}; Event event; };
    std::array<Slot, 256> commands;
    std::atomic<std::size_t> enqueue{0};
    std::size_t dequeue = 0;
    bool pop(Event& event) {
        auto& slot = commands[dequeue % commands.size()]; if (slot.sequence.load(std::memory_order_acquire) != dequeue + 1) return false;
        event = slot.event; slot.sequence.store(dequeue + commands.size(), std::memory_order_release); ++dequeue; return true;
    }
    std::atomic<bool> reset{false};
    std::atomic<const Snapshot*> current{nullptr}, hazard{nullptr};
    std::vector<std::unique_ptr<Snapshot>> retained;
    uint64_t revision = 0, activeRevision = 0;
public:
    std::atomic<int> droppedEvents{0};
    InstrumentEngine() { for (std::size_t i = 0; i < commands.size(); ++i) commands[i].sequence = i; publish({}); }
    void publish(const InstrumentState& state) {
        auto next = std::make_unique<Snapshot>(); next->state = state; next->revision = ++revision;
        auto* p = next.get(); retained.push_back(std::move(next)); current.store(p); auto* active = hazard.load();
        std::erase_if(retained, [&](const auto& v) { return v.get() != p && v.get() != active; });
    }
    bool note(int noteNumber, int velocity) {
        if (noteNumber < 0 || noteNumber > 127) return false;
        auto pos = enqueue.load();
        for (int attempt = 0; attempt < 16; ++attempt) {
            auto& slot = commands[pos % commands.size()]; auto sequence = slot.sequence.load(std::memory_order_acquire);
            auto difference = static_cast<int64_t>(sequence) - static_cast<int64_t>(pos);
            if (difference == 0) { if (enqueue.compare_exchange_weak(pos, pos + 1)) { slot.event = {noteNumber, std::clamp(velocity, 0, 127)}; slot.sequence.store(pos + 1, std::memory_order_release); return true; } }
            else if (difference < 0) { ++droppedEvents; reset = true; return false; } else pos = enqueue.load();
        }
        ++droppedEvents; reset = true; return false;
    }
    void allNotesOff() { reset = true; }
    void render(juce::AudioBuffer<float>& output, double rate) {
        const Snapshot* snapshot; do { snapshot = current.load(); hazard.store(snapshot); } while (current.load() != snapshot);
        bool silence = reset.exchange(false);
        if (silence || activeRevision != snapshot->revision) { for (auto& v : voices) v = {}; activeRevision = snapshot->revision; }
        if (silence) { Event discarded; for (int i = 0; i < 256 && pop(discarded); ++i) {} }
        const auto& state = snapshot->state;
        Event event; int handled = 0;
        while (handled++ < 256 && pop(event)) {
            int note = event.note, velocity = event.velocity;
            if (velocity == 0) { for (auto& v : voices) if (v.asset && v.note == note && !v.oneShot) { v.released = true; v.releasedAt = v.age; v.releaseLevel = v.envelope; } continue; }
            int padIndex = state.chromatic ? 0 : note - 36; if (padIndex < 0 || padIndex >= 16) continue;
            const auto& pad = state.pads[static_cast<std::size_t>(padIndex)]; const auto* asset = velocity >= pad.splitVelocity && pad.hard ? pad.hard.get() : pad.soft.get(); if (!asset) continue;
            auto free = std::find_if(voices.begin(), voices.end(), [](const auto& v) { return !v.asset; });
            if (free == voices.end()) free = std::max_element(voices.begin(), voices.end(), [](const auto& a, const auto& b) { return a.age < b.age; });
            *free = {}; free->asset = asset; free->note = note; free->oneShot = pad.oneShot; free->velocity = static_cast<float>(velocity / 127.0 * pad.gain);
            free->step = asset->rate / rate * (state.chromatic ? std::pow(2.0, (note - pad.root) / 12.0) : 1);
        }
        for (auto& v : voices) if (v.asset) for (int i = 0; i < output.getNumSamples(); ++i) {
            if (v.at >= v.asset->frames) { v.asset = nullptr; break; }
            if (v.released) v.envelope = v.releaseLevel * std::max(0.0, 1 - (v.age - v.releasedAt) / std::max(.001, state.release));
            else if (v.age < state.attack) v.envelope = v.age / std::max(.00001, state.attack);
            else v.envelope = state.sustain + (1 - state.sustain) * std::max(0.0, 1 - (v.age - state.attack) / std::max(.001, state.decay));
            if (v.released && v.envelope <= 0) { v.asset = nullptr; break; }
            for (int c = 0; c < output.getNumChannels(); ++c) output.addSample(c, i, v.asset->sample(c, v.at) * v.velocity * static_cast<float>(v.envelope));
            v.at += v.step; v.age += 1 / rate;
        }
        hazard.store(nullptr);
    }
};
struct DrumPattern {
    std::array<std::array<float, 16>, 16> steps{};
    double swing = 0;
    uint32_t seed = 42;
    int bars = 1;
};
inline DrumPattern drumPreset(int preset) {
    DrumPattern p;
    for (int i = 0; i < 16; i += 2) p.steps[2][static_cast<std::size_t>(i)] = i % 4 == 0 ? .75f : .45f;
    for (int i : {0, 8}) p.steps[0][static_cast<std::size_t>(i)] = 1;
    for (int i : {4, 12}) p.steps[1][static_cast<std::size_t>(i)] = .9f;
    if (preset == 1) { for (int i : {0, 3, 6, 8, 11, 14}) p.steps[3][static_cast<std::size_t>(i)] = i % 4 == 0 ? 1.f : .65f; for (int i : {2, 7, 10, 15}) p.steps[4][static_cast<std::size_t>(i)] = .7f; }
    if (preset == 2) { for (int i : {0, 4, 7, 8, 12, 15}) p.steps[3][static_cast<std::size_t>(i)] = .8f; for (int i : {2, 5, 6, 10, 13, 14}) p.steps[4][static_cast<std::size_t>(i)] = .55f; }
    return p;
}
inline juce::MidiMessageSequence drumMidi(const DrumPattern& pattern) {
    if (pattern.bars < 1 || pattern.bars > 8 || pattern.swing < 0 || pattern.swing > .49) throw std::runtime_error("Invalid drum pattern");
    juce::MidiMessageSequence midi;
    for (int bar = 0; bar < pattern.bars; ++bar) for (int pad = 0; pad < 16; ++pad) for (int step = 0; step < 16; ++step) {
        float v = pattern.steps[static_cast<std::size_t>(pad)][static_cast<std::size_t>(step)]; if (v <= 0) continue;
        double beat = bar * 4 + step * .25 + (step % 2 ? pattern.swing * .25 : 0);
        auto on = juce::MidiMessage::noteOn(10, 36 + pad, static_cast<juce::uint8>(std::clamp(v, 0.f, 1.f) * 127)); on.setTimeStamp(beat); midi.addEvent(on);
        auto off = juce::MidiMessage::noteOff(10, 36 + pad); off.setTimeStamp(beat + .12); midi.addEvent(off);
    }
    midi.updateMatchedPairs(); return midi;
}
inline std::array<Lane, 8> drumArrangement(const InstrumentState& instrument, const DrumPattern& pattern, double bpm, double startBeat) {
    auto midi = drumMidi(pattern); std::array<Lane, 8> lanes;
    for (int i = 0; i < midi.getNumEvents(); ++i) { auto message = midi.getEventPointer(i)->message; if (!message.isNoteOn()) continue;
        int index = message.getNoteNumber() - 36; const auto& pad = instrument.pads[static_cast<std::size_t>(index)]; auto asset = message.getVelocity() >= pad.splitVelocity && pad.hard ? pad.hard : pad.soft; if (!asset) continue;
        Region c; c.asset = asset; c.beat = startBeat + message.getTimeStamp(); c.duration = asset->seconds(); c.gain = message.getFloatVelocity() * static_cast<float>(pad.gain); c.fadeIn = std::min(instrument.attack, c.duration / 4); c.fadeOut = std::min(instrument.release, c.duration / 4);
        if (valid(c)) lanes[static_cast<std::size_t>(index % 8)].clips.push_back(c);
    }
    return lanes;
}
}

namespace wingman::studio {
inline juce::String instrumentXml(const InstrumentState& state, const DrumPattern& pattern) {
    juce::ValueTree tree("JasProInstrument"); tree.setProperty("version", 1, nullptr); tree.setProperty("chromatic", state.chromatic, nullptr);
    tree.setProperty("attack", state.attack, nullptr); tree.setProperty("decay", state.decay, nullptr); tree.setProperty("sustain", state.sustain, nullptr); tree.setProperty("release", state.release, nullptr);
    tree.setProperty("swing", pattern.swing, nullptr); tree.setProperty("bars", pattern.bars, nullptr);
    for (std::size_t i = 0; i < 16; ++i) {
        const auto& p = state.pads[i]; juce::ValueTree pad("Pad"); pad.setProperty("soft", p.soft ? p.soft->source.getFullPathName() : juce::String{}, nullptr); pad.setProperty("hard", p.hard ? p.hard->source.getFullPathName() : juce::String{}, nullptr);
        pad.setProperty("root", p.root, nullptr); pad.setProperty("split", p.splitVelocity, nullptr); pad.setProperty("gain", p.gain, nullptr); pad.setProperty("oneShot", p.oneShot, nullptr);
        juce::StringArray steps; for (float value : pattern.steps[i]) steps.add(juce::String(value, 6)); pad.setProperty("steps", steps.joinIntoString(","), nullptr); tree.addChild(pad, -1, nullptr);
    }
    return tree.toXmlString();
}
inline void loadInstrumentXml(const juce::String& xml, InstrumentState& destination, DrumPattern& destinationPattern, const std::function<bool()>& cancel = [] { return false; }) {
    if (xml.getNumBytesAsUTF8() > 65536) throw std::runtime_error("Oversized instrument preset");
    auto tree = juce::ValueTree::fromXml(xml); if (!tree.hasType("JasProInstrument") || static_cast<int>(tree["version"]) != 1 || tree.getNumChildren() != 16) throw std::runtime_error("Invalid instrument preset");
    InstrumentState state; DrumPattern pattern; state.chromatic = tree["chromatic"]; state.attack = tree["attack"]; state.decay = tree["decay"]; state.sustain = tree["sustain"]; state.release = tree["release"]; pattern.swing = tree["swing"]; pattern.bars = tree["bars"];
    for (double value : {state.attack, state.decay, state.sustain, state.release, pattern.swing}) if (!std::isfinite(value) || value < 0 || value > 2) throw std::runtime_error("Invalid envelope");
    if (state.sustain > 1 || pattern.swing > .49 || pattern.bars < 1 || pattern.bars > 8) throw std::runtime_error("Invalid instrument pattern");
    std::unordered_map<std::string, std::shared_ptr<const Asset>> assets;
    for (int i = 0; i < 16; ++i) {
        if (cancel()) throw std::runtime_error("Preset load cancelled"); auto v = tree.getChild(i); if (!v.hasType("Pad")) throw std::runtime_error("Invalid pad"); auto& p = state.pads[static_cast<std::size_t>(i)];
        p.root = v["root"]; p.splitVelocity = v["split"]; p.gain = v["gain"]; p.oneShot = v["oneShot"];
        if (p.root < 0 || p.root > 127 || p.splitVelocity < 1 || p.splitVelocity > 127 || !std::isfinite(p.gain) || p.gain < 0 || p.gain > 2) throw std::runtime_error("Invalid sample mapping");
        for (auto pair : {std::pair<const char*, std::shared_ptr<const Asset>*>{"soft", &p.soft}, {"hard", &p.hard}}) {
            auto path = v[pair.first].toString(); if (path.isEmpty()) continue; if (!juce::File::isAbsolutePath(path)) throw std::runtime_error("Invalid sample path"); auto key = path.toStdString();
            if (!assets.count(key)) { std::shared_ptr<const Asset> asset; auto result = importAsset(juce::File(path), asset, cancel); if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString()); assets[key] = asset; } *pair.second = assets[key];
        }
        auto steps = juce::StringArray::fromTokens(v["steps"].toString(), ",", ""); if (steps.size() != 16) throw std::runtime_error("Invalid sequencer steps");
        for (int step = 0; step < 16; ++step) { auto token = steps[step]; if (!token.containsOnly("0123456789.")) throw std::runtime_error("Invalid step value"); float value = token.getFloatValue(); if (!std::isfinite(value) || value < 0 || value > 1) throw std::runtime_error("Invalid step velocity"); pattern.steps[static_cast<std::size_t>(i)][static_cast<std::size_t>(step)] = value; }
    }
    destination = std::move(state); destinationPattern = pattern;
}
}
