#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>
#include "Studio/Arrangement.h"
#include "Studio/Library/LibraryPanel.h"
#include "Studio/ProductionPanel.h"
#include "Studio/InstrumentPanel.h"
#include "Studio/EffectsPanel.h"
#include "Studio/Cloud/CloudPanel.h"
#include "Music/GuitarRenderer.h"
#include "MIDI/PatternSequence.h"
#include "GUI/Theme.h"
#include "Threading/BackgroundThread.h"

namespace wingman {
class Studio final : public juce::AudioAppComponent, public juce::DragAndDropContainer, private juce::MidiInputCallback, private juce::Timer {
    Theme theme;
    studio::Editor editor;
    studio::TimelineEngine engine, previewEngine;
    juce::ComboBox workspace;
    studio::LibraryPanel browser;
    studio::ProductionPanel production;
    studio::InstrumentPanel instruments;
    studio::EffectsPanel effects;
    studio::CloudPanel cloudPanel{browser.databaseFile()};
    studio::InstrumentEngine instrumentEngine;
    std::unique_ptr<juce::MidiInput> midiInput;
    std::vector<studio::Region> sliceMidi;
    juce::String selected;
    std::optional<studio::Region> clipboard;
    int selectedLane = 0;
    double deviceRate = 44100, pixelsPerBeat = 32;
    bool busy = false, syncing = false;
    std::unique_ptr<juce::FileChooser> chooser;
    BackgroundThread worker;
    juce::Viewport viewport;
    const std::array<juce::String,8> names{"Kick", "Snare", "Hi-Hat", "Bass", "Chords", "Melody", "Vocals", "FX"};
    const std::array<juce::Colour,8> colours{juce::Colour(0xfff3a65a), juce::Colour(0xfff57782), juce::Colour(0xffe4cc70), juce::Colour(0xff69c78e), juce::Colour(0xff55c5d3), juce::Colour(0xff889cf0), juce::Colour(0xffc38ce3), juce::Colour(0xffc9a38b)};
    juce::TextButton play{"Play"}, stop{"Stop"}, loop{"Loop"}, devices{"Audio device"}, bounce{"Export WAV"}, saveProject{"Save"}, openProject{"Open"};
    juce::TextButton undo{"Undo"}, redo{"Redo"}, split{"Split [S]"}, duplicate{"Duplicate"}, copy{"Copy"}, paste{"Paste"}, remove{"Delete"};
    juce::ToggleButton snap{"Snap"};
    juce::ComboBox grid;
    juce::Slider bpm, zoom, loopStart, loopEnd;
    juce::Label status, position, inspector;
    std::array<juce::TextButton,8> imports, mutes, solos;
    std::array<juce::Slider,8> gains, pans;
    std::array<juce::Slider,6> properties;
    const std::array<juce::String,6> propertyNames{"Start beat (0-based)", "Source offset (s)", "Length (s)", "Clip gain", "Fade in (s)", "Fade out (s)"};
    int rowHeight = 60, dockWidth = 355;
    int dockOffset() const { return workspace.getSelectedId() == 7 ? dockWidth + 18 : 0; }
    static constexpr int rulerHeight = 32;
    double snapped(double beat) const {
        if (!snap.getToggleState()) return std::max(0.0, beat);
        double step = grid.getSelectedId() == 1 ? .25 : grid.getSelectedId() == 2 ? 1 : 4;
        return std::max(0.0, std::round(beat / step) * step);
    }
    int laneFor(const juce::String& id) const {
        for (int i = 0; i < 8; ++i) for (const auto& c : editor.state.lanes[static_cast<std::size_t>(i)].clips) if (c.id == id) return i;
        return selectedLane;
    }
    class Timeline final : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget {
        Studio& owner;
        studio::Arrangement before;
        studio::Region original;
        int mode = 0, originalLane = 0;
        bool changed = false;
    public:
        explicit Timeline(Studio& s) : owner(s) { setMouseCursor(juce::MouseCursor::CrosshairCursor); }
        juce::Rectangle<float> bounds(const studio::Region& c, int lane) const {
            return {static_cast<float>(c.beat * owner.pixelsPerBeat), static_cast<float>(rulerHeight + lane * owner.rowHeight + 5),
                static_cast<float>(c.duration / owner.editor.state.secondsPerBeat() * owner.pixelsPerBeat), static_cast<float>(owner.rowHeight - 10)};
        }
        void paint(juce::Graphics& g) override {
            g.fillAll(juce::Colour(0xff121722));
            auto clipBounds = g.getClipBounds();
            const auto& a = owner.editor.state;
            for (int i = 0; i < 8; ++i) {
                g.setColour(i % 2 ? juce::Colour(0xff19202c) : juce::Colour(0xff161c27));
                g.fillRect(0, rulerHeight + i * owner.rowHeight, getWidth(), owner.rowHeight - 1);
            }
            g.setColour(juce::Colour(0xff262e40)); g.fillRect(0, 0, getWidth(), rulerHeight);
            int stride = owner.pixelsPerBeat < 16 ? 4 : 1;
            int first = std::max(0, static_cast<int>(clipBounds.getX() / owner.pixelsPerBeat) / stride * stride);
            int last = static_cast<int>(clipBounds.getRight() / owner.pixelsPerBeat) + stride;
            for (int beat = first; beat <= last; beat += stride) {
                int x = static_cast<int>(beat * owner.pixelsPerBeat);
                g.setColour(juce::Colour(beat % 4 == 0 ? 0xff3c475c : 0xff242e3c));
                g.drawVerticalLine(x, 0, static_cast<float>(getHeight()));
                if (beat % 4 == 0) { g.setColour(juce::Colours::lightgrey); g.setFont(12); g.drawText(juce::String(beat / 4 + 1), x + 5, 2, 60, 20, juce::Justification::left); }
            }
            if (a.loop) {
                g.setColour(juce::Colour(0x6664d9c1));
                g.fillRect(static_cast<float>(a.loopStart * owner.pixelsPerBeat), 24.0f, static_cast<float>((a.loopEnd - a.loopStart) * owner.pixelsPerBeat), 7.0f);
            }
            for (int lane = 0; lane < 8; ++lane) for (const auto& c : a.lanes[static_cast<std::size_t>(lane)].clips) {
                auto r = bounds(c, lane); if (!r.intersects(clipBounds.toFloat())) continue;
                const auto colour = owner.colours[static_cast<std::size_t>(lane)];
                g.setColour(colour.withAlpha(c.id == owner.selected ? .65f : .32f)); g.fillRoundedRectangle(r, 4);
                g.setColour(c.id == owner.selected ? juce::Colours::white : colour); g.drawRoundedRectangle(r, 4, c.id == owner.selected ? 2.0f : 1.0f);
                auto audio = r.reduced(2, 2); audio.removeFromTop(17);
                if (c.asset) {
                    g.saveState(); g.reduceClipRegion(audio.toNearestInt()); g.setColour(colour.brighter(.2f));
                    int left = std::max(clipBounds.getX(), static_cast<int>(audio.getX()));
                    int right = std::min(clipBounds.getRight(), static_cast<int>(audio.getRight()));
                    for (int x = left; x < right; ++x) {
                        double local = (x - r.getX()) / owner.pixelsPerBeat * a.secondsPerBeat();
                        double sourceTime = c.reverse ? c.offset + (c.duration - local) * c.speed : c.offset + local * c.speed;
                        auto index = static_cast<std::size_t>(sourceTime * c.asset->rate / studio::Asset::peakHop);
                        float peak = index < c.asset->peaks.size() ? c.asset->peaks[index] : 0;
                        float h = std::min(1.0f, peak * c.gain) * audio.getHeight() * .45f;
                        g.drawVerticalLine(x, audio.getCentreY() - h, audio.getCentreY() + h);
                    }
                    g.restoreState();
                    g.setColour(juce::Colours::white); g.setFont(12);
                    g.drawText(c.asset->source.getFileName(), r.toNearestInt().reduced(7, 1).withHeight(18), juce::Justification::left, true);
                    g.setColour(juce::Colours::white.withAlpha(.6f));
                    float in = static_cast<float>(c.fadeIn / a.secondsPerBeat() * owner.pixelsPerBeat);
                    float out = static_cast<float>(c.fadeOut / a.secondsPerBeat() * owner.pixelsPerBeat);
                    g.drawLine(r.getX(), r.getBottom(), r.getX() + in, r.getY(), 1);
                    g.drawLine(r.getRight() - out, r.getY(), r.getRight(), r.getBottom(), 1);
                }
            }
            g.setColour(juce::Colour(0xffffdf90));
            g.drawVerticalLine(static_cast<int>(owner.engine.position() / a.secondsPerBeat() * owner.pixelsPerBeat), 0, static_cast<float>(getHeight()));
        }
        void mouseDown(const juce::MouseEvent& e) override {
            if (owner.isShowing()) owner.grabKeyboardFocus(); changed = false; mode = 0;
            if (owner.busy) return;
            double beat = owner.snapped(e.x / owner.pixelsPerBeat);
            if (e.y < rulerHeight) { owner.engine.seek(beat * owner.editor.state.secondsPerBeat()); return; }
            int lane = juce::jlimit(0, 7, (e.y - rulerHeight) / owner.rowHeight); owner.selectedLane = lane;
            auto& clips = owner.editor.state.lanes[static_cast<std::size_t>(lane)].clips;
            for (auto it = clips.rbegin(); it != clips.rend(); ++it) {
                auto r = bounds(*it, lane);
                if (!r.contains(e.position)) continue;
                owner.selected = it->id; original = *it; originalLane = lane; before = owner.editor.state;
                mode = e.x < r.getX() + 7 ? 2 : e.x > r.getRight() - 7 ? 3 : 1;
                owner.syncInspector(); repaint(); return;
            }
            owner.selected.clear(); owner.engine.seek(beat * owner.editor.state.secondsPerBeat()); owner.syncInspector(); repaint();
        }
        void mouseDrag(const juce::MouseEvent& e) override {
            if (!mode || owner.busy) return;
            double delta = e.getDistanceFromDragStartX() / owner.pixelsPerBeat;
            auto next = original; int lane = originalLane;
            if (mode == 1) { next.beat = owner.snapped(original.beat + delta); lane = juce::jlimit(0, 7, (e.y - rulerHeight) / owner.rowHeight); }
            else {
                const double spb = before.secondsPerBeat();
                if (mode == 2) {
                    double left = owner.snapped(original.beat + delta) - original.beat;
                    next.beat += left; if (!next.reverse) next.offset += left * spb * next.speed; next.duration -= left * spb;
                } else {
                    double end = owner.snapped(original.beat + original.duration / spb + delta);
                    next.duration = (end - original.beat) * spb;
                    if (next.reverse) next.offset -= (next.duration - original.duration) * next.speed;
                }
                next.fadeIn = std::min(next.fadeIn, next.duration); next.fadeOut = std::min(next.fadeOut, next.duration);
            }
            if (!studio::valid(next)) return;
            owner.editor.state = before;
            for (auto& track : owner.editor.state.lanes) std::erase_if(track.clips, [&](const auto& c) { return c.id == original.id; });
            owner.editor.state.lanes[static_cast<std::size_t>(lane)].clips.push_back(next);
            changed = true; owner.engine.publish(owner.editor.state); owner.syncInspector(); repaint();
        }
        void mouseUp(const juce::MouseEvent&) override {
            if (changed) { owner.editor.commit(before); owner.refresh(); }
            mode = 0; changed = false;
        }
        bool isInterestedInDragSource(const SourceDetails& details) override { return juce::File(details.description.toString()).hasFileExtension("wav;aif;aiff;flac"); }
        void itemDropped(const SourceDetails& details) override { filesDropped({details.description.toString()}, details.localPosition.x, details.localPosition.y); }
        bool isInterestedInFileDrag(const juce::StringArray& files) override {
            return files.size() == 1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac");
        }
        void filesDropped(const juce::StringArray& files, int x, int y) override {
            if (!owner.busy && y >= rulerHeight && isInterestedInFileDrag(files))
                owner.load(juce::File(files[0]), juce::jlimit(0, 7, (y - rulerHeight) / owner.rowHeight), owner.snapped(x / owner.pixelsPerBeat));
        }
    } timeline{*this};
    class DockDivider final : public juce::Component {
        Studio& owner; int start = 0;
    public:
        explicit DockDivider(Studio& s) : owner(s) { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); }
        void mouseDown(const juce::MouseEvent&) override { start = owner.dockWidth; }
        void mouseDrag(const juce::MouseEvent& e) override { owner.dockWidth = juce::jlimit(300, 500, start + e.getDistanceFromDragStartX()); owner.resized(); owner.repaint(); }
        void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff3b526c)); }
    } divider{*this};
    void message(juce::String text) { status.setText(text, juce::dontSendNotification); }
    void syncInspector() {
        syncing = true; instruments.setLoading(busy); effects.setEnabled(!busy);
        for (auto* button : {&loop, &saveProject, &openProject, &bounce}) button->setEnabled(!busy);
        for (auto* slider : {&bpm, &loopStart, &loopEnd}) slider->setEnabled(!busy);
        for (std::size_t i = 0; i < 8; ++i) {
            imports[i].setEnabled(!busy); mutes[i].setEnabled(!busy); solos[i].setEnabled(!busy);
            gains[i].setEnabled(!busy); pans[i].setEnabled(!busy);
        }
        auto* c = editor.state.find(selected);
        inspector.setText(c ? "CLIP  /  " + c->asset->source.getFileName() : "ARRANGEMENT  /  Drop audio onto a track | Drag body to move | Drag edges to trim", juce::dontSendNotification);
        for (auto& p : properties) p.setEnabled(c && !busy);
        if (c) {
            properties[0].setRange(0, std::max(64.0, editor.state.end() / editor.state.secondsPerBeat() + 64), .001);
            properties[1].setRange(0, std::max(.001, c->asset->seconds() - c->duration * c->speed), .001);
            properties[2].setRange(.001, std::max(.002, (c->asset->seconds() - c->offset) / c->speed), .001);
            properties[4].setRange(0, c->duration, .001); properties[5].setRange(0, c->duration, .001);
            const double values[]{c->beat, c->offset, c->duration, c->gain, c->fadeIn, c->fadeOut};
            for (std::size_t i = 0; i < properties.size(); ++i) properties[i].setValue(values[i], juce::dontSendNotification);
        }
        bpm.setValue(editor.state.bpm, juce::dontSendNotification);
        loop.setToggleState(editor.state.loop, juce::dontSendNotification);
        double loopRange = std::max({64.0, editor.state.end() / editor.state.secondsPerBeat() + 32, editor.state.loopEnd + 16});
        loopStart.setRange(0, loopRange, 1); loopEnd.setRange(1, loopRange, 1);
        loopStart.setValue(editor.state.loopStart, juce::dontSendNotification); loopEnd.setValue(editor.state.loopEnd, juce::dontSendNotification);
        for (std::size_t i = 0; i < 8; ++i) {
            gains[i].setValue(editor.state.lanes[i].gain, juce::dontSendNotification); pans[i].setValue(editor.state.lanes[i].pan, juce::dontSendNotification);
            mutes[i].setToggleState(editor.state.lanes[i].mute, juce::dontSendNotification); solos[i].setToggleState(editor.state.lanes[i].solo, juce::dontSendNotification);
        }
        undo.setEnabled(editor.canUndo() && !busy); redo.setEnabled(editor.canRedo() && !busy);
        for (auto* b : {&split, &duplicate, &copy, &remove}) b->setEnabled(c && !busy);
        paste.setEnabled(clipboard.has_value() && !busy); syncing = false;
    }
    void refresh() { production.setBusy(busy); effects.setEffects(editor.state.lanes[static_cast<std::size_t>(effects.selectedTrack())].effects); engine.publish(editor.state); syncInspector(); resized(); repaint(); timeline.repaint(); }
    void applyProperty(std::size_t index) {
        if (syncing || busy) return;
        auto* c = editor.state.find(selected); if (!c) return;
        auto before = editor.state; auto next = *c; double value = properties[index].getValue();
        switch (index) { case 0: next.beat = snapped(value); break; case 1: next.offset = value; break; case 2: next.duration = value; break; case 3: next.gain = static_cast<float>(value); break; case 4: next.fadeIn = value; break; case 5: next.fadeOut = value; break; }
        next.fadeIn = std::min(next.fadeIn, next.duration); next.fadeOut = std::min(next.fadeOut, next.duration);
        if (studio::valid(next)) { *c = next; editor.commit(before); refresh(); }
        else { message("Edit exceeds source audio bounds"); syncInspector(); }
    }
    void handleIncomingMidiMessage(juce::MidiInput*, const juce::MidiMessage& midi) override { if (midi.isNoteOnOrOff()) instrumentEngine.note(midi.getNoteNumber(), midi.isNoteOn() ? midi.getVelocity() : 0); else if (midi.isAllNotesOff() || midi.isAllSoundOff()) instrumentEngine.allNotesOff(); }
    void assignPad(bool hard, int pad) {
        auto sample = browser.currentSample(); if (busy || !sample) { instruments.report("Select a licensed sample in Library + Match first"); return; }
        busy = true; worker.start([safe = juce::Component::SafePointer<Studio>(this), file = juce::File(sample->path), hard, pad](CancellationToken token) { std::shared_ptr<const studio::Asset> asset; auto result = studio::importAsset(file, asset, [&] { return token.stop_requested(); }); if (token.stop_requested()) return;
            juce::MessageManager::callAsync([safe, asset, result, hard, pad] { if (!safe) return; safe->busy = false; if (result.wasOk()) safe->instruments.assignSample(pad, hard, asset); safe->instruments.report(result.wasOk() ? "Assigned real audio to pad " + juce::String(pad + 1) : result.getErrorMessage()); safe->refresh(); }); });
    }
    void commitDrum(const studio::InstrumentState& instrument, const studio::DrumPattern& pattern) {
        if (busy) return; auto lanes = studio::drumArrangement(instrument, pattern, editor.state.bpm, snapped(engine.position() / editor.state.secondsPerBeat())); std::size_t n = 0; for (const auto& lane : lanes) n += lane.clips.size();
        if (!n || editor.count() + n > studio::Editor::maxClips) { instruments.report(!n ? "Assign samples to active pads before committing" : "Pattern exceeds 512 regions"); return; }
        auto before = editor.state; for (std::size_t i = 0; i < 8; ++i) editor.state.lanes[i].clips.insert(editor.state.lanes[i].clips.end(), lanes[i].clips.begin(), lanes[i].clips.end()); editor.commit(before); refresh(); instruments.report("Committed " + juce::String(n) + " audible sample regions; use Arrangement transport. Undo restores the prior arrangement.");
    }
    void exportDrum(studio::DrumPattern pattern) {
        if (busy) return; chooser = std::make_unique<juce::FileChooser>("Export original drum pattern", juce::File{}, "*.mid");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe = juce::Component::SafePointer<Studio>(this), pattern](const auto& dialog) { if (!safe || dialog.getResult() == juce::File{}) return; auto result = writeMidi(dialog.getResult().withFileExtension("mid"), studio::drumMidi(pattern), safe->editor.state.bpm, 4, 4, pattern.bars * 4, "Jas PRO original drum groove"); safe->instruments.report(result.wasOk() ? "Exported MIDI channel 10 / notes 36-51; map your own licensed sounds" : result.getErrorMessage()); });
    }
    void instrumentDialog(bool saving) {
        if (busy) return; chooser = std::make_unique<juce::FileChooser>(saving ? "Save sample instrument preset" : "Open sample instrument preset", juce::File{}, "*.jasinst");
        chooser->launchAsync((saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles, [safe = juce::Component::SafePointer<Studio>(this), saving](const auto& dialog) {
            if (!safe || safe->busy || dialog.getResult() == juce::File{}) return; safe->busy = true; auto file = saving ? dialog.getResult().withFileExtension("jasinst") : dialog.getResult(); auto xml = studio::instrumentXml(safe->instruments.instrumentState(), safe->instruments.drumPattern());
            safe->worker.start([safe, file, xml, saving](CancellationToken token) { auto instrument = std::make_shared<studio::InstrumentState>(); auto pattern = std::make_shared<studio::DrumPattern>(); juce::String error;
                try { if (saving) { juce::TemporaryFile temp(file); if (!temp.getFile().replaceWithText(xml) || !temp.overwriteTargetFileWithTemporary()) throw std::runtime_error("Cannot save preset"); } else { if (!file.existsAsFile() || file.getSize() > 65536) throw std::runtime_error("Invalid preset file"); studio::loadInstrumentXml(file.loadFileAsString(), *instrument, *pattern, [&] { return token.stop_requested(); }); } } catch (const std::exception& e) { error = juce::String::fromUTF8(e.what()); }
                if (token.stop_requested()) return; juce::MessageManager::callAsync([safe, instrument, pattern, saving, error] { if (!safe) return; safe->busy = false; if (!saving && error.isEmpty()) safe->instruments.restore(*instrument, *pattern); safe->instruments.report(error.isEmpty() ? "Instrument preset " + juce::String(saving ? "saved" : "loaded") : error); safe->refresh(); });
            });
        });
    }
    void generateGuitar(const juce::String& text, studio::InspireSettings parameters) {
        if (busy) return; std::vector<Chord> chords; auto tokens = juce::StringArray::fromTokens(text, " ,|", ""); tokens.removeEmptyStrings();
        for (const auto& token : tokens) { auto chord = parseChord(token.toStdString()); if (!chord) { production.report("Invalid chord: " + token); return; } chords.push_back(*chord); }
        if (chords.empty() || chords.size() > 32) { production.report("Provide 1-32 chord symbols"); return; }
        busy = true; production.setBusy(true); double bpmValue = editor.state.bpm;
        worker.start([safe = juce::Component::SafePointer<Studio>(this), chords, parameters, bpmValue](CancellationToken token) {
            std::shared_ptr<const studio::Asset> asset; juce::File audioFile, midiFile; juce::String error;
            try {
                GuitarSettings settings; settings.bars = std::min(16, parameters.bars); settings.seed = parameters.seed; settings.energy = parameters.energy;
                if (parameters.genre == "Punjabi Pop" || parameters.genre == "Bhangra") settings.style = GuitarStyle::PunjabiPop; else if (parameters.genre == "Bollywood") settings.style = GuitarStyle::Bollywood;
                auto notes = wingman::generateGuitar(chords, 0, settings, [&] { return !token.stop_requested(); });
                auto audio = renderGuitar(notes, settings, bpmValue, 48000, [&](double) { return !token.stop_requested(); });
                auto directory = studio::dataDirectory().getChildFile("generated"); if (directory.createDirectory().failed()) throw std::runtime_error("Cannot create generated audio folder");
                auto name = "guitar-" + juce::Uuid().toString(); audioFile = directory.getChildFile(name + ".wav"); midiFile = directory.getChildFile(name + ".mid");
                juce::WavAudioFormat wav; auto stream = audioFile.createOutputStream(); if (!stream) throw std::runtime_error("Cannot create guitar WAV");
                std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 48000, 2, 24, {}, 0)); if (!writer) throw std::runtime_error("Cannot create guitar writer"); stream.release();
                juce::AudioBuffer<float> block(2, 4096); for (std::size_t at = 0; at < audio.channels[0].size(); at += 4096) { if (token.stop_requested()) throw std::runtime_error("Guitar cancelled"); int n = static_cast<int>(std::min<std::size_t>(4096, audio.channels[0].size() - at)); for (int c = 0; c < 2; ++c) block.copyFrom(c, 0, audio.channels[static_cast<std::size_t>(c)].data() + at, n); if (!writer->writeFromAudioSampleBuffer(block, 0, n)) throw std::runtime_error("Guitar WAV write failed"); } writer.reset();
                auto result = writeMidi(midiFile, patternSequence(notes), bpmValue, 4, 4, settings.bars * 4, "Jas PRO chord-aware guitar"); if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString());
                result = studio::importAsset(audioFile, asset, [&] { return token.stop_requested(); }); if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString());
            } catch (const std::exception& e) { error = juce::String::fromUTF8(e.what()); audioFile.deleteFile(); midiFile.deleteFile(); }
            if (token.stop_requested()) return;
            juce::MessageManager::callAsync([safe, asset, midiFile, error] { if (!safe) return; safe->busy = false;
                if (error.isEmpty()) { studio::Region c; c.asset = asset; c.duration = asset->seconds(); if (safe->editor.add(4, c)) { safe->selected = c.id; safe->production.report("Generated chord-aware basic plucked-string synthesis on track 5. This is not a realistic sampled guitar. MIDI: " + midiFile.getFullPathName()); } else safe->production.report("Arrangement limit reached; generated files remain in the data folder"); } else safe->production.report(error); safe->refresh();
            });
        });
    }
    void audition(const studio::Sample& sample, bool matched) {
        if (busy) return; busy = true; production.setBusy(true); message("Preparing audition...");
        const double targetBpm = editor.state.bpm;
        worker.start([safe = juce::Component::SafePointer<Studio>(this), sample, matched, targetBpm](CancellationToken token) {
            std::shared_ptr<const studio::Asset> asset; auto result = studio::importAsset(juce::File(sample.path), asset, [&] { return token.stop_requested(); });
            if (token.stop_requested()) return;
            juce::MessageManager::callAsync([safe, sample, matched, targetBpm, asset, result] {
                if (!safe) return; safe->busy = false;
                if (result.wasOk()) { studio::Arrangement a; a.bpm = targetBpm; studio::Region c; c.asset = asset; c.speed = matched && sample.bpm > 0 ? juce::jlimit(.25, 4.0, targetBpm / sample.bpm) : 1; c.duration = asset->seconds() / c.speed; a.lanes[0].clips.push_back(c); a.loop = true; a.loopEnd = c.duration / a.secondsPerBeat(); safe->engine.playing = false; safe->previewEngine.publish(a); safe->previewEngine.seek(0); safe->previewEngine.playing = true; safe->message("Audition: " + asset->source.getFileName() + (matched && sample.bpm > 0 ? " | tempo via linked pitch/rate" : " | original rate (no tempo match)")); }
                else safe->message(result.getErrorMessage()); safe->refresh();
            });
        });
    }
    void generate(studio::InspireSettings settings, bool magic, int lane) {
        if (busy) return; auto reference = production.reference(); if (magic && !reference) return;
        if (!magic) settings.bpm = editor.state.bpm;
        busy = true; production.setBusy(true); message("Ranking local audio and generating editable regions..."); auto db = browser.databaseFile();
        worker.start([safe = juce::Component::SafePointer<Studio>(this), db, settings, magic, reference, lane](CancellationToken token) {
            auto generated = std::make_shared<studio::Generated>(); juce::String error;
            try {
                studio::Library library(db); studio::Query q; q.limit = 2000; auto samples = library.search(q); std::optional<studio::Region> referenceRegion;
                if (magic) { studio::Region r; auto result = studio::importAsset(juce::File(reference->path), r.asset, [&] { return token.stop_requested(); }); if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString()); r.duration = r.asset->seconds(); referenceRegion = r; }
                *generated = studio::inspire(samples, settings, [&] { return token.stop_requested(); }, referenceRegion, lane);
            } catch (const std::exception& e) { error = juce::String::fromUTF8(e.what()); }
            if (token.stop_requested()) return;
            juce::MessageManager::callAsync([safe, generated, error, lane] {
                if (!safe) return; safe->busy = false;
                if (error.isEmpty()) { auto before = safe->editor.state; if (lane >= 0) safe->editor.state.lanes[static_cast<std::size_t>(lane)].clips = generated->arrangement.lanes[static_cast<std::size_t>(lane)].clips; else safe->editor.state = generated->arrangement; if (safe->editor.count() <= studio::Editor::maxClips) { safe->editor.commit(before); safe->production.report(generated->explanation); safe->selected.clear(); } else { safe->editor.state = before; safe->production.report("Generation exceeds 512 regions; shorten the arrangement"); } }
                else safe->production.report(error); safe->refresh(); safe->message(error.isEmpty() ? "Generated actual audio - inspect Arrangement; Undo restores previous project" : error);
            });
        });
    }
    void flipSelected(studio::FlipSettings settings, bool useOnsets) {
        if (busy) return; auto* c = editor.state.find(selected); if (!c) { production.report("Select a clip in Arrangement first"); return; }
        auto source = *c; int lane = laneFor(selected); busy = true; production.setBusy(true);
        worker.start([safe = juce::Component::SafePointer<Studio>(this), settings, useOnsets, source, lane, bpmValue = editor.state.bpm](CancellationToken token) {
            auto slices = std::make_shared<std::vector<studio::Region>>(); juce::String error;
            try { std::vector<double> onsets; if (useOnsets) onsets = studio::Library::analyse(source.asset->source, [&] { return token.stop_requested(); }).onsets; *slices = studio::flip(source, bpmValue, settings, onsets); if (slices->empty()) throw std::runtime_error("Probability produced no slices; original clip retained"); }
            catch (const std::exception& e) { error = juce::String::fromUTF8(e.what()); }
            if (token.stop_requested()) return;
            juce::MessageManager::callAsync([safe, slices, error, source, lane] { if (!safe) return; safe->busy = false;
                if (error.isEmpty() && safe->editor.count() - 1 + slices->size() <= studio::Editor::maxClips) { auto before = safe->editor.state; auto& clips = safe->editor.state.lanes[static_cast<std::size_t>(lane)].clips; std::erase_if(clips, [&](const auto& c) { return c.id == source.id; }); clips.insert(clips.end(), slices->begin(), slices->end()); safe->sliceMidi = *slices; safe->editor.commit(before); safe->selected = slices->front().id; safe->production.report("Flip applied " + juce::String(slices->size()) + " actual audio slices. Undo restores the source clip. MIDI exports trigger timing only; map notes to slices in your sampler."); }
                else safe->production.report(error.isEmpty() ? "Slice count exceeds 512 regions" : error); safe->refresh();
            });
        });
    }
    void exportSlices() {
        if (busy || sliceMidi.empty()) { production.report("Apply Flip before exporting its trigger MIDI"); return; }
        chooser = std::make_unique<juce::FileChooser>("Export Flip trigger MIDI", juce::File{}, "*.mid");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe = juce::Component::SafePointer<Studio>(this)](const auto& dialog) {
            if (!safe || dialog.getResult() == juce::File{}) return; double end = 0; for (const auto& c : safe->sliceMidi) end = std::max(end, c.beat + .1);
            auto result = writeMidi(dialog.getResult().withFileExtension("mid"), studio::regionMidi(safe->sliceMidi), safe->editor.state.bpm, 4, 4, end, "Jas PRO Flip triggers"); safe->production.report(result.wasOk() ? "Exported slice trigger timing; source audio is not embedded in MIDI" : result.getErrorMessage());
        });
    }
    void chooseImport(int lane) {
        if (busy) return;
        chooser = std::make_unique<juce::FileChooser>("Add clip to " + names[static_cast<std::size_t>(lane)], juce::File{}, "*.wav;*.aif;*.aiff;*.flac");
        double beat = snapped(engine.position() / editor.state.secondsPerBeat());
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<Studio>(this), lane, beat](const auto& dialog) { if (safe && dialog.getResult().existsAsFile()) safe->load(dialog.getResult(), lane, beat); });
    }
    void load(juce::File file, int lane, double beat) {
        if (busy) return;
        busy = true; syncInspector(); message("Caching " + file.getFileName());
        worker.start([safe = juce::Component::SafePointer<Studio>(this), file, lane, beat](CancellationToken token) {
            std::shared_ptr<const studio::Asset> asset;
            auto result = studio::importAsset(file, asset, [&] { return token.stop_requested(); });
            if (token.stop_requested()) return;
            juce::MessageManager::callAsync([safe, asset, result, lane, beat] {
                if (!safe) return; safe->busy = false;
                if (result.wasOk()) {
                    studio::Region c; c.asset = asset; c.duration = asset->seconds(); c.beat = beat;
                    if (safe->editor.add(lane, c)) { safe->selected = c.id; safe->selectedLane = lane; safe->message("Imported " + asset->source.getFileName()); }
                    else safe->message("Arrangement limit: 512 clips");
                } else safe->message(result.getErrorMessage());
                safe->refresh();
            });
        });
    }
    void projectDialog(bool saving) {
        if (busy) return;
        chooser = std::make_unique<juce::FileChooser>(saving ? "Save arrangement (references original audio)" : "Open arrangement", juce::File{}, "*.jaspro");
        chooser->launchAsync((saving ? juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting : juce::FileBrowserComponent::openMode) | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<Studio>(this), saving](const auto& dialog) {
                if (!safe || safe->busy || dialog.getResult() == juce::File{}) return;
                auto file = saving ? dialog.getResult().withFileExtension("jaspro") : dialog.getResult();
                safe->busy = true; safe->syncInspector(); safe->message(saving ? "Saving..." : "Loading and caching sources...");
                auto snapshot = safe->editor.state; snapshot.instrumentPreset = studio::instrumentXml(safe->instruments.instrumentState(), safe->instruments.drumPattern());
                safe->worker.start([safe, saving, file, snapshot](CancellationToken token) {
                    auto next = std::make_shared<studio::Arrangement>(); auto instrument = std::make_shared<studio::InstrumentState>(); auto pattern = std::make_shared<studio::DrumPattern>();
                    auto result = saving ? studio::saveArrangement(file, snapshot) : studio::loadArrangement(file, *next, [&] { return token.stop_requested(); });
                    if (result.wasOk() && !saving && next->instrumentPreset.isNotEmpty()) { try { studio::loadInstrumentXml(next->instrumentPreset, *instrument, *pattern, [&] { return token.stop_requested(); }); } catch (const std::exception& e) { result = juce::Result::fail(juce::String::fromUTF8(e.what())); } }
                    if (token.stop_requested()) return;
                    juce::MessageManager::callAsync([safe, saving, next, instrument, pattern, result] {
                        if (!safe) return; safe->busy = false;
                        if (result.wasOk() && !saving) { safe->engine.playing = false; safe->engine.seek(0); safe->editor.replace(*next); safe->instruments.restore(*instrument, *pattern); safe->selected.clear(); }
                        safe->message(result.wasOk() ? (saving ? "Saved - keep original source files in place" : "Arrangement restored") : result.getErrorMessage()); safe->refresh();
                    });
                });
            });
    }
    void exportMix() {
        if (busy) return;
        chooser = std::make_unique<juce::FileChooser>("Export full arrangement, 48 kHz PCM24", juce::File{}, "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe = juce::Component::SafePointer<Studio>(this)](const auto& dialog) {
                if (!safe || safe->busy || dialog.getResult() == juce::File{}) return;
                auto file = dialog.getResult().withFileExtension("wav");
                for (const auto& pad : safe->instruments.instrumentState().pads) if ((pad.soft && pad.soft->source == file) || (pad.hard && pad.hard->source == file)) { safe->message("Export must not overwrite an instrument source"); return; }
                safe->busy = true; safe->syncInspector(); safe->message("Rendering full arrangement (loop ignored)...");
                auto snapshot = safe->editor.state;
                safe->worker.start([safe, snapshot, file](CancellationToken token) {
                    auto result = studio::bounceArrangement(file, snapshot, [&] { return token.stop_requested(); });
                    if (token.stop_requested()) return;
                    juce::MessageManager::callAsync([safe, result] { if (safe) { safe->busy = false; safe->syncInspector(); safe->message(result.wasOk() ? "Exported 48 kHz / 24-bit stereo WAV" : result.getErrorMessage()); } });
                });
            });
    }
    void duplicateClip(bool fromClipboard) {
        if (busy) return;
        auto* c = editor.state.find(selected);
        if (fromClipboard ? !clipboard.has_value() : c == nullptr) return;
        auto next = fromClipboard ? *clipboard : *c;
        next.id = juce::Uuid().toString();
        next.beat = fromClipboard ? snapped(engine.position() / editor.state.secondsPerBeat()) : next.beat + next.duration / editor.state.secondsPerBeat();
        int lane = fromClipboard ? selectedLane : laneFor(selected);
        if (editor.add(lane, next)) { selected = next.id; refresh(); } else message("Arrangement limit: 512 clips");
    }
    void splitClip() { if (!busy) { if (editor.split(selected, snapped(engine.position() / editor.state.secondsPerBeat()))) refresh(); else message("Place the playhead inside the selected clip to split"); } }
    static void configureSlider(juce::Slider& s, double lo, double hi, double step) {
        s.setRange(lo, hi, step); s.setSliderStyle(juce::Slider::LinearHorizontal); s.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 22);
    }
