#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <limits>
namespace {
constexpr double maxClipSeconds = 120.0;
class Sound final : public juce::SynthesiserSound {
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};
class Voice final : public juce::SynthesiserVoice {
    double phase = 0, delta = 0;
    float level = 0;
    juce::ADSR envelope;
public:
    using juce::SynthesiserVoice::renderNextBlock;
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void startNote(int note, float velocity, juce::SynthesiserSound*, int) override {
        phase = 0;
        delta = juce::MathConstants<double>::twoPi * juce::MidiMessage::getMidiNoteInHertz(note) / getSampleRate();
        level = velocity * 0.12f;
        envelope.setSampleRate(getSampleRate());
        envelope.setParameters({0.01f, 0.12f, 0.65f, 0.02f});
        envelope.noteOn();
    }
    void stopNote(float, bool tail) override { if (tail) envelope.noteOff(); else { envelope.reset(); clearCurrentNote(); } }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int count) override {
        while (--count >= 0) {
            const float value = static_cast<float>(std::sin(phase)) * level * envelope.getNextSample();
            phase = std::fmod(phase + delta, juce::MathConstants<double>::twoPi);
            for (int ch = 0; ch < out.getNumChannels(); ++ch) out.addSample(ch, start, value);
            ++start;
            if (!envelope.isActive()) { clearCurrentNote(); break; }
        }
    }
};
bool validTrack(int i) { return i >= 0 && i < 4; }
}
WingmanProcessor::WingmanProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    for (int i = 0; i < 16; ++i) synth.addVoice(new Voice);
    synth.addSound(new Sound);
    setProgression(chordText);
}
bool WingmanProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto out = layout.getMainOutputChannelSet();
    const auto in = layout.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in.isDisabled() || in == out);
}
void WingmanProcessor::prepareToPlay(double rate, int) {
    const juce::ScopedLock lock(mutex);
    engineRate = rate;
    synth.setCurrentPlaybackSampleRate(rate);
    // Reserve once outside the audio callback. Recording is capped at two minutes.
    recordBuffer.setSize(2, static_cast<int>(rate * maxClipSeconds));
    recordSamples = 0; recording = false; panic = true;
}
void WingmanProcessor::releaseResources() {
    const juce::ScopedLock lock(mutex);
    synth.allNotesOff(0, false);
    playing = recording = false;
}
void WingmanProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const juce::ScopedTryLock lock(mutex);
    if (!lock.isLocked()) { audio.clear(); midi.clear(); return; }
    const int count = audio.getNumSamples();
    if (recording) {
        const int n = std::min(count, recordBuffer.getNumSamples() - recordSamples);
        const int inputs = std::min(getTotalNumInputChannels(), audio.getNumChannels());
        for (int ch = 0; ch < 2; ++ch)
            if (inputs > 0) recordBuffer.copyFrom(ch, recordSamples, audio, std::min(ch, inputs-1), 0, n);
            else recordBuffer.clear(ch, recordSamples, n);
        recordSamples += n;
    }
    audio.clear();
    if (panic) {
        synth.allNotesOff(0, false);
        for (int ch = 1; ch <= 16; ++ch) midi.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
        panic = false;
    }
    if (playing) {
        bool anySolo = false;
        for (const auto& track : tracks) anySolo |= track.solo;
        for (const auto& track : tracks) {
            if (!track.clip || track.mute || (anySolo && !track.solo)) continue;
            const auto& clip = *track.clip;
            for (int i = 0; i < count; ++i) {
                const double sourcePosition = (seconds + i / engineRate) * clip.sampleRate;
                for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                    audio.addSample(ch, i, wingman::interpolate(clip, ch, sourcePosition) * track.gain * wingman::balanceGain(track.pan,ch));
            }
        }
        if (generated && !chords.empty()) {
            const double startBeat = seconds * bpm / 60.0;
            const double endBeat = (seconds + count / engineRate) * bpm / 60.0;
            const double length = chords.size() * 4.0;
            const auto firstLoop = static_cast<juce::int64>(std::floor(startBeat / length));
            const auto lastLoop = static_cast<juce::int64>(std::floor(endBeat / length));
            for (auto loop = firstLoop; loop <= lastLoop; ++loop)
                for (int i = 0; i < sequence.getNumEvents(); ++i) {
                    const auto& msg = sequence.getEventPointer(i)->message;
                    const double beat = loop * length + msg.getTimeStamp();
                    if (beat >= startBeat && beat < endBeat) {
                        const int offset = juce::jlimit(0, count-1, static_cast<int>(std::llround((beat-startBeat)*60.0/bpm*engineRate)));
                        midi.addEvent(msg, offset);
                    }
                }
        }
        seconds += count / engineRate;
    }
    if (previewEnabled) synth.renderNextBlock(audio, midi, 0, count);
}
void WingmanProcessor::play() { const juce::ScopedLock lock(mutex); playing = true; }
void WingmanProcessor::stop() { const juce::ScopedLock lock(mutex); playing = false; seconds = 0; panic = true; }
bool WingmanProcessor::isPlaying() const { const juce::ScopedLock lock(mutex); return playing; }
double WingmanProcessor::positionSeconds() const { const juce::ScopedLock lock(mutex); return seconds; }
void WingmanProcessor::setTempo(double value) { const juce::ScopedLock lock(mutex); if (std::isfinite(value)) { bpm = juce::jlimit(40.0,240.0,value); panic = true; seconds = 0; } }
double WingmanProcessor::tempo() const { const juce::ScopedLock lock(mutex); return bpm; }
void WingmanProcessor::setKey(int value) { const juce::ScopedLock lock(mutex); transpose = juce::jlimit(0,11,value); rebuildMidi(); }
int WingmanProcessor::key() const { const juce::ScopedLock lock(mutex); return transpose; }
void WingmanProcessor::setPreview(bool enabled) { const juce::ScopedLock lock(mutex); previewEnabled = enabled; panic = true; }
bool WingmanProcessor::preview() const { const juce::ScopedLock lock(mutex); return previewEnabled; }
bool WingmanProcessor::midiIsGenerated() const { const juce::ScopedLock lock(mutex); return generated; }
wingman::Track WingmanProcessor::trackSnapshot(int i) const { const juce::ScopedLock lock(mutex); return validTrack(i) ? tracks[static_cast<std::size_t>(i)] : wingman::Track{}; }
void WingmanProcessor::setTrackMix(int i, float gain, bool mute, bool solo) {
    if (!validTrack(i)) return;
    const juce::ScopedLock lock(mutex);
    auto& t = tracks[static_cast<std::size_t>(i)];
    t.gain = std::isfinite(gain) ? juce::jlimit(0.0f,1.0f,gain) : 0; t.mute = mute; t.solo = solo;
}
void WingmanProcessor::setTrackPan(int i, float pan) {
    if (!validTrack(i) || !std::isfinite(pan)) return;
    const juce::ScopedLock lock(mutex); tracks[static_cast<std::size_t>(i)].pan = juce::jlimit(-1.0f,1.0f,pan);
}
void WingmanProcessor::setEditorSize(int width, int height) {
    const juce::ScopedLock lock(mutex); editorWidth = juce::jlimit(960,1920,width); editorHeight = juce::jlimit(608,1216,height);
}
std::pair<int,int> WingmanProcessor::editorSize() const { const juce::ScopedLock lock(mutex); return {editorWidth,editorHeight}; }
juce::Result WingmanProcessor::importWav(int i, const juce::File& file) {
    if (!validTrack(i)) return juce::Result::fail("Invalid track");
    juce::WavAudioFormat format;
    auto stream = file.createInputStream();
    if (!stream) return juce::Result::fail("Cannot open WAV file");
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(stream.release(), true));
    if (!reader) return juce::Result::fail("Cannot read WAV file");
    if (reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples <= 0
        || reader->lengthInSamples > reader->sampleRate * maxClipSeconds || reader->numChannels < 1)
        return juce::Result::fail("WAV must be 8-192 kHz and at most 120 seconds");
    auto clip = std::make_shared<wingman::Clip>();
    clip->sampleRate = reader->sampleRate; clip->name = file.getFileName();
    clip->audio.setSize(2, static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&clip->audio, 0, clip->audio.getNumSamples(), 0, true, true)) return juce::Result::fail("WAV decoding failed");
    for (int ch = 0; ch < 2; ++ch) for (int sample = 0; sample < clip->audio.getNumSamples(); ++sample)
        if (!std::isfinite(clip->audio.getSample(ch,sample))) return juce::Result::fail("WAV contains nonfinite samples");
    const juce::ScopedLock lock(mutex);
    tracks[static_cast<std::size_t>(i)].clip = std::move(clip);
    return juce::Result::ok();
}
juce::String WingmanProcessor::detectTempo(int i) {
    auto t = trackSnapshot(i);
    if (!t.clip) return "Import or record a WAV first";
    auto detected = wingman::detectBpm(t.clip->audio.getReadPointer(0), t.clip->audio.getNumSamples(), t.clip->sampleRate);
    if (!detected) return "No reliable pulse found (requires 3+ seconds of rhythmic audio)";
    setTempo(*detected);
    return "Estimated " + juce::String(*detected,1) + " BPM; half/double tempo ambiguity is possible";
}
juce::Result WingmanProcessor::setProgression(const juce::String& text) {
    const auto tokens = juce::StringArray::fromTokens(text, " ,|\t\n", "");
    std::vector<wingman::Chord> parsed;
    for (const auto& token : tokens) {
        if (token.isEmpty()) continue;
        auto chord = wingman::parseChord(token.toStdString());
        if (!chord) return juce::Result::fail("Use major/minor chord names: C Am F G, F# or Bb");
        parsed.push_back(*chord);
    }
    if (parsed.empty() || parsed.size() > 16) return juce::Result::fail("Enter 1-16 chords, one bar per chord");
    const juce::ScopedLock lock(mutex);
    chords = std::move(parsed); chordText = text.trim(); rebuildMidi();
    return juce::Result::ok();
}
juce::String WingmanProcessor::progression() const { const juce::ScopedLock lock(mutex); return chordText; }
void WingmanProcessor::rebuildMidi() { sequence = wingman::makeChords(chords, transpose); panic = true; seconds = 0; }
void WingmanProcessor::generateMidi() { const juce::ScopedLock lock(mutex); generated = true; rebuildMidi(); }
juce::Result WingmanProcessor::beginRecording(int i) {
    const juce::ScopedLock lock(mutex);
    if (!validTrack(i) || recordBuffer.getNumSamples() == 0) return juce::Result::fail("Audio device must be running");
    if (recording) return juce::Result::fail("Stop the current recording first");
    recordSamples = 0; recordTrack = i; recording = true;
    return juce::Result::ok();
}
bool WingmanProcessor::isRecording() const { const juce::ScopedLock lock(mutex); return recording; }
juce::Result WingmanProcessor::finishRecording() {
    const juce::ScopedLock lock(mutex);
    if (!recording) return juce::Result::fail("No recording in progress");
    recording = false;
    if (!recordSamples) return juce::Result::fail("No input samples captured");
    auto clip = std::make_shared<wingman::Clip>();
    clip->audio.setSize(2, recordSamples);
    for (int ch = 0; ch < 2; ++ch) clip->audio.copyFrom(ch,0,recordBuffer,ch,0,recordSamples);
    clip->sampleRate = engineRate; clip->name = "Recorded input";
    tracks[static_cast<std::size_t>(recordTrack)].clip = std::move(clip);
    return juce::Result::ok();
}
juce::Result WingmanProcessor::exportMidi(const juce::File& file) {
    juce::MidiMessageSequence events;
    {
        const juce::ScopedLock lock(mutex);
        if (!generated) return juce::Result::fail("Generate MIDI first");
        events.addEvent(juce::MidiMessage::tempoMetaEvent(static_cast<int>(60000000/bpm)));
        events.addEvent(juce::MidiMessage::timeSignatureMetaEvent(4,4));
        for (int i = 0; i < sequence.getNumEvents(); ++i) {
            auto msg = sequence.getEventPointer(i)->message;
            msg.setTimeStamp(msg.getTimeStamp()*960.0); events.addEvent(msg);
        }
        auto end = juce::MidiMessage::endOfTrack(); end.setTimeStamp(chords.size()*4.0*960); events.addEvent(end);
    }
    juce::MidiFile midi; midi.setTicksPerQuarterNote(960); midi.addTrack(events);
    juce::TemporaryFile temporary(file);
    { juce::FileOutputStream out(temporary.getFile());
      if (!out.openedOk() || !midi.writeTo(out)) return juce::Result::fail("Cannot write MIDI");
      out.flush(); if (out.getStatus().failed()) return out.getStatus(); }
    return temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot replace destination");
}
juce::Result WingmanProcessor::exportWav(const juce::File& file, int track) {
    std::array<wingman::Track,4> snapshot;
    { const juce::ScopedLock lock(mutex); snapshot = tracks; }
    if (track >= 0) {
        if (!validTrack(track)) return juce::Result::fail("Invalid track");
        for (int i = 0; i < 4; ++i) { auto& t = snapshot[static_cast<std::size_t>(i)]; t.solo = false; t.mute = i != track; }
    }
    double duration = 0; bool solo = false;
    for (const auto& t : snapshot) solo |= t.solo;
    for (const auto& t : snapshot) if (t.clip && !t.mute && (!solo || t.solo)) duration = std::max(duration, t.clip->audio.getNumSamples()/t.clip->sampleRate);
    if (duration <= 0) return juce::Result::fail("No audible audio tracks to export");
    juce::TemporaryFile temporary(file);
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(temporary.getFile()),48000,2,24,{},0));
        if (!writer) return juce::Result::fail("Cannot write WAV");
        juce::AudioBuffer<float> block(2,1024);
        const auto total = static_cast<juce::int64>(std::ceil(duration * 48000));
        for (juce::int64 pos = 0; pos < total; pos += 1024) {
            const int n = static_cast<int>(std::min<juce::int64>(1024,total-pos)); block.clear();
            for (const auto& t : snapshot) if (t.clip && !t.mute && (!solo || t.solo))
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i)
                    block.addSample(ch,i,wingman::interpolate(*t.clip,ch,(pos+i)*t.clip->sampleRate/48000.0)*t.gain*wingman::balanceGain(t.pan,ch));
            // Saturate PCM mixdown instead of wrapping on overload.
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) block.setSample(ch,i,juce::jlimit(-1.0f,1.0f,block.getSample(ch,i)));
            if (!writer->writeFromAudioSampleBuffer(block,0,n)) return juce::Result::fail("WAV write failed");
        }
        if (!writer->flush()) return juce::Result::fail("WAV flush failed");
    }
    return temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot replace destination");
}
void WingmanProcessor::getStateInformation(juce::MemoryBlock& destination) {
    juce::ValueTree state("Wingman");
    const juce::ScopedLock lock(mutex);
    state.setProperty("width",editorWidth,nullptr); state.setProperty("height",editorHeight,nullptr);
    state.setProperty("version",1,nullptr); state.setProperty("bpm",bpm,nullptr); state.setProperty("key",transpose,nullptr);
    state.setProperty("chords",chordText,nullptr); state.setProperty("generated",generated,nullptr); state.setProperty("preview",previewEnabled,nullptr);
    for (const auto& t : tracks) {
        juce::ValueTree node("Track");
        node.setProperty("pan",t.pan,nullptr); node.setProperty("gain",t.gain,nullptr); node.setProperty("mute",t.mute,nullptr); node.setProperty("solo",t.solo,nullptr);
        if (t.clip) {
            node.setProperty("name",t.clip->name,nullptr); node.setProperty("rate",t.clip->sampleRate,nullptr);
            node.setProperty("samples",t.clip->audio.getNumSamples(),nullptr);
            for (int ch = 0; ch < 2; ++ch) {
                juce::MemoryBlock bytes(t.clip->audio.getReadPointer(ch),static_cast<std::size_t>(t.clip->audio.getNumSamples())*sizeof(float));
                node.setProperty(ch == 0 ? "left" : "right",juce::var(bytes),nullptr);
            }
        }
        state.addChild(node,-1,nullptr);
    }
    juce::MemoryOutputStream stream(destination,false); state.writeToStream(stream);
}
void WingmanProcessor::setStateInformation(const void* data, int size) {
    auto state = juce::ValueTree::readFromData(data,static_cast<std::size_t>(std::max(0,size)));
    if (!state.hasType("Wingman") || static_cast<int>(state["version"]) != 1 || state.getNumChildren() != 4) return;
    std::array<wingman::Track,4> restored;
    for (int i = 0; i < 4; ++i) {
        auto node = state.getChild(i); auto& t = restored[static_cast<std::size_t>(i)];
        float gain = node["gain"]; if (!std::isfinite(gain)) return;
        const float pan = node.getProperty("pan",0.0); if (!std::isfinite(pan)) return;
        t.pan = juce::jlimit(-1.0f,1.0f,pan); t.gain = juce::jlimit(0.0f,1.0f,gain); t.mute = node["mute"]; t.solo = node["solo"];
        if (node.hasProperty("samples")) {
            const int n = node["samples"]; const double rate = node["rate"];
            if (!std::isfinite(rate) || rate < 8000 || rate > 192000 || n <= 0 || n > rate*maxClipSeconds) return;
            auto clip = std::make_shared<wingman::Clip>(); clip->sampleRate = rate; clip->name = node["name"].toString(); clip->audio.setSize(2,n);
            for (int ch = 0; ch < 2; ++ch) {
                const auto* bytes = node[ch == 0 ? "left" : "right"].getBinaryData();
                if (!bytes || bytes->getSize() != static_cast<std::size_t>(n)*sizeof(float)) return;
                std::memcpy(clip->audio.getWritePointer(ch), bytes->getData(), bytes->getSize());
                for (int s = 0; s < n; ++s) if (!std::isfinite(clip->audio.getSample(ch,s))) return;
            }
            t.clip = std::move(clip);
        }
    }
    const double restoredBpm = state["bpm"];
    if (!std::isfinite(restoredBpm)) return;
    const juce::ScopedLock lock(mutex);
    if (setProgression(state["chords"].toString()).failed()) return;
    setEditorSize(state.getProperty("width",1200),state.getProperty("height",760));
    tracks = std::move(restored); bpm = juce::jlimit(40.0,240.0,restoredBpm); transpose = juce::jlimit(0,11,static_cast<int>(state["key"]));
    generated = state["generated"]; previewEnabled = state["preview"]; playing = recording = false; rebuildMidi();
}
juce::AudioProcessorEditor* WingmanProcessor::createEditor() { return new WingmanEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WingmanProcessor; }
