#pragma once
#include "CreatePanel.h"
#include "Studio/Library/LibraryPanel.h"
namespace wingman::studio {
class DiscoveryPanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
    class Row final : public juce::Component, public juce::SettableTooltipClient {
        DiscoveryPanel& owner; int index = -1; bool dragging = false;
        Sample sample;
        float waveformScale = 1;
        juce::TextButton play{" > "}, favorite{"*"};
    public:
        explicit Row(DiscoveryPanel& p) : owner(p) { addAndMakeVisible(play); addAndMakeVisible(favorite); favorite.setComponentID("favourite");
            play.onClick = [this] { owner.select(index); if (owner.onAudition) owner.onAudition(sample, owner.matchTempo.getToggleState()); };
            favorite.onClick = [this] { owner.toggleFavorite(sample); };
        }
        void set(int row) { index = row; sample = owner.rows[static_cast<std::size_t>(row)]; waveformScale = sample.waveform.empty() ? 1.f : std::max(.00001f, *std::max_element(sample.waveform.begin(), sample.waveform.end())); favorite.setToggleState(sample.favorite, juce::dontSendNotification); favorite.setTooltip(sample.favorite ? "Remove from favorites" : "Add to favorites"); play.setButtonText(owner.playingId == sample.id ? "Stop" : "> "); repaint(); }
        void resized() override { play.setBounds(7, 14, 29, 30); favorite.setBounds(getWidth() - 33, 15, 28, 28); }
        void mouseDown(const juce::MouseEvent&) override { dragging = false; owner.select(index); }
        void mouseDoubleClick(const juce::MouseEvent&) override { if (owner.onAudition) owner.onAudition(sample, owner.matchTempo.getToggleState()); }
        void mouseDrag(const juce::MouseEvent& e) override { if (!dragging && e.getDistanceFromDragStart() >= 6) { if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor(this)) { dragging = true; container->startDragging(sample.path, this); } } }
        void paint(juce::Graphics& g) override {
            bool active = owner.selected && owner.selected->id == sample.id; g.fillAll(active ? juce::Colour(0xff282947) : juce::Colour(0xff16191f)); g.setColour(juce::Colour(0xff30333e)); g.drawHorizontalLine(getHeight() - 1, 5, static_cast<float>(getWidth() - 5));
            auto colour = musicalRole(sample.instrument) >= 0 ? roleColour(musicalRole(sample.instrument)) : juce::Colour(0xff929bb4);
            auto wave = juce::Rectangle<int>(44, 15, 51, 29); g.setColour(colour.withAlpha(.10f)); g.fillRoundedRectangle(wave.toFloat(), 5);
            g.setColour(colour); for (int x = 0; x < wave.getWidth() && !sample.waveform.empty(); ++x) { auto at = static_cast<std::size_t>(x) * sample.waveform.size() / wave.getWidth(); float h = juce::jlimit(.5f, 12.f, sample.waveform[at] / waveformScale * 12.f); g.drawVerticalLine(wave.getX() + x, wave.getCentreY() - h, wave.getCentreY() + h); }
            int bpmX = getWidth() - 151, keyX = getWidth() - 100;
            g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(13, juce::Font::bold)); g.drawText(juce::File(sample.path).getFileNameWithoutExtension(), 106, 8, std::max(28, bpmX - 113), 23, juce::Justification::left, true);
            g.setColour(juce::Colour(0xffa3a8bb)); g.setFont(11); auto tags = (sample.instrument.isEmpty() ? "Untagged instrument" : sample.instrument) + " / " + (sample.genre.isEmpty() ? "No genre tag" : sample.genre);
            g.drawText(tags, 106, 32, std::max(28, bpmX - 113), 19, juce::Justification::left, true);
            g.setColour(juce::Colour(0xffc4c7d2)); g.setFont(12); g.drawText(sample.bpm > 0 ? juce::String(sample.bpm, 0) : "--", bpmX, 12, 47, 31, juce::Justification::centred);
            g.drawText(sample.key >= 0 ? keyName(sample.key, sample.mode).replace(" major", "").replace(" minor", "m") : "--", keyX, 12, 59, 31, juce::Justification::centred);
            setTooltip(sample.path + "\n" + juce::String(sample.duration, 2) + " s | normalized waveform thumbnail | drag to a Create layer or hover Arrangement to place on the timeline");
        }
    };
    std::unique_ptr<Library> library;
    std::vector<Sample> rows;
    std::optional<Sample> selected, similarReference;
    juce::ListBox list{"Sample discovery", this};
    juce::TextEditor search;
    juce::ToggleButton matchTempo{"Match BPM"}, matchKey{"Match key"};
    juce::TextButton similar{"Similar sounds"}, all{"All"}, tags{"Edit tags"}, add{"To track"}, previous{"<"}, next{">"};
    FileDragButton external;
    juce::Label caption, status, empty;
    Query scope;
    juce::String scopeName = "Browse";
    int page = 0;
    bool similarMode = false;
    double targetBpm = 120;
    int targetKey = -1, targetMode = 0;
    int64_t playingId = -1;
    int getNumRows() override { return static_cast<int>(rows.size()); }
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow(int row, bool, juce::Component* old) override {
        if (row < 0 || row >= getNumRows()) { delete old; return nullptr; } auto* component = dynamic_cast<Row*>(old); if (!component) { delete old; component = new Row(*this); } component->set(row); return component;
    }
    void selectedRowsChanged(int row) override {
        selected = row >= 0 && row < getNumRows() ? std::optional<Sample>(rows[static_cast<std::size_t>(row)]) : std::nullopt;
        for (auto* b : {&similar, &tags, &add}) b->setEnabled(selected.has_value()); external.setEnabled(selected.has_value()); list.repaint();
        if (onSelection) onSelection(selected);
    }
    void select(int row) { list.selectRow(row); selectedRowsChanged(row); list.updateContent(); }
    void toggleFavorite(Sample sample) { try { if (!library) return; library->setFavorite(sample.id, !sample.favorite); reload(); } catch (const std::exception& e) { report(juce::String::fromUTF8(e.what())); } }
    void timerCallback() override { stopTimer(); page = 0; similarMode = false; reload(); }
    void report(const juce::String& text) { status.setText(text, juce::dontSendNotification); status.setTooltip(text); }