public:
    explicit Studio(bool enableDevice = true) {
        setLookAndFeel(&theme); setWantsKeyboardFocus(true);
        addAndMakeVisible(workspace); workspace.addItem("Arrangement", 1); workspace.addItem("Library + Match", 2); workspace.addItem("Inspire / Flip", 3); workspace.addItem("Play / Drum", 4); workspace.addItem("Effects / Mixer", 5); workspace.addItem("Cloud / Account", 6); workspace.addItem("Library + Timeline", 7); workspace.setSelectedId(1);
        addChildComponent(browser); addChildComponent(production); addChildComponent(instruments); addChildComponent(effects); addChildComponent(cloudPanel); addChildComponent(divider); workspace.onChange = [this] { if (workspace.getSelectedId() == 2 || workspace.getSelectedId() == 7) browser.refreshResults(); resized(); repaint(); };
        cloudPanel.selectedSample = [this] { return browser.currentSample(); };
        effects.onSelect = [this](int lane) { effects.setEffects(editor.state.lanes[static_cast<std::size_t>(lane)].effects); };
        effects.onChange = [this](int lane, const studio::Effects& parameters) { if (busy) return; auto before = editor.state; editor.state.lanes[static_cast<std::size_t>(lane)].effects = parameters; editor.commit(before); refresh(); };
        instruments.onChange = [this](const auto& state) { instrumentEngine.publish(state); };
        instruments.onNote = [this](int note, int velocity) { instrumentEngine.note(note, velocity); };
        instruments.onSilence = [this] { instrumentEngine.allNotesOff(); };
        instruments.onAssign = [this](bool hard, int pad) { assignPad(hard, pad); };
        instruments.onCommit = [this](const auto& state, const auto& pattern) { commitDrum(state, pattern); };
        instruments.onMidi = [this](const auto&, const auto& pattern) { exportDrum(pattern); };
        instruments.onPreset = [this](bool saving) { instrumentDialog(saving); };
        instruments.onMidiDevice = [this](const auto& id) { if (midiInput) midiInput->stop(); midiInput.reset(); instrumentEngine.allNotesOff(); if (id.isNotEmpty()) { midiInput = juce::MidiInput::openDevice(id, this); if (midiInput) midiInput->start(); else instruments.report("MIDI device unavailable"); } };
        browser.addToTrack = [this](const studio::Sample& sample) { workspace.setSelectedId(1); load(juce::File(sample.path), selectedLane, snapped(engine.position() / editor.state.secondsPerBeat())); };
        browser.reference = [this](const studio::Sample& sample) { production.setReference(sample); workspace.setSelectedId(3); };
        browser.stopAudition = [this] { previewEngine.playing = false; };
        browser.audition = [this](const studio::Sample& sample, bool matched) { audition(sample, matched); };
        production.onGenerate = [this](auto settings, bool magicMode, int lane) { generate(settings, magicMode, lane); };
        production.onFlip = [this](auto settings, bool transients) { flipSelected(settings, transients); };
        production.onGuitar = [this](auto chords, auto settings) { generateGuitar(chords, settings); };
        production.onMidi = [this] { exportSlices(); };
        for (auto* b : {&play, &stop, &loop, &devices, &bounce, &saveProject, &openProject, &undo, &redo, &split, &duplicate, &copy, &paste, &remove}) { addAndMakeVisible(b); b->setWantsKeyboardFocus(false); }
        addAndMakeVisible(viewport); viewport.setViewedComponent(&timeline, false); viewport.setScrollBarsShown(false, true);
        for (auto* c : {static_cast<juce::Component*>(&snap), static_cast<juce::Component*>(&grid), static_cast<juce::Component*>(&bpm), static_cast<juce::Component*>(&zoom), static_cast<juce::Component*>(&loopStart), static_cast<juce::Component*>(&loopEnd), static_cast<juce::Component*>(&status), static_cast<juce::Component*>(&position), static_cast<juce::Component*>(&inspector)}) addAndMakeVisible(c);
        snap.setToggleState(true, juce::dontSendNotification); grid.addItem("1/16 note", 1); grid.addItem("Beat", 2); grid.addItem("Bar", 3); grid.setSelectedId(2);
        configureSlider(bpm, 30, 300, .1); bpm.setValue(120, juce::dontSendNotification); bpm.setTooltip("Project BPM. Audio keeps its original speed; clip anchors follow beats.");
        configureSlider(zoom, 8, 128, 1); zoom.setValue(pixelsPerBeat, juce::dontSendNotification); zoom.setTooltip("Horizontal waveform zoom");
        configureSlider(loopStart, 0, 999996, 1); configureSlider(loopEnd, 1, 1000000, 1); loopEnd.setValue(16, juce::dontSendNotification);
        bpm.onValueChange = [this] { if (syncing || busy) return; auto before = editor.state; editor.state.bpm = bpm.getValue(); editor.commit(before); refresh(); };
        zoom.onValueChange = [this] { pixelsPerBeat = zoom.getValue(); resized(); timeline.repaint(); };
        auto changeLoop = [this] {
            if (syncing || busy) return;
            if (loopEnd.getValue() <= loopStart.getValue()) { message("Loop end must follow loop start"); syncInspector(); return; }
            auto before = editor.state; editor.state.loopStart = loopStart.getValue(); editor.state.loopEnd = loopEnd.getValue(); editor.commit(before); refresh();
        };
        loopStart.onValueChange = changeLoop; loopEnd.onValueChange = changeLoop;
        loop.setClickingTogglesState(true); loop.onClick = [this] { if (busy) { syncInspector(); return; } auto before = editor.state; editor.state.loop = loop.getToggleState(); editor.commit(before); refresh(); };
        play.onClick = [this] { previewEngine.playing = false; if (engine.playing.load()) { engine.playing = false; return; } if (editor.state.end() <= 0) { message("Import audio first"); return; } if (engine.position() >= editor.state.end()) engine.seek(0); engine.playing = true; };
        stop.onClick = [this] { engine.playing = false; previewEngine.playing = false; instrumentEngine.allNotesOff(); engine.seek(0); };
        saveProject.onClick = [this] { projectDialog(true); }; openProject.onClick = [this] { projectDialog(false); }; bounce.onClick = [this] { exportMix(); };
        undo.onClick = [this] { if (!busy && editor.undo()) refresh(); }; redo.onClick = [this] { if (!busy && editor.redo()) refresh(); };
        split.onClick = [this] { splitClip(); }; duplicate.onClick = [this] { duplicateClip(false); };
        copy.onClick = [this] { if (auto* c = editor.state.find(selected)) { clipboard = *c; message("Clip copied; paste at playhead on selected track"); syncInspector(); } };
        paste.onClick = [this] { duplicateClip(true); }; remove.onClick = [this] { if (!busy && editor.remove(selected)) { selected.clear(); refresh(); } };
        devices.onClick = [this] { juce::DialogWindow::LaunchOptions options; options.dialogTitle = "Audio output"; options.dialogBackgroundColour = panel; options.content.setOwned(new juce::AudioDeviceSelectorComponent(deviceManager, 0, 0, 2, 2, false, false, true, false)); options.content->setSize(500, 430); options.launchAsync(); };
        for (std::size_t i = 0; i < 8; ++i) {
            imports[i].setButtonText("+ Audio"); mutes[i].setButtonText("M"); solos[i].setButtonText("S");
            for (auto* b : {&imports[i], &mutes[i], &solos[i]}) { addAndMakeVisible(b); b->setWantsKeyboardFocus(false); }
            mutes[i].setClickingTogglesState(true); solos[i].setClickingTogglesState(true);
            imports[i].onClick = [this, i] { chooseImport(static_cast<int>(i)); };
            auto mix = [this, i] { if (syncing || busy) return; auto before = editor.state; auto& lane = editor.state.lanes[i]; lane.gain = static_cast<float>(gains[i].getValue()); lane.pan = static_cast<float>(pans[i].getValue()); lane.mute = mutes[i].getToggleState(); lane.solo = solos[i].getToggleState(); editor.commit(before); refresh(); };
            configureSlider(gains[i], 0, 1, .01); configureSlider(pans[i], -1, 1, .01);
            for (auto* s : {&gains[i], &pans[i]}) { s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0); addAndMakeVisible(s); s->onValueChange = mix; }
            gains[i].setTooltip("Track volume"); pans[i].setTooltip("Stereo pan"); mutes[i].onClick = mix; solos[i].onClick = mix;
        }
        for (std::size_t i = 0; i < properties.size(); ++i) {
            auto& p = properties[i]; configureSlider(p, 0, i == 3 ? 2 : 1000000, i == 3 ? .01 : .001);
            p.setTooltip(propertyNames[i]); p.onValueChange = [this, i] { applyProperty(i); }; addAndMakeVisible(p);
        }
        message("Import or drop WAV / AIFF / FLAC | Click ruler to seek | Space: play/pause | S: split | Cmd/Ctrl+Z: undo");
        setSize(1400, 850); refresh(); if (enableDevice) setAudioChannels(0, 2); startTimerHz(30);
    }
    void testView(int view) { workspace.setSelectedId(view, juce::sendNotificationSync); resized(); }
    bool testEditing() {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-ui-fixture", ".wav", true);
        { juce::WavAudioFormat wav; auto stream = file.createOutputStream();
          std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 8000, 1, 24, {}, 0));
          if (!writer) return false; stream.release(); juce::AudioBuffer<float> data(1, 16000);
          for (int i = 0; i < 16000; ++i) data.setSample(0, i, .2f);
          if (!writer->writeFromAudioSampleBuffer(data, 0, 16000)) return false; }
        std::shared_ptr<const studio::Asset> asset; auto result = studio::importAsset(file, asset);
        auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-ui-test-data", "", true); temp.createDirectory(); bool panels = false;
        { studio::LibraryPanel testBrowser(temp.getChildFile("library.sqlite")); panels = testBrowser.testWorkflow(file); studio::ProductionPanel testProduction; panels &= testProduction.testControls(); studio::InstrumentPanel testInstrument; panels &= testInstrument.testControls(asset); studio::EffectsPanel testEffects; panels &= testEffects.testControls(); }
        temp.deleteRecursively(); file.deleteFile();
        if (result.failed()) return false;
        studio::Region c; c.asset = asset; c.duration = 2; c.beat = 2; editor.add(0, c); selected = c.id; refresh();
        auto event = [this](juce::Point<float> at, juce::Point<float> down, bool drag) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys::leftButtonModifier,
                1, 0, 0, 0, 0, &timeline, &timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag);
        };
        auto from = timeline.bounds(c, 0).getCentre();
        auto to = from + juce::Point<float>(static_cast<float>(2 * pixelsPerBeat), static_cast<float>(rowHeight));
        timeline.mouseDown(event(from, from, false)); timeline.mouseDrag(event(to, from, true)); timeline.mouseUp(event(to, from, true));
        bool ok = panels && editor.state.lanes[1].clips.size() == 1 && editor.state.find(c.id)->beat == 4;
        undo.onClick(); ok &= editor.state.lanes[0].clips.size() == 1 && editor.state.find(c.id)->beat == 2;
        redo.onClick(); engine.seek(3); split.onClick(); ok &= editor.state.lanes[1].clips.size() == 2;
        duplicate.onClick(); ok &= editor.state.lanes[1].clips.size() == 3;
        copy.onClick(); selectedLane = 7; engine.seek(5); paste.onClick(); ok &= editor.state.lanes[7].clips.size() == 1;
        properties[3].setValue(.5, juce::sendNotificationSync); ok &= editor.state.find(selected)->gain == .5f;
        properties[4].setValue(.1, juce::sendNotificationSync); ok &= std::abs(editor.state.find(selected)->fadeIn - .1) < .0001;
        auto pasted = *editor.state.find(selected); auto r = timeline.bounds(pasted, 7);
        from = {r.getRight() - 2, r.getCentreY()}; to = from - juce::Point<float>(static_cast<float>(pixelsPerBeat), 0);
        timeline.mouseDown(event(from, from, false)); timeline.mouseDrag(event(to, from, true)); timeline.mouseUp(event(to, from, true));
        ok &= std::abs(editor.state.find(selected)->duration - .5) < .0001;
        remove.onClick(); ok &= editor.state.lanes[7].clips.empty(); undo.onClick(); ok &= editor.state.lanes[7].clips.size() == 1;
        bpm.setValue(100, juce::sendNotificationSync); ok &= editor.state.bpm == 100;
        selected = pasted.id; refresh(); timerCallback();
        message(ok ? "GUI edit workflow passed (synthetic audio fixture)" : "GUI edit workflow FAILED");
        std::fprintf(stderr, "%s\n", status.getText().toRawUTF8());
        return ok;
    }
    ~Studio() override { stopTimer(); if (midiInput) midiInput->stop(); midiInput.reset(); shutdownAudio(); worker.request_stop(); worker.join(); viewport.setViewedComponent(nullptr, false); setLookAndFeel(nullptr); }
    void prepareToPlay(int, double rate) override { deviceRate = rate; engine.prepare(rate); previewEngine.prepare(rate); }
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion(); juce::AudioBuffer<float> view(info.buffer->getArrayOfWritePointers(), info.buffer->getNumChannels(), info.startSample, info.numSamples); engine.render(view, deviceRate); previewEngine.render(view, deviceRate); instrumentEngine.render(view, deviceRate);
        for (int ch = 0; ch < view.getNumChannels(); ++ch) for (int i = 0; i < view.getNumSamples(); ++i) view.setSample(ch, i, std::clamp(view.getSample(ch, i), -1.f, 1.f));
    }
    bool keyPressed(const juce::KeyPress& key) override {
        if (busy) return false;
        bool command = key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown(); auto text = juce::CharacterFunctions::toLowerCase(key.getTextCharacter());
        if (key.getKeyCode() == juce::KeyPress::spaceKey) { play.onClick(); return true; }
        if (command && text == 'z') { if (key.getModifiers().isShiftDown()) redo.onClick(); else undo.onClick(); return true; }
        if (command && text == 'y') { redo.onClick(); return true; }
        if (command && text == 'd') { duplicateClip(false); return true; }
        if (command && text == 'c') { copy.onClick(); return true; }
        if (command && text == 'v') { duplicateClip(true); return true; }
        if (command && text == 's') { projectDialog(true); return true; }
        if (command && text == 'o') { projectDialog(false); return true; }
        if (!command && text == 's') { splitClip(); return true; }
        if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey) { remove.onClick(); return true; }
        return false;
    }
    void resized() override {
        if (getWidth() < 500) return;
        const int width = getWidth();
        int x = 18; for (auto* b : {&play, &stop, &loop, &openProject, &saveProject, &bounce, &devices}) { int w = b == &bounce || b == &devices ? 110 : 72; b->setBounds(x, 48, w, 30); x += w + 6; }
        workspace.setBounds(660, 48, std::max(130, width - 990), 30);
        position.setBounds(width - 255, 10, 235, 28);
        bpm.setBounds(width - 260, 48, 235, 30);
        x = 18; for (auto* b : {&undo, &redo, &split, &duplicate, &copy, &paste, &remove}) { b->setBounds(x, 89, 82, 28); x += 88; }
        snap.setBounds(x + 2, 89, 65, 28); grid.setBounds(x + 70, 89, 115, 28); zoom.setBounds(width - 225, 89, 205, 28);
        int bottom = getHeight() - 170; rowHeight = std::max(42, (bottom - 135 - rulerHeight - 16) / 8);
        int offset = dockOffset();
        viewport.setBounds(285 + offset, 135, width - 303 - offset, rulerHeight + 8 * rowHeight + 16);
        double beats = std::max({64.0, editor.state.end() / editor.state.secondsPerBeat() + 16, editor.state.loopEnd + 4});
        timeline.setSize(std::max(viewport.getWidth() - 16, static_cast<int>(std::min(1000000.0, beats * pixelsPerBeat))), rulerHeight + 8 * rowHeight);
        for (int i = 0; i < 8; ++i) {
            auto n = static_cast<std::size_t>(i); int y = 135 + rulerHeight + i * rowHeight;
            imports[n].setBounds(111 + offset, y + 5, 73, 24); mutes[n].setBounds(191 + offset, y + 5, 34, 24); solos[n].setBounds(232 + offset, y + 5, 34, 24);
            gains[n].setBounds(110 + offset, y + 30, 78, 17); pans[n].setBounds(194 + offset, y + 30, 75, 17);
        }
        inspector.setBounds(20, bottom + 5, width - 400, 25);
        loopStart.setBounds(width - 370, bottom + 5, 165, 25); loopEnd.setBounds(width - 195, bottom + 5, 175, 25);
        int cell = (width - 40) / 6;
        for (int i = 0; i < 6; ++i) properties[static_cast<std::size_t>(i)].setBounds(20 + i * cell, bottom + 65, cell - 12, 29);
        status.setBounds(18, getHeight() - 40, width - 36, 30);
        bool arrangement = workspace.getSelectedId() == 1 || workspace.getSelectedId() == 7;
        viewport.setVisible(arrangement); inspector.setVisible(arrangement); loopStart.setVisible(arrangement); loopEnd.setVisible(arrangement);
        for (std::size_t i = 0; i < 8; ++i) { imports[i].setVisible(arrangement); mutes[i].setVisible(arrangement); solos[i].setVisible(arrangement); gains[i].setVisible(arrangement); pans[i].setVisible(arrangement); }
        for (auto& property : properties) property.setVisible(arrangement);
        for (auto* button : {&undo, &redo, &split, &duplicate, &copy, &paste, &remove}) button->setVisible(arrangement);
        snap.setVisible(arrangement); grid.setVisible(arrangement); zoom.setVisible(arrangement);
        browser.setBounds(18, 88, width - 36, getHeight() - 136); production.setBounds(browser.getBounds()); instruments.setBounds(browser.getBounds()); effects.setBounds(browser.getBounds()); cloudPanel.setBounds(browser.getBounds());
        bool docked = workspace.getSelectedId() == 7; browser.setCompact(docked); if (docked) browser.setBounds(18, 135, dockWidth - 10, rulerHeight + 8 * rowHeight + 16); divider.setVisible(docked); divider.setBounds(dockWidth + 10, 135, 5, viewport.getHeight());
        browser.setVisible(workspace.getSelectedId() == 2 || docked); production.setVisible(workspace.getSelectedId() == 3); instruments.setVisible(workspace.getSelectedId() == 4); effects.setVisible(workspace.getSelectedId() == 5); cloudPanel.setVisible(workspace.getSelectedId() == 6);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0e121b)); g.setColour(juce::Colours::white); g.setFont(22);
        g.drawText("JAS PRO  /  LOOP STUDIO", 20, 8, 550, 30, juce::Justification::left);
        g.setFont(12); g.setColour(juce::Colours::lightgrey);
        g.drawText("BPM", getWidth() - 300, 48, 40, 30, juce::Justification::left);
        if (workspace.getSelectedId() != 1 && workspace.getSelectedId() != 7) return;
        int offset = dockOffset();
        g.drawText("ZOOM", getWidth() - 265, 89, 40, 28, juce::Justification::left);
        g.drawText("TRACKS  /  VOLUME | PAN", 20 + offset, 135, 250, 30, juce::Justification::left);
        for (int i = 0; i < 8; ++i) {
            int y = 135 + rulerHeight + i * rowHeight;
            g.setColour(juce::Colour(0xff1c2330)); g.fillRoundedRectangle(static_cast<float>(18 + offset), static_cast<float>(y + 2), 255.0f, static_cast<float>(rowHeight - 3), 4);
            g.setColour(colours[static_cast<std::size_t>(i)]); g.fillRect(18 + offset, y + 2, 4, rowHeight - 3);
            g.setFont(14); g.drawText(juce::String(i + 1) + "  " + names[static_cast<std::size_t>(i)], 30 + offset, y + 8, 78, 25, juce::Justification::left);
        }
        int bottom = getHeight() - 170; g.setColour(juce::Colour(0xff1c2330)); g.fillRect(0, bottom, getWidth(), 130);
        g.setColour(juce::Colours::lightgrey); g.setFont(12);
        g.drawText("Loop start / end (beats)", getWidth() - 370, bottom + 32, 340, 20, juce::Justification::centred);
        int cell = (getWidth() - 40) / 6;
        for (int i = 0; i < 6; ++i) g.drawText(propertyNames[static_cast<std::size_t>(i)], 20 + i * cell, bottom + 43, cell - 12, 20, juce::Justification::left);
    }
    void timerCallback() override {
        if (instrumentEngine.droppedEvents.exchange(0) > 0) message("MIDI queue overload: pending voices were silenced");
        double beat = engine.position() / editor.state.secondsPerBeat();
        position.setText("BAR " + juce::String(static_cast<int>(beat / 4) + 1) + "  /  " + juce::String(engine.position(), 2) + " s", juce::dontSendNotification);
        std::array<float, 8> peaks; for (std::size_t i = 0; i < 8; ++i) peaks[i] = engine.meters[i].load(); effects.setMeters(peaks);
        play.setButtonText(engine.playing.load() ? "Pause" : "Play"); timeline.repaint();
    }
};
class Application final : public juce::JUCEApplication {
    class Window final : public juce::DocumentWindow {
    public:
        Window() : DocumentWindow("Jas PRO Ultimate Loop Studio", background, allButtons) {
            setUsingNativeTitleBar(true); setContentOwned(new Studio, true); setResizable(true, true); setResizeLimits(1100, 740, 3000, 1800); centreWithSize(1400, 850); setVisible(true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };
    std::unique_ptr<Window> window;
public:
    const juce::String getApplicationName() override { return "Jas PRO Ultimate Loop Studio"; }
    const juce::String getApplicationVersion() override { return "0.3.0"; }
    void initialise(const juce::String& args) override {
        if (args.contains("--self-test")) {
            struct Log final : juce::Logger { bool asserted = false; void logMessage(const juce::String& message) override { asserted |= message.containsIgnoreCase("Assertion failure"); std::fputs(message.toRawUTF8(), stderr); std::fputs("\n", stderr); } } log;
            juce::Logger::setCurrentLogger(&log); bool ok = true;
            { Studio studio(false); ok &= studio.testEditing(); for (auto size : {juce::Point<int>(1100, 740), juce::Point<int>(1400, 850), juce::Point<int>(2400, 1500)}) {
                studio.setSize(size.x, size.y); for (auto* child : studio.getChildren()) if (child->isVisible()) ok &= studio.getLocalBounds().contains(child->getBounds());
                for (int view = 1; view <= 7; ++view) {
                    studio.testView(view); ok &= studio.createComponentSnapshot(studio.getLocalBounds()).isValid();
                    for (auto* child : studio.getChildren()) if (child->isVisible()) ok &= studio.getLocalBounds().contains(child->getBounds());
                    if (args.contains("--screenshot") && size.x == 1400) { juce::File file("/tmp/jaspro-view-" + juce::String(view) + ".png"); auto stream = file.createOutputStream(); if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat png; png.writeImageToStream(studio.createComponentSnapshot(studio.getLocalBounds()), *stream); } }
                }
            } }
            std::fprintf(stderr, "GUI self-test: %s\n", ok && !log.asserted ? "PASS" : "FAIL");
            juce::Logger::setCurrentLogger(nullptr); setApplicationReturnValue(ok && !log.asserted ? 0 : 1); quit(); return;
        }
        window = std::make_unique<Window>();
    }
    void shutdown() override { window.reset(); }
};
}
START_JUCE_APPLICATION(wingman::Application)
