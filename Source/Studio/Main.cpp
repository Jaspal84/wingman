#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>
#include "Studio/Arrangement.h"
#include "GUI/Theme.h"
#include "Threading/BackgroundThread.h"

namespace wingman {
class Studio final : public juce::AudioAppComponent, private juce::Timer {
    Theme theme;
    studio::Editor editor;
    studio::TimelineEngine engine;
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
    int rowHeight = 60;
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
    class Timeline final : public juce::Component, public juce::FileDragAndDropTarget {
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
                        auto index = static_cast<std::size_t>((c.offset + local) * c.asset->rate / studio::Asset::peakHop);
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
                    next.beat += left; next.offset += left * spb; next.duration -= left * spb;
                } else {
                    double end = owner.snapped(original.beat + original.duration / spb + delta);
                    next.duration = (end - original.beat) * spb;
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
        bool isInterestedInFileDrag(const juce::StringArray& files) override {
            return files.size() == 1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac");
        }
        void filesDropped(const juce::StringArray& files, int x, int y) override {
            if (!owner.busy && y >= rulerHeight && isInterestedInFileDrag(files))
                owner.load(juce::File(files[0]), juce::jlimit(0, 7, (y - rulerHeight) / owner.rowHeight), owner.snapped(x / owner.pixelsPerBeat));
        }
    } timeline{*this};
    void message(juce::String text) { status.setText(text, juce::dontSendNotification); }
    void syncInspector() {
        syncing = true;
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
            properties[1].setRange(0, std::max(.001, c->asset->seconds() - c->duration), .001);
            properties[2].setRange(.001, std::max(.002, c->asset->seconds() - c->offset), .001);
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
    void refresh() { engine.publish(editor.state); syncInspector(); resized(); repaint(); timeline.repaint(); }
    void applyProperty(std::size_t index) {
        if (syncing || busy) return;
        auto* c = editor.state.find(selected); if (!c) return;
        auto before = editor.state; auto next = *c; double value = properties[index].getValue();
        switch (index) { case 0: next.beat = snapped(value); break; case 1: next.offset = value; break; case 2: next.duration = value; break; case 3: next.gain = static_cast<float>(value); break; case 4: next.fadeIn = value; break; case 5: next.fadeOut = value; break; }
        next.fadeIn = std::min(next.fadeIn, next.duration); next.fadeOut = std::min(next.fadeOut, next.duration);
        if (studio::valid(next)) { *c = next; editor.commit(before); refresh(); }
        else { message("Edit exceeds source audio bounds"); syncInspector(); }
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
                auto snapshot = safe->editor.state;
                safe->worker.start([safe, saving, file, snapshot](CancellationToken token) {
                    auto next = std::make_shared<studio::Arrangement>();
                    auto result = saving ? studio::saveArrangement(file, snapshot) : studio::loadArrangement(file, *next, [&] { return token.stop_requested(); });
                    if (token.stop_requested()) return;
                    juce::MessageManager::callAsync([safe, saving, next, result] {
                        if (!safe) return; safe->busy = false;
                        if (result.wasOk() && !saving) { safe->engine.playing = false; safe->engine.seek(0); safe->editor.replace(*next); safe->selected.clear(); }
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
                safe->busy = true; safe->syncInspector(); safe->message("Rendering full arrangement (loop ignored)...");
                auto snapshot = safe->editor.state; auto file = dialog.getResult().withFileExtension("wav");
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
        play.onClick = [this] { if (engine.playing.load()) { engine.playing = false; return; } if (editor.state.end() <= 0) { message("Import audio first"); return; } if (engine.position() >= editor.state.end()) engine.seek(0); engine.playing = true; };
        stop.onClick = [this] { engine.playing = false; engine.seek(0); };
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
    bool testEditing() {
        auto file = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-ui-fixture", ".wav", true);
        { juce::WavAudioFormat wav; auto stream = file.createOutputStream();
          std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 8000, 1, 24, {}, 0));
          if (!writer) return false; stream.release(); juce::AudioBuffer<float> data(1, 16000);
          for (int i = 0; i < 16000; ++i) data.setSample(0, i, .2f);
          if (!writer->writeFromAudioSampleBuffer(data, 0, 16000)) return false; }
        std::shared_ptr<const studio::Asset> asset; auto result = studio::importAsset(file, asset); file.deleteFile();
        if (result.failed()) return false;
        studio::Region c; c.asset = asset; c.duration = 2; c.beat = 2; editor.add(0, c); selected = c.id; refresh();
        auto event = [this](juce::Point<float> at, juce::Point<float> down, bool drag) {
            return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys::leftButtonModifier,
                1, 0, 0, 0, 0, &timeline, &timeline, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), 1, drag);
        };
        auto from = timeline.bounds(c, 0).getCentre();
        auto to = from + juce::Point<float>(static_cast<float>(2 * pixelsPerBeat), static_cast<float>(rowHeight));
        timeline.mouseDown(event(from, from, false)); timeline.mouseDrag(event(to, from, true)); timeline.mouseUp(event(to, from, true));
        bool ok = editor.state.lanes[1].clips.size() == 1 && editor.state.find(c.id)->beat == 4;
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
    ~Studio() override { stopTimer(); shutdownAudio(); worker.request_stop(); worker.join(); viewport.setViewedComponent(nullptr, false); setLookAndFeel(nullptr); }
    void prepareToPlay(int, double rate) override { deviceRate = rate; }
    void releaseResources() override {}
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override {
        info.clearActiveBufferRegion(); juce::AudioBuffer<float> view(info.buffer->getArrayOfWritePointers(), info.buffer->getNumChannels(), info.startSample, info.numSamples); engine.render(view, deviceRate);
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
        position.setBounds(width - 255, 10, 235, 28);
        bpm.setBounds(width - 275, 48, 250, 30);
        x = 18; for (auto* b : {&undo, &redo, &split, &duplicate, &copy, &paste, &remove}) { b->setBounds(x, 89, 82, 28); x += 88; }
        snap.setBounds(x + 2, 89, 65, 28); grid.setBounds(x + 70, 89, 115, 28); zoom.setBounds(width - 225, 89, 205, 28);
        int bottom = getHeight() - 170; rowHeight = std::max(42, (bottom - 135 - rulerHeight - 16) / 8);
        viewport.setBounds(285, 135, width - 303, rulerHeight + 8 * rowHeight + 16);
        double beats = std::max({64.0, editor.state.end() / editor.state.secondsPerBeat() + 16, editor.state.loopEnd + 4});
        timeline.setSize(std::max(viewport.getWidth() - 16, static_cast<int>(std::min(1000000.0, beats * pixelsPerBeat))), rulerHeight + 8 * rowHeight);
        for (int i = 0; i < 8; ++i) {
            auto n = static_cast<std::size_t>(i); int y = 135 + rulerHeight + i * rowHeight;
            imports[n].setBounds(111, y + 5, 73, 24); mutes[n].setBounds(191, y + 5, 34, 24); solos[n].setBounds(232, y + 5, 34, 24);
            gains[n].setBounds(110, y + 30, 78, 17); pans[n].setBounds(194, y + 30, 75, 17);
        }
        inspector.setBounds(20, bottom + 5, width - 400, 25);
        loopStart.setBounds(width - 370, bottom + 5, 165, 25); loopEnd.setBounds(width - 195, bottom + 5, 175, 25);
        int cell = (width - 40) / 6;
        for (int i = 0; i < 6; ++i) properties[static_cast<std::size_t>(i)].setBounds(20 + i * cell, bottom + 65, cell - 12, 29);
        status.setBounds(18, getHeight() - 40, width - 36, 30);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff0e121b)); g.setColour(juce::Colours::white); g.setFont(22);
        g.drawText("JAS PRO  /  LOOP STUDIO", 20, 8, 550, 30, juce::Justification::left);
        g.setFont(12); g.setColour(juce::Colours::lightgrey);
        g.drawText("BPM", getWidth() - 315, 48, 40, 30, juce::Justification::left);
        g.drawText("ZOOM", getWidth() - 265, 89, 40, 28, juce::Justification::left);
        g.drawText("TRACKS  /  VOLUME | PAN", 20, 135, 250, 30, juce::Justification::left);
        for (int i = 0; i < 8; ++i) {
            int y = 135 + rulerHeight + i * rowHeight;
            g.setColour(juce::Colour(0xff1c2330)); g.fillRoundedRectangle(18.0f, static_cast<float>(y + 2), 255.0f, static_cast<float>(rowHeight - 3), 4);
            g.setColour(colours[static_cast<std::size_t>(i)]); g.fillRect(18, y + 2, 4, rowHeight - 3);
            g.setFont(14); g.drawText(juce::String(i + 1) + "  " + names[static_cast<std::size_t>(i)], 30, y + 8, 78, 25, juce::Justification::left);
        }
        int bottom = getHeight() - 170; g.setColour(juce::Colour(0xff1c2330)); g.fillRect(0, bottom, getWidth(), 130);
        g.setColour(juce::Colours::lightgrey); g.setFont(12);
        g.drawText("Loop start / end (beats)", getWidth() - 370, bottom + 32, 340, 20, juce::Justification::centred);
        int cell = (getWidth() - 40) / 6;
        for (int i = 0; i < 6; ++i) g.drawText(propertyNames[static_cast<std::size_t>(i)], 20 + i * cell, bottom + 43, cell - 12, 20, juce::Justification::left);
    }
    void timerCallback() override {
        double beat = engine.position() / editor.state.secondsPerBeat();
        position.setText("BAR " + juce::String(static_cast<int>(beat / 4) + 1) + "  /  " + juce::String(engine.position(), 2) + " s", juce::dontSendNotification);
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
    const juce::String getApplicationVersion() override { return "0.2.0"; }
    void initialise(const juce::String& args) override {
        if (args.contains("--self-test")) {
            struct Log final : juce::Logger { bool asserted = false; void logMessage(const juce::String& message) override { asserted |= message.containsIgnoreCase("Assertion failure"); std::fputs(message.toRawUTF8(), stderr); std::fputs("\n", stderr); } } log;
            juce::Logger::setCurrentLogger(&log); bool ok = true;
            { Studio studio(false); ok &= studio.testEditing(); for (auto size : {juce::Point<int>(1100, 740), juce::Point<int>(1400, 850), juce::Point<int>(2400, 1500)}) {
                studio.setSize(size.x, size.y); for (auto* child : studio.getChildren()) if (child->isVisible()) ok &= studio.getLocalBounds().contains(child->getBounds());
                ok &= studio.createComponentSnapshot(studio.getLocalBounds()).isValid();
                if (args.contains("--screenshot") && size.x == 1400) { juce::File file("/tmp/jaspro-timeline.png"); auto stream = file.createOutputStream(); if (stream) { stream->setPosition(0); stream->truncate(); juce::PNGImageFormat png; png.writeImageToStream(studio.createComponentSnapshot(studio.getLocalBounds()), *stream); } }
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
