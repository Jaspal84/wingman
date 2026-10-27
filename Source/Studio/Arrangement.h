#pragma once
#include "Audio/Track.h"
#include <juce_data_structures/juce_data_structures.h>
#include <atomic>
#include <functional>
#include <limits>
#include <optional>
#include <unordered_map>

namespace wingman::studio {
// Immutable, disk-backed PCM. Import uses a fixed-size decode buffer, not a
// duration-sized AudioBuffer. The OS pages this read-only cache on demand.
struct Asset {
    juce::File source, cache;
    double rate = 44100;
    int channels = 0;
    int64_t frames = 0;
    static constexpr int peakHop = 2048;
    std::vector<float> peaks;
    std::unique_ptr<juce::MemoryMappedFile> mapping;
    ~Asset() { mapping.reset(); cache.deleteFile(); }
    double seconds() const { return frames / rate; }
    float sample(int channel, double frame) const {
        if (!mapping || frame < 0 || frame >= frames || !std::isfinite(frame)) return 0;
        auto i = static_cast<int64_t>(frame);
        auto j = std::min(i + 1, frames - 1);
        auto* pcm = static_cast<const float*>(mapping->getData());
        const int c = std::min(channel, channels - 1);
        float a = pcm[i * channels + c], b = pcm[j * channels + c];
        return a + static_cast<float>(frame - i) * (b - a);
    }
};
inline juce::Result importAsset(const juce::File& source, std::shared_ptr<const Asset>& output,
                               const std::function<bool()>& cancelled = [] { return false; }) {
    juce::AudioFormatManager formats; formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(source));
    if (!reader || reader->numChannels < 1 || reader->numChannels > 2 ||
        reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples < 1)
        return juce::Result::fail("Requires mono/stereo WAV, AIFF or FLAC, 8-192 kHz");
    auto asset = std::make_shared<Asset>();
    asset->source = source; asset->rate = reader->sampleRate;
    asset->channels = static_cast<int>(reader->numChannels); asset->frames = reader->lengthInSamples;
    if (asset->frames > (16LL * 1024 * 1024 * 1024) / (asset->channels * static_cast<int>(sizeof(float))))
        return juce::Result::fail("Source exceeds 16 GiB decoded");
    const int64_t bytes = asset->frames * asset->channels * static_cast<int64_t>(sizeof(float));
    auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory);
    if (bytes <= 0 || bytes > 16LL * 1024 * 1024 * 1024 || temp.getBytesFreeOnVolume() < bytes + 64 * 1024 * 1024)
        return juce::Result::fail("Insufficient cache space or source exceeds 16 GiB decoded");
    asset->cache = temp.getNonexistentChildFile("jaspro-pcm", ".cache", true);
    auto stream = asset->cache.createOutputStream();
    if (!stream || !stream->openedOk()) return juce::Result::fail("Cannot create PCM cache");
    juce::AudioBuffer<float> block(asset->channels, Asset::peakHop);
    std::vector<float> interleaved(static_cast<std::size_t>(Asset::peakHop * asset->channels));
    for (int64_t at = 0; at < asset->frames; at += Asset::peakHop) {
        if (cancelled()) return juce::Result::fail("Import cancelled");
        int n = static_cast<int>(std::min<int64_t>(Asset::peakHop, asset->frames - at));
        if (!reader->read(&block, 0, n, at, true, true)) return juce::Result::fail("Audio decode failed");
        float peak = 0;
        for (int i = 0; i < n; ++i) for (int c = 0; c < asset->channels; ++c) {
            auto value = block.getSample(c, i);
            if (!std::isfinite(value)) return juce::Result::fail("Non-finite audio rejected");
            interleaved[static_cast<std::size_t>(i * asset->channels + c)] = value;
            peak = std::max(peak, std::abs(value));
        }
        asset->peaks.push_back(peak);
        if (!stream->write(interleaved.data(), static_cast<std::size_t>(n * asset->channels) * sizeof(float)))
            return juce::Result::fail("PCM cache write failed");
    }
    stream->flush();
    if (stream->getStatus().failed()) return stream->getStatus();
    stream.reset();
    asset->mapping = std::make_unique<juce::MemoryMappedFile>(asset->cache, juce::MemoryMappedFile::readOnly);
    if (!asset->mapping->getData() || asset->mapping->getSize() != bytes) return juce::Result::fail("Cannot map PCM cache");
    output = std::move(asset); return juce::Result::ok();
}
struct Region {
    juce::String id = juce::Uuid().toString();
    std::shared_ptr<const Asset> asset;
    double beat = 0, offset = 0, duration = 0;
    float gain = 1;
    double fadeIn = 0, fadeOut = 0;
};
struct Lane { std::vector<Region> clips; float gain = .8f, pan = 0; bool mute = false, solo = false; };
struct Arrangement {
    std::array<Lane, 8> lanes;
    double bpm = 120, loopStart = 0, loopEnd = 16;
    bool loop = false;
    double secondsPerBeat() const { return 60 / bpm; }
    double end() const {
        double value = 0;
        for (const auto& lane : lanes) for (const auto& c : lane.clips)
            value = std::max(value, c.beat * secondsPerBeat() + c.duration);
        return value;
    }
    Region* find(const juce::String& id) {
        for (auto& lane : lanes) for (auto& c : lane.clips) if (c.id == id) return &c;
        return nullptr;
    }
};
inline bool valid(const Region& c) {
    return c.asset && !c.id.isEmpty() && std::isfinite(c.beat) && c.beat >= 0 && c.beat <= 1000000 &&
        std::isfinite(c.offset) && c.offset >= 0 && std::isfinite(c.duration) && c.duration > 0 &&
        c.offset + c.duration <= c.asset->seconds() + 1e-6 &&
        std::isfinite(c.gain) && c.gain >= 0 && c.gain <= 2 && std::isfinite(c.fadeIn) &&
        std::isfinite(c.fadeOut) && c.fadeIn >= 0 && c.fadeOut >= 0 && c.fadeIn <= c.duration && c.fadeOut <= c.duration;
}
// Whole-state commands share immutable assets. A drag is one transaction; history
// is bounded and never copies PCM or runs on the audio thread.
class Editor {
    std::vector<Arrangement> past, future;
public:
    Arrangement state;
    static constexpr std::size_t maxClips = 512;
    bool canUndo() const { return !past.empty(); }
    bool canRedo() const { return !future.empty(); }
    void commit(const Arrangement& before) {
        past.push_back(before); if (past.size() > 64) past.erase(past.begin()); future.clear();
    }
    bool undo() { if (!canUndo()) return false; future.push_back(state); state = std::move(past.back()); past.pop_back(); return true; }
    bool redo() { if (!canRedo()) return false; past.push_back(state); state = std::move(future.back()); future.pop_back(); return true; }
    void replace(Arrangement next) { state = std::move(next); past.clear(); future.clear(); }
    std::size_t count() const { std::size_t n = 0; for (const auto& lane : state.lanes) n += lane.clips.size(); return n; }
    bool add(int lane, Region c) {
        if (lane < 0 || lane >= 8 || !valid(c) || state.find(c.id) || count() >= maxClips) return false;
        auto before = state; state.lanes[static_cast<std::size_t>(lane)].clips.push_back(std::move(c)); commit(before); return true;
    }
    bool remove(const juce::String& id) {
        if (!state.find(id)) return false; auto before = state;
        for (auto& lane : state.lanes) std::erase_if(lane.clips, [&](const auto& c) { return c.id == id; });
        commit(before); return true;
    }
    bool move(const juce::String& id, int target, double beat) {
        auto* c = state.find(id);
        if (!c || target < 0 || target >= 8 || !std::isfinite(beat) || beat < 0 || beat > 1000000) return false;
        Region next = *c; auto before = state; next.beat = beat;
        for (auto& lane : state.lanes) std::erase_if(lane.clips, [&](const auto& r) { return r.id == id; });
        state.lanes[static_cast<std::size_t>(target)].clips.push_back(std::move(next)); commit(before); return true;
    }
    bool split(const juce::String& id, double beat) {
        auto* c = state.find(id); if (!c || count() >= maxClips) return false;
        double seconds = (beat - c->beat) * state.secondsPerBeat();
        if (seconds < .001 || seconds > c->duration - .001) return false;
        auto before = state; Region right = *c; right.id = juce::Uuid().toString();
        right.beat = beat; right.offset += seconds; right.duration -= seconds;
        right.fadeIn = 0; right.fadeOut = std::min(right.fadeOut, right.duration);
        for (auto& lane : state.lanes) for (auto& left : lane.clips) if (left.id == id) {
            left.duration = seconds; left.fadeOut = 0; left.fadeIn = std::min(left.fadeIn, seconds);
            lane.clips.push_back(std::move(right)); commit(before); return true;
        }
        return false;
    }
    bool trim(const juce::String& id, double leftSeconds, double rightSeconds) {
        auto* c = state.find(id); if (!c) return false; Region next = *c;
        next.beat += leftSeconds / state.secondsPerBeat(); next.offset += leftSeconds;
        next.duration += rightSeconds - leftSeconds;
        next.fadeIn = std::min(next.fadeIn, next.duration); next.fadeOut = std::min(next.fadeOut, next.duration);
        if (!valid(next)) return false; auto before = state; *c = next; commit(before); return true;
    }
};
// Single audio reader, serialized control-thread writer; retirement stays off audio.
class TimelineEngine {
    std::atomic<const Arrangement*> current{nullptr}, hazard{nullptr};
    std::vector<std::unique_ptr<Arrangement>> retained;
    std::atomic<double> requested{0}, visible{0};
    double cursor = 0;
public:
    std::atomic<bool> playing{false};
    TimelineEngine() { publish({}); }
    void publish(const Arrangement& state) {
        auto next = std::make_unique<Arrangement>(state); auto* p = next.get();
        retained.push_back(std::move(next)); current.store(p);
        const auto* active = hazard.load();
        std::erase_if(retained, [&](const auto& v) { return v.get() != p && v.get() != active; });
    }
    void seek(double seconds) { if (std::isfinite(seconds)) { requested.store(std::max(0.0, seconds)); visible.store(std::max(0.0, seconds)); } }
    double position() const { return visible.load(); }
    void render(juce::AudioBuffer<float>& output, double rate) {
        const Arrangement* a;
        do { a = current.load(); hazard.store(a); } while (current.load() != a);
        auto request = requested.exchange(std::numeric_limits<double>::quiet_NaN());
        if (std::isfinite(request)) cursor = request;
        if (playing.load() && rate > 0) {
            bool solo = false; for (const auto& lane : a->lanes) solo |= lane.solo;
            const double end = a->end(), spb = a->secondsPerBeat();
            const double loopStart = a->loopStart * spb, loopEnd = a->loopEnd * spb;
            for (int i = 0; i < output.getNumSamples(); ++i) {
                if (a->loop && loopEnd > loopStart && cursor >= loopEnd)
                    cursor = loopStart + std::fmod(cursor - loopStart, loopEnd - loopStart);
                if (!a->loop && cursor >= end) { playing = false; break; }
                for (const auto& lane : a->lanes) {
                    if (lane.mute || (solo && !lane.solo)) continue;
                    for (const auto& c : lane.clips) {
                        const double local = cursor - c.beat * spb;
                        if (local < 0 || local >= c.duration || !c.asset) continue;
                        double envelope = 1;
                        if (c.fadeIn > 0) envelope *= std::min(1.0, local / c.fadeIn);
                        if (c.fadeOut > 0) envelope *= std::min(1.0, (c.duration - local) / c.fadeOut);
                        for (int ch = 0; ch < output.getNumChannels(); ++ch)
                            output.addSample(ch, i, c.asset->sample(ch, (c.offset + local) * c.asset->rate) *
                                lane.gain * c.gain * static_cast<float>(envelope) * balanceGain(lane.pan, ch));
                    }
                }
                cursor += 1 / rate;
            }
        }
        visible.store(cursor);
        for (int c = 0; c < output.getNumChannels(); ++c) for (int i = 0; i < output.getNumSamples(); ++i) {
            float value = output.getSample(c, i); output.setSample(c, i, std::isfinite(value) ? std::clamp(value, -1.0f, 1.0f) : 0);
        }
        hazard.store(nullptr);
    }
};
inline juce::Result saveArrangement(const juce::File& file, const Arrangement& a) {
    for (const auto& lane : a.lanes) for (const auto& c : lane.clips)
        if (c.asset && c.asset->source == file) return juce::Result::fail("Cannot overwrite source audio with a project");
    juce::ValueTree root("JasProProject"); root.setProperty("version", 2, nullptr);
    root.setProperty("bpm", a.bpm, nullptr); root.setProperty("loop", a.loop, nullptr);
    root.setProperty("loopStart", a.loopStart, nullptr); root.setProperty("loopEnd", a.loopEnd, nullptr);
    for (const auto& lane : a.lanes) {
        juce::ValueTree t("Track"); t.setProperty("gain", lane.gain, nullptr); t.setProperty("pan", lane.pan, nullptr);
        t.setProperty("mute", lane.mute, nullptr); t.setProperty("solo", lane.solo, nullptr);
        for (const auto& c : lane.clips) {
            if (!valid(c)) return juce::Result::fail("Invalid clip");
            juce::ValueTree v("Clip"); v.setProperty("id", c.id, nullptr); v.setProperty("source", c.asset->source.getFullPathName(), nullptr);
            v.setProperty("beat", c.beat, nullptr); v.setProperty("offset", c.offset, nullptr); v.setProperty("duration", c.duration, nullptr);
            v.setProperty("gain", c.gain, nullptr); v.setProperty("fadeIn", c.fadeIn, nullptr); v.setProperty("fadeOut", c.fadeOut, nullptr);
            t.addChild(v, -1, nullptr);
        }
        root.addChild(t, -1, nullptr);
    }
    juce::TemporaryFile temp(file);
    return temp.getFile().replaceWithText(root.toXmlString()) && temp.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot save project");
}
inline juce::Result loadArrangement(const juce::File& file, Arrangement& destination,
                                  const std::function<bool()>& cancelled = [] { return false; }) {
    if (!file.existsAsFile() || file.getSize() > 4 * 1024 * 1024) return juce::Result::fail("Missing or oversized project");
    auto root = juce::ValueTree::fromXml(file.loadFileAsString()); int version = root["version"];
    if (!root.hasType("JasProProject") || (version != 1 && version != 2) || root.getNumChildren() != 8)
        return juce::Result::fail("Invalid project version or tracks");
    Arrangement next; next.loop = root["loop"];
    if (version == 2) { next.bpm = root["bpm"]; next.loopStart = root["loopStart"]; next.loopEnd = root["loopEnd"]; }
    if (!std::isfinite(next.bpm) || next.bpm < 30 || next.bpm > 300 || !std::isfinite(next.loopStart) ||
        !std::isfinite(next.loopEnd) || next.loopStart < 0 || next.loopEnd <= next.loopStart || next.loopEnd > 1000000)
        return juce::Result::fail("Invalid tempo or loop region");
    std::unordered_map<std::string, std::shared_ptr<const Asset>> assets;
    juce::StringArray ids; std::size_t count = 0;
    for (int i = 0; i < 8; ++i) {
        auto t = root.getChild(i); auto& lane = next.lanes[static_cast<std::size_t>(i)];
        if (!t.hasType("Track")) return juce::Result::fail("Invalid track");
        lane.gain = t["gain"]; lane.pan = t["pan"]; lane.mute = t["mute"]; lane.solo = t["solo"];
        if (!std::isfinite(lane.gain) || lane.gain < 0 || lane.gain > 1 || !std::isfinite(lane.pan) || std::abs(lane.pan) > 1)
            return juce::Result::fail("Invalid mixer");
        int children = version == 1 ? (t["source"].toString().isEmpty() ? 0 : 1) : t.getNumChildren();
        for (int j = 0; j < children; ++j) {
            if (cancelled()) return juce::Result::fail("Load cancelled");
            if (++count > Editor::maxClips) return juce::Result::fail("Project exceeds 512 clips");
            auto v = version == 1 ? t : t.getChild(j);
            if (version == 2 && !v.hasType("Clip")) return juce::Result::fail("Invalid clip node");
            auto path = v["source"].toString();
            if (!juce::File::isAbsolutePath(path)) return juce::Result::fail("Source path must be absolute");
            auto key = path.toStdString();
            if (!assets.count(key)) {
                std::shared_ptr<const Asset> asset; auto result = importAsset(juce::File(path), asset, cancelled);
                if (result.failed()) return result; assets[key] = std::move(asset);
            }
            Region c; c.asset = assets[key]; c.duration = c.asset->seconds();
            if (version == 2) {
                c.id = v["id"]; c.beat = v["beat"]; c.offset = v["offset"]; c.duration = v["duration"];
                c.gain = v["gain"]; c.fadeIn = v["fadeIn"]; c.fadeOut = v["fadeOut"];
            }
            if (!valid(c) || ids.contains(c.id)) return juce::Result::fail("Invalid or duplicate clip");
            ids.add(c.id); lane.clips.push_back(std::move(c));
        }
    }
    if (cancelled()) return juce::Result::fail("Load cancelled");
    destination = std::move(next); return juce::Result::ok();
}
inline juce::Result bounceArrangement(const juce::File& file, Arrangement snapshot, const std::function<bool()>& cancelled) {
    for (const auto& lane : snapshot.lanes) for (const auto& c : lane.clips)
        if (c.asset && c.asset->source == file) return juce::Result::fail("Export must not overwrite source audio");
    snapshot.loop = false; const double end = snapshot.end();
    if (end <= 0) return juce::Result::fail("Import audio before exporting");
    TimelineEngine engine; engine.publish(snapshot); engine.playing = true;
    juce::TemporaryFile temp(file); juce::WavAudioFormat wav;
    auto stream = temp.getFile().createOutputStream();
    if (!stream) return juce::Result::fail("Cannot create WAV");
    std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 48000, 2, 24, {}, 0));
    if (!writer) return juce::Result::fail("Cannot create WAV writer"); stream.release();
    juce::AudioBuffer<float> block(2, 512); const auto total = static_cast<int64_t>(std::ceil(end * 48000));
    for (int64_t at = 0; at < total; at += 512) {
        if (cancelled()) return juce::Result::fail("Export cancelled");
        block.clear(); engine.render(block, 48000);
        if (!writer->writeFromAudioSampleBuffer(block, 0, static_cast<int>(std::min<int64_t>(512, total - at)))) return juce::Result::fail("WAV write failed");
    }
    writer.reset(); if (cancelled()) return juce::Result::fail("Export cancelled");
    return temp.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot publish WAV");
}
}