public:
    std::function<void(const Sample&, bool)> onAudition;
    std::function<void(const Sample&)> onEdit, onAdd;
    std::function<void(const std::optional<Sample>&)> onSelection;
    explicit DiscoveryPanel(const juce::File& database) {
        for (auto* c : std::initializer_list<juce::Component*>{&list, &search, &matchTempo, &matchKey, &similar, &all, &tags, &add, &external, &previous, &next, &caption, &status, &empty}) addAndMakeVisible(c);
        list.setRowHeight(60); list.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff16191f)); search.setFont(juce::FontOptions(17)); search.setTextToShowWhenEmpty("Search your sounds...", juce::Colour(0xff8c91a3)); search.onTextChange = [this] { startTimer(180); };
        matchTempo.setTooltip("Filter +/-6 BPM and audition at project rate. Matching tempo also changes pitch."); matchKey.setTooltip("Filter actual sources by the musical key selected in AI Create. This does not pitch-shift audio.");
        matchTempo.onClick = [this] { page = 0; reload(); }; matchKey.onClick = [this] { if (targetKey < 0 && matchKey.getToggleState()) { matchKey.setToggleState(false, juce::dontSendNotification); report("Select a key in AI Create before matching sources"); return; } page = 0; reload(); };
        similar.onClick = [this] { similarReference = selected; similarMode = true; page = 0; reload(); }; all.onClick = [this] { similarMode = false; similarReference.reset(); page = 0; reload(); };
        tags.onClick = [this] { if (selected && onEdit) onEdit(*selected); }; add.onClick = [this] { if (selected && onAdd) onAdd(*selected); };
        previous.onClick = [this] { if (page > 0) { --page; reload(); } }; next.onClick = [this] { if (rows.size() == 100) { ++page; reload(); } };
        external.setButtonText("Drag to DAW"); external.prepare = [this](juce::File& file) { if (!selected) return juce::Result::fail("Select a sound first"); file = juce::File(selected->path); return file.existsAsFile() ? juce::Result::ok() : juce::Result::fail("Source file is missing; reindex its folder"); }; external.report = [this](const auto& message) { report(message); };
        empty.setJustificationType(juce::Justification::centred); empty.setFont(juce::FontOptions(15)); empty.setColour(juce::Label::textColourId, juce::Colour(0xff9da3b5));
        try { library = std::make_unique<Library>(database); reload(); } catch (const std::exception& e) { report("Library unavailable: " + juce::String::fromUTF8(e.what())); }
        selectedRowsChanged(-1);
    }
    void setScope(Query query, const juce::String& name) { scope = std::move(query); scopeName = name; page = 0; similarMode = false; reload(); }
    void setTarget(double bpm, int key, int mode) { targetBpm = bpm; targetKey = key; targetMode = mode; if (key < 0) matchKey.setToggleState(false, juce::dontSendNotification); if (matchTempo.getToggleState() || matchKey.getToggleState()) reload(); }
    void setPlaying(int64_t id) { if (playingId != id) { playingId = id; list.updateContent(); } }
    void reload() {
        if (!library) return;
        try {
            auto id = selected ? selected->id : -1; Query query = scope; query.text = search.getText(); query.limit = 100; query.offset = page * 100;
            if (matchTempo.getToggleState()) { query.bpm = targetBpm; if (matchKey.getToggleState()) query.tolerance = .01; } if (matchKey.getToggleState() && targetKey >= 0) { query.key = targetKey; query.mode = targetMode; }
            rows = similarMode && similarReference ? library->similar(*similarReference, query) : library->search(query);
            list.updateContent(); list.deselectAllRows(); selectedRowsChanged(-1); for (int i = 0; i < getNumRows(); ++i) if (rows[static_cast<std::size_t>(i)].id == id) { list.selectRow(i); break; }
            previous.setEnabled(page > 0); next.setEnabled(rows.size() == 100); caption.setText(scopeName + " / " + juce::String(rows.size()) + " shown" + (similarMode ? " / descriptor ranking" : ""), juce::dontSendNotification);
            report("Page " + juce::String(page + 1) + " | Tempo preview changes pitch. Key match filters sources.");
            empty.setText(library->count() == 0 ? "Your local library starts here.\n\nIndex a licensed WAV, AIFF or FLAC folder.\nAdd instrument and genre tags to use AI Create." : "No sounds match these filters.\nTry another category, search or musical key.", juce::dontSendNotification); empty.setVisible(rows.empty()); repaint();
        } catch (const std::exception& e) { report(juce::String::fromUTF8(e.what())); }
    }
    void auditionCurrent() { if (selected && onAudition) onAudition(*selected, matchTempo.getToggleState()); }
    std::optional<Sample> currentSample() const { return selected; }
    bool testControls(int64_t expectedId) {
        reload(); if (rows.empty()) return false; select(0); bool auditioned = false, edited = false; onAudition = [&](const auto& s, bool) { auditioned = s.id == expectedId; }; onEdit = [&](const auto& s) { edited = s.id == expectedId; };
        if (onAudition && selected) onAudition(*selected, false); tags.onClick(); if (selected) toggleFavorite(*selected); bool persisted = library->get(expectedId).favorite;
        onAudition = {}; onEdit = {}; return auditioned && edited && persisted;
    }
    void resized() override {
        int w = getWidth(), h = getHeight(); search.setBounds(20, 16, w - 40, 44); matchTempo.setBounds(16, 72, 111, 27); matchKey.setBounds(129, 72, 103, 27); similar.setBounds(235, 72, 112, 27); all.setBounds(355, 72, 49, 27);
        caption.setBounds(20, 107, std::max(80, w - 210), 24); list.setBounds(12, 145, w - 24, std::max(70, h - 248)); empty.setBounds(23, 163, w - 46, std::max(60, h - 280));
        tags.setBounds(20, h - 87, 80, 29); add.setBounds(108, h - 87, 82, 29); external.setBounds(198, h - 87, 119, 29); previous.setBounds(w - 93, h - 87, 32, 29); next.setBounds(w - 53, h - 87, 32, 29);
        status.setBounds(18, h - 49, w - 36, 42);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff16191f)); g.setColour(juce::Colour(0xff8c91a3)); g.setFont(11); g.drawText("SOUND / INSTRUMENT / GENRE", 118, 126, std::max(20, getWidth() - 295), 18, juce::Justification::left); g.drawText("BPM", getWidth() - 164, 126, 48, 18, juce::Justification::centred); g.drawText("KEY", getWidth() - 112, 126, 55, 18, juce::Justification::centred);
    }
};
}
