#pragma once
#include "Studio/Generate.h"
namespace wingman::studio {
inline constexpr std::array<const char*, 8> layerNames{"Drums", "Bass", "Chords", "Keys", "Guitar", "Synth", "FX", "Vocals"};
inline constexpr std::array<juce::uint32, 8> layerColours{0xffe4b65a, 0xffac87df, 0xffe69a51, 0xff5ccbdc, 0xff7ac7a3, 0xffe6a5be, 0xffb9ce76, 0xff96b1ee};
inline int musicalRole(const juce::String& instrument) {
    auto tag = instrument.toLowerCase();
    if (tag.contains("vocal") || tag.contains("voice")) return 7;
    if (tag.contains("guitar")) return 4;
    if (tag.contains("piano") || tag.contains("keys") || tag.contains("keyboard") || tag.contains("organ")) return 3;
    if (tag.contains("bass")) return 1;
    if (tag.contains("synth") || tag.contains("lead") || tag.contains("melody")) return 5;
    if (tag.contains("chord") || tag.contains("pad") || tag.contains("string")) return 2;
    if (tag.contains("fx") || tag.contains("effect")) return 6;
    int lane = instrumentLane(tag); return lane >= 0 && lane <= 2 ? 0 : -1;
}
struct Combination {
    Arrangement arrangement;
    std::array<bool, 8> locked{};
    std::array<juce::String, 8> explanation;
    uint32_t seed = 42;
    bool audible() const { for (const auto& lane : arrangement.lanes) if (!lane.clips.empty()) return true; return false; }
};
inline juce::String sourceSummary(const Lane& lane) {
    juce::StringArray names; for (const auto& c : lane.clips) if (c.asset) names.addIfNotAlreadyThere(c.asset->source.getFileNameWithoutExtension());
    return names.joinIntoString(" + ");
}
inline bool audibleRegion(const Region& c) {
    if (!valid(c) || c.gain <= .00001f) return false;
    const auto first = static_cast<std::size_t>(c.offset * c.asset->rate / Asset::peakHop);
    const auto last = std::min(c.asset->peaks.size(), static_cast<std::size_t>((c.offset + c.duration * c.speed) * c.asset->rate / Asset::peakHop) + 1);
    for (auto i = first; i < last; ++i) if (c.asset->peaks[i] > .00001f) return true;
    return false;
}
// Uses Inspire's real sample decoding, timing, ranking and musical placements.
// Roles are adapted at this boundary; legacy tracks and project files stay valid.
inline Combination createCombination(const std::vector<Sample>& indexed, InspireSettings settings, const Combination& previous,
                                     int replacement, const Cancel& cancelled) {
    if (replacement < -1 || replacement > 7) throw std::runtime_error("Invalid layer replacement");
    Combination next = previous; next.seed = settings.seed; next.arrangement.bpm = settings.bpm; next.arrangement.loopEnd = settings.bars * 4; next.arrangement.loop = false;
    static constexpr int legacyLane[]{2, 3, 4, 5, 4, 5, 7, 6};
    static constexpr const char* aliases[]{"Drums", "Bass", "Chords", "Melody", "Guitar", "Melody", "FX", "Vocals"};
    for (int role = 0; role < 8; ++role) {
        if (cancelled()) throw std::runtime_error("Generation cancelled; previous combination retained");
        auto slot = static_cast<std::size_t>(role); next.arrangement.lanes[slot].name = layerNames[slot];
        if ((replacement >= 0 && replacement != role) || previous.locked[slot]) continue;
        std::vector<Sample> candidates, alternatives;
        juce::StringArray oldPaths, oldHashes;
        for (const auto& c : previous.arrangement.lanes[slot].clips) if (c.asset) oldPaths.addIfNotAlreadyThere(c.asset->source.getFullPathName());
        for (const auto& s : indexed) if (oldPaths.contains(s.path)) oldHashes.addIfNotAlreadyThere(s.hash);
        for (auto sample : indexed) {
            if (musicalRole(sample.instrument) != role || sample.rms <= .00001 || !juce::File(sample.path).existsAsFile()) continue;
            if ((settings.genre.isNotEmpty() && !sample.genre.equalsIgnoreCase(settings.genre)) || (settings.mood.isNotEmpty() && !sample.mood.equalsIgnoreCase(settings.mood))) continue;
            if (sample.bpm > 0 && (settings.bpm / sample.bpm < .8 || settings.bpm / sample.bpm > 1.25)) continue;
            if (role != 0 && role != 6 && settings.key >= 0 && sample.key >= 0 && compatibility(sample, 0, settings.key, settings.mode) < 0) continue;
            bool oldSource = oldPaths.contains(sample.path) || (sample.hash.isNotEmpty() && oldHashes.contains(sample.hash));
            if (replacement == role && oldSource) continue;
            if (role == 0 || role == 6) sample.key = -1; // unpitched roles do not claim a harmonic key
            else if (settings.key >= 0) {
                if (sample.key < 0) continue;
                // Linked resampling cannot retain the chosen key at another BPM.
                // One-shots keep their pitch; pitched loops need the target tempo.
                if (sample.duration >= .8 && sample.bpm > 0 && std::abs(sample.bpm - settings.bpm) > .01) continue;
                sample.bpm = 0;
            }
            sample.instrument = aliases[slot]; if (!oldSource) alternatives.push_back(sample); candidates.push_back(std::move(sample));
        }
        if (replacement < 0 && settings.seed != previous.seed && !alternatives.empty()) candidates = std::move(alternatives);
        // Prefer complete drum loops. One-shot drums still generate actual hits.
        if (role == 0 && std::any_of(candidates.begin(), candidates.end(), [](const auto& s) { return s.duration >= .8; }))
            std::erase_if(candidates, [](const auto& s) { return s.duration < .8; });
        auto roleSettings = settings; roleSettings.seed ^= static_cast<uint32_t>(role * 2654435761U);
        try {
            auto generated = inspire(candidates, roleSettings, cancelled, {}, legacyLane[slot]);
            Lane lane = generated.arrangement.lanes[static_cast<std::size_t>(legacyLane[slot])]; lane.name = layerNames[slot];
            std::erase_if(lane.clips, [](const auto& c) { return !audibleRegion(c); });
            if (lane.clips.empty()) throw std::runtime_error("No audible regions in selected bar range");
            next.arrangement.lanes[slot] = std::move(lane); next.explanation[slot] = sourceSummary(next.arrangement.lanes[slot]);
        } catch (const std::exception& e) {
            if (cancelled()) throw;
            next.explanation[slot] = "Missing " + juce::String(layerNames[slot]) + ": index audible audio with an instrument tag and matching genre, BPM/key. " + juce::String::fromUTF8(e.what());
            if (replacement < 0) { next.arrangement.lanes[slot].clips.clear(); }
            else next.explanation[slot] = previous.arrangement.lanes[slot].clips.empty() ? "Missing " + juce::String(layerNames[slot]) + ": import audible, instrument-tagged audio with matching genre, BPM/key." : "No alternative for " + juce::String(layerNames[slot]) + "; current audio retained. Import another compatible source.";
        }
    }
    std::size_t count = 0; for (const auto& lane : next.arrangement.lanes) count += lane.clips.size();
    if (count > Editor::maxClips) throw std::runtime_error("Combination exceeds 512 regions. Choose fewer bars.");
    return next;
}
inline juce::Result writeCombinationMidi(const juce::File& destination, const Arrangement& a) {
    if (a.bpm < 30 || a.bpm > 300 || !std::isfinite(a.bpm)) return juce::Result::fail("Invalid MIDI tempo");
    bool loaded = false; for (const auto& lane : a.lanes) for (const auto& c : lane.clips) { loaded = true; if (c.asset && c.asset->source == destination) return juce::Result::fail("MIDI must not overwrite source audio"); }
    if (!loaded) return juce::Result::fail("Generate audible layers before exporting MIDI");
    juce::MidiFile file; file.setTicksPerQuarterNote(960); juce::MidiMessageSequence tempo;
    tempo.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(60000000 / a.bpm))); tempo.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4, 4));
    tempo.addEvent(juce::MidiMessage::textMetaEvent(1, "Sample triggers only. Map notes 36-43 to the eight source layers; no audio or transcription is embedded.")); file.addTrack(tempo);
    for (int role = 0; role < 8; ++role) {
        juce::MidiMessageSequence events; events.addEvent(juce::MidiMessage::textMetaEvent(3, layerNames[static_cast<std::size_t>(role)]));
        for (const auto& c : a.lanes[static_cast<std::size_t>(role)].clips) {
            auto on = juce::MidiMessage::noteOn(10, 36 + role, static_cast<juce::uint8>(juce::jlimit(1, 127, static_cast<int>(c.gain * 100)))); on.setTimeStamp(c.beat * 960); events.addEvent(on);
            auto off = juce::MidiMessage::noteOff(10, 36 + role); off.setTimeStamp((c.beat + c.duration / a.secondsPerBeat()) * 960); events.addEvent(off);
        }
        events.updateMatchedPairs(); file.addTrack(events);
    }
    juce::TemporaryFile temp(destination);
    { auto stream = temp.getFile().createOutputStream(); if (!stream || !file.writeTo(*stream)) return juce::Result::fail("Cannot write trigger MIDI"); stream->flush(); if (stream->getStatus().failed()) return stream->getStatus(); }
    return temp.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot publish trigger MIDI");
}
}
