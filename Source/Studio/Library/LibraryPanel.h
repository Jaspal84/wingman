#pragma once
#include "Library.h"
#include "GUI/FileDragButton.h"
#include "Threading/BackgroundThread.h"
namespace wingman::studio {
inline juce::File dataDirectory() {
    auto configured = juce::SystemStats::getEnvironmentVariable("JASPRO_DATA_DIR", {});
    return configured.isNotEmpty() ? juce::File(configured) : juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("JasProStudio");
}
class LibraryPanel final : public juce::Component, private juce::ListBoxModel, private juce::Timer {
    juce::File database;
    std::unique_ptr<Library> library;
    std::vector<Sample> rows;
    std::optional<Sample> selected, similarityReference;
    juce::ListBox list{"Local sample catalogue", this};
    juce::TextEditor searchText, instrument, genre, mood, collection, tempo;
    juce::ComboBox key, mode, filterKey, filterMode;
    juce::TextEditor filterBpm;
    juce::ToggleButton favorites{"Favorites only"}, duplicates{"Duplicates"}, favorite{"Favorite"}, matchTempo{"Tempo match (also changes pitch)"};
    juce::TextButton folder{"Index folder"}, cancel{"Cancel scan"}, preview{"Audition"}, stop{"Stop preview"}, insert{"Add to track"}, save{"Save metadata"}, similar{"Find similar"}, reset{"All results"}, useReference{"Use as reference"};
    juce::TextButton previous{"Previous"}, next{"Next"};
    FileDragButton drag;
    juce::Label status, details;
    juce::TextEditor analysis;
    std::unique_ptr<juce::FileChooser> chooser;
    BackgroundThread worker;
    bool scanning = false, similarity = false, compact = false;
    int page = 0;
    void report(const juce::String& s) { status.setText(s, juce::dontSendNotification); }
    int getNumRows() override { return static_cast<int>(rows.size()); }
    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool active) override {
        if (row < 0 || row >= getNumRows()) return; const auto& s = rows[static_cast<std::size_t>(row)];
        g.fillAll(active ? juce::Colour(0xff344c65) : row % 2 ? juce::Colour(0xff202735) : juce::Colour(0xff18202c));
        g.setColour(juce::Colours::white); g.setFont(14); g.drawText((s.favorite ? "* " : "") + juce::File(s.path).getFileName(), 10, 3, width - 20, 20, juce::Justification::left);
        g.setColour(juce::Colours::lightgrey); g.setFont(11);
        g.drawText(juce::String(s.duration, 2) + "s | " + (s.bpm > 0 ? juce::String(s.bpm, 1) + " BPM" : "BPM unknown") + " | " + keyName(s.key, s.mode) + " | " + s.instrument, 10, 23, width - 20, height - 23, juce::Justification::left);
    }
    juce::var getDragSourceDescription(const juce::SparseSet<int>& indexes) override { return indexes.size() == 1 ? juce::var(rows[static_cast<std::size_t>(indexes[0])].path) : juce::var{}; }
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override { if (row >= 0 && row < getNumRows() && audition) audition(rows[static_cast<std::size_t>(row)], matchTempo.getToggleState()); }
    void selectedRowsChanged(int row) override {
        selected = row >= 0 && row < getNumRows() ? std::optional<Sample>(rows[static_cast<std::size_t>(row)]) : std::nullopt;
        bool enabled = selected.has_value(); for (auto* b : {&preview, &insert, &save, &similar, &useReference}) b->setEnabled(enabled); drag.setEnabled(enabled);
        if (selected) {
            const auto& s = *selected; instrument.setText(s.instrument); genre.setText(s.genre); mood.setText(s.mood); collection.setText(s.collection);
            tempo.setText(juce::String(s.bpm, 2)); key.setSelectedId(s.key + 2); mode.setSelectedId(s.mode + 1); favorite.setToggleState(s.favorite, juce::dontSendNotification);
            details.setText("USER METADATA / Corrections override detected values", juce::dontSendNotification);
            analysis.setText("Analysis: first 30 seconds (heuristic)\nTempo confidence: " + juce::String(s.tempoConfidence, 2) + (s.manualTempo ? " [manual BPM]" : "") + "\nKey confidence: " + juce::String(s.keyConfidence, 2) + (s.manualKey ? " [manual key]" : "") + "\nRMS: " + juce::String(s.rms, 3) + " | zero-crossing ratio: " + juce::String(s.brightness, 4) + "\n\nChord estimates:\n" + (s.chords.isEmpty() ? juce::String("Unknown / insufficient harmonic evidence") : s.chords) + "\n\nSource:\n" + s.path + "\n\nSHA-256: " + s.hash, false);
        } else analysis.clear(); if (onSelectionChanged) onSelectionChanged(selected); repaint();
    }
    void refresh() {
        if (!library) return;
        try {
            int64_t previousId = selected ? selected->id : -1;
            Query q; q.text = searchText.getText(); q.favorites = favorites.getToggleState(); q.duplicates = duplicates.getToggleState(); q.limit = 100; q.offset = page * 100; q.bpm = filterBpm.getText().getDoubleValue(); q.key = filterKey.getSelectedId() - 2; q.mode = filterMode.getSelectedId() - 2;
            rows = similarity && similarityReference ? library->similar(*similarityReference, q) : library->search(q); list.updateContent(); list.deselectAllRows(); selectedRowsChanged(-1);
            for (int i = 0; i < static_cast<int>(rows.size()); ++i) if (rows[static_cast<std::size_t>(i)].id == previousId) { list.selectRow(i); break; }
            report(juce::String(library->count()) + " indexed | page " + juce::String(page + 1) + " | " + juce::String(rows.size()) + " results" + (similarity ? " | descriptor/key/tempo ranking" : " | text/tag search"));
        } catch (const std::exception& e) { report(juce::String::fromUTF8(e.what())); }
    }
    void scanFolder() {
        if (scanning || !library) return;
        chooser = std::make_unique<juce::FileChooser>("Index a licensed sample folder", juce::File{}, "*");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [safe = juce::Component::SafePointer<LibraryPanel>(this)](const auto& dialog) { if (safe && dialog.getResult().isDirectory()) safe->scan(dialog.getResult()); });
    }
    void timerCallback() override { stopTimer(); page = 0; similarity = false; refresh(); }
public:
    std::function<void(const Sample&, bool)> audition;
    std::function<void()> stopAudition;
    std::function<void(const std::optional<Sample>&)> onSelectionChanged;
    std::function<void(const Sample&)> addToTrack, reference;
    explicit LibraryPanel(juce::File db = dataDirectory().getChildFile("library.sqlite")) : database(std::move(db)) {
        for (auto* c : std::initializer_list<juce::Component*>{&list, &status, &details, &analysis, &searchText, &instrument, &genre, &mood, &collection, &tempo, &key, &mode, &filterKey, &filterMode, &filterBpm, &favorites, &duplicates, &favorite, &matchTempo, &folder, &cancel, &preview, &stop, &insert, &save, &similar, &reset, &useReference, &drag, &previous, &next}) addAndMakeVisible(c);
        list.setRowHeight(47); searchText.setTextToShowWhenEmpty("Search filenames and user tags (all words)", juce::Colours::grey); searchText.onTextChange = [this] { startTimer(180); };
        for (auto* t : {&instrument, &genre, &mood, &collection, &tempo}) t->setSelectAllWhenFocused(true);
        analysis.setMultiLine(true); analysis.setReadOnly(true); analysis.setScrollbarsShown(true);
        filterBpm.setTextToShowWhenEmpty("BPM +/-6", juce::Colours::grey); filterBpm.setInputRestrictions(6, "0123456789."); filterBpm.onTextChange = [this] { startTimer(180); };
        filterKey.addItem("Any key", 1); for (int i = 0; i < 12; ++i) filterKey.addItem(keyName(i, 0).upToFirstOccurrenceOf(" ", false, false), i + 2); filterKey.setSelectedId(1); filterMode.addItem("Any mode", 1); filterMode.addItem("Major", 2); filterMode.addItem("Minor", 3); filterMode.setSelectedId(1);
        filterKey.onChange = filterMode.onChange = [this] { page = 0; refresh(); };
        key.addItem("Unknown", 1); for (int i = 0; i < 12; ++i) key.addItem(keyName(i, 0).upToFirstOccurrenceOf(" ", false, false), i + 2);
        mode.addItem("Major", 1); mode.addItem("Minor", 2); key.setSelectedId(1); mode.setSelectedId(1);
        favorites.onClick = duplicates.onClick = [this] { page = 0; refresh(); };
        folder.onClick = [this] { scanFolder(); }; cancel.onClick = [this] { worker.request_stop(); report("Cancelling scan; completed records are retained"); };
        preview.onClick = [this] { if (selected && audition) audition(*selected, matchTempo.getToggleState()); };
        stop.onClick = [this] { if (stopAudition) stopAudition(); };
        insert.onClick = [this] { if (selected && addToTrack) addToTrack(*selected); };
        useReference.onClick = [this] { if (selected && reference) reference(*selected); };
        similar.onClick = [this] { similarityReference = selected; similarity = true; page = 0; refresh(); }; reset.onClick = [this] { similarity = false; page = 0; refresh(); };
        previous.onClick = [this] { if (page > 0) { --page; refresh(); } }; next.onClick = [this] { if (rows.size() == 100) { ++page; refresh(); } };
        save.onClick = [this] {
            if (!selected || !library) return;
            try {
                auto text = tempo.getText().trim(); auto value = text.getDoubleValue();
                if (text.isEmpty() || !text.containsOnly("0123456789.")) throw std::runtime_error("BPM must be numeric; use 0 for unknown");
                library->annotate(selected->id, instrument.getText(), genre.getText(), mood.getText(), collection.getText(), favorite.getToggleState(), value, key.getSelectedId() - 2, mode.getSelectedId() - 1); refresh(); report("Metadata saved locally");
            } catch (const std::exception& e) { report(juce::String::fromUTF8(e.what())); }
        };
        drag.setButtonText("Drag source to DAW"); drag.prepare = [this](juce::File& file) { if (!selected) return juce::Result::fail("Select a sample"); file = juce::File(selected->path); return file.existsAsFile() ? juce::Result::ok() : juce::Result::fail("Source file is missing"); }; drag.report = [this](const auto& s) { report(s); };
        try { library = std::make_unique<Library>(database); refresh(); } catch (const std::exception& e) { report("Catalogue unavailable: " + juce::String::fromUTF8(e.what())); folder.setEnabled(false); }
        cancel.setEnabled(false); selectedRowsChanged(-1);
    }
    ~LibraryPanel() override { stopTimer(); worker.request_stop(); worker.join(); list.setModel(nullptr); }
    void refreshResults() { refresh(); }
    void inspectSample(int64_t id) { if (!library) return; try { rows = {library->get(id)}; list.updateContent(); list.deselectAllRows(); list.selectRow(0); selectedRowsChanged(0); report("Edit the selected source's metadata, then return to Discover"); } catch (const std::exception& e) { report(juce::String::fromUTF8(e.what())); } }
    const juce::File& databaseFile() const { return database; }
    std::optional<Sample> currentSample() const { return selected; }
    void scan(const juce::File& directory) {
        if (scanning || !library) return; scanning = true; folder.setEnabled(false); cancel.setEnabled(true); report("Indexing and analysing " + directory.getFullPathName());
        worker.start([safe = juce::Component::SafePointer<LibraryPanel>(this), db = database, directory](CancellationToken token) {
            ScanResult result;
            try { Library connection(db); result = connection.scan(directory, [&] { return token.stop_requested(); }); }
            catch (const std::exception& e) { result.error = juce::String::fromUTF8(e.what()); }
            juce::MessageManager::callAsync([safe, result] { if (!safe) return; safe->scanning = false; safe->folder.setEnabled(true); safe->cancel.setEnabled(false); safe->refresh(); safe->report((result.cancelled ? "Cancelled | " : "Scan complete | ") + juce::String(result.indexed) + " indexed, " + juce::String(result.unchanged) + " unchanged, " + juce::String(result.rejected) + " rejected. " + result.error); });
        });
    }
    bool testWorkflow(const juce::File& source) {
        if (!library) return false; auto sample = library->index(source); library->annotate(sample.id, "Bass", "Bhangra", "Warm", "Validation", true, 120, 0, 0);
        refresh(); list.selectRow(0); bool auditioned = false, inserted = false;
        audition = [&](const auto& s, bool) { auditioned = s.id == sample.id; }; addToTrack = [&](const auto& s) { inserted = s.id == sample.id; };
        preview.onClick(); insert.onClick(); instrument.setText("Chords"); favorite.setToggleState(false, juce::dontSendNotification); save.onClick();
        auto updated = library->get(sample.id); bool ok = auditioned && inserted && updated.instrument == "Chords" && !updated.favorite;
        setSize(1064, 604); for (auto* child : getChildren()) if (child->isVisible()) ok &= getLocalBounds().contains(child->getBounds());
        setCompact(true); setSize(345, 450); for (auto* child : getChildren()) if (child->isVisible()) ok &= getLocalBounds().contains(child->getBounds());
        audition = {}; addToTrack = {}; return ok;
    }
    void setCompact(bool value) { compact = value; resized(); repaint(); }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff131a25)); g.setColour(juce::Colours::lightgrey); g.setFont(12);
        if (compact) {
            auto wave = juce::Rectangle<int>(12, getHeight() - 114, getWidth() - 24, 50); g.setColour(juce::Colour(0xff0c121b)); g.fillRect(wave);
            if (selected && !selected->waveform.empty()) { g.setColour(juce::Colour(0xff5ed0b2)); for (int px = 0; px < wave.getWidth(); ++px) { auto i = std::min(selected->waveform.size() - 1, static_cast<std::size_t>(px) * selected->waveform.size() / wave.getWidth()); float h = selected->waveform[i] * 23; g.drawVerticalLine(wave.getX() + px, wave.getCentreY() - h, wave.getCentreY() + h); } }
            return;
        }
        int x = getWidth() * 55 / 100 + 18, fieldWidth = getWidth() - x - 18;
        const char* labels[]{"Instrument (user tag)", "Genre (user tag)", "Mood (user tag)", "Collection / playlist", "BPM correction (0 = unknown)", "Key correction"};
        for (int i = 0; i < 6; ++i) g.drawText(labels[i], x, 85 + i * 43, fieldWidth, 18, juce::Justification::left);
        auto wave = juce::Rectangle<int>(18, getHeight() - 130, getWidth() * 55 / 100 - 35, 78); g.setColour(juce::Colour(0xff0c121b)); g.fillRect(wave);
        if (selected && !selected->waveform.empty()) { g.setColour(juce::Colour(0xff5ed0b2)); for (int px = 0; px < wave.getWidth(); ++px) { auto i = std::min(selected->waveform.size() - 1, static_cast<std::size_t>(px) * selected->waveform.size() / wave.getWidth()); float h = selected->waveform[i] * wave.getHeight() * .45f; g.drawVerticalLine(wave.getX() + px, wave.getCentreY() - h, wave.getCentreY() + h); } }
    }
    void resized() override {
        for (auto* c : std::initializer_list<juce::Component*>{&instrument, &genre, &mood, &collection, &tempo, &key, &mode, &filterKey, &filterMode, &filterBpm, &favorite, &matchTempo, &save, &similar, &reset, &useReference, &drag, &previous, &next, &analysis, &details, &filterKey, &filterMode, &filterBpm}) c->setVisible(!compact);
        int w = getWidth(), h = getHeight(), left = w * 55 / 100;
        if (compact) {
            folder.setBounds(12, 10, (w - 30) / 2, 27); cancel.setBounds(18 + (w - 30) / 2, 10, (w - 30) / 2, 27); searchText.setBounds(12, 44, w - 24, 28);
            favorites.setBounds(12, 77, 145, 25); duplicates.setBounds(160, 77, w - 170, 25); list.setBounds(12, 108, w - 24, std::max(60, h - 230));
            preview.setBounds(12, h - 56, (w - 36) / 3, 25); stop.setBounds(18 + (w - 36) / 3, h - 56, (w - 36) / 3, 25); insert.setBounds(24 + (w - 36) * 2 / 3, h - 56, (w - 36) / 3, 25); status.setBounds(12, h - 29, w - 24, 23); return;
        }
        folder.setBounds(18, 12, 115, 29); cancel.setBounds(141, 12, 115, 29); searchText.setBounds(264, 12, std::max(100, left - 280), 29);
        favorites.setBounds(18, 47, 135, 26); duplicates.setBounds(160, 47, 120, 26); similar.setBounds(285, 47, 105, 26); reset.setBounds(398, 47, 100, 26);
        filterBpm.setBounds(18, 82, 110, 26); filterKey.setBounds(141, 82, 130, 26); filterMode.setBounds(284, 82, 130, 26);
        list.setBounds(18, 116, left - 35, std::max(100, h - 257)); previous.setBounds(18, h - 43, 88, 26); next.setBounds(113, h - 43, 80, 26); status.setBounds(202, h - 43, w - 218, 30);
        int x = left + 18, fw = w - x - 18; details.setBounds(x, 12, fw, 30);
        favorite.setBounds(x, 47, 110, 27); useReference.setBounds(x + 120, 47, 160, 27);
        juce::TextEditor* fields[]{&instrument, &genre, &mood, &collection, &tempo}; for (int i = 0; i < 5; ++i) fields[i]->setBounds(x, 103 + i * 43, fw, 24);
        key.setBounds(x, 318, fw / 2 - 4, 25); mode.setBounds(x + fw / 2 + 4, 318, fw / 2 - 4, 25);
        save.setBounds(x, 350, 125, 28); matchTempo.setBounds(x + 135, 350, fw - 135, 28);
        preview.setBounds(x, 385, 95, 28); stop.setBounds(x + 103, 385, 110, 28); insert.setBounds(x + 221, 385, 105, 28); drag.setBounds(x + 334, 385, std::max(100, fw - 334), 28);
        analysis.setBounds(x, 425, fw, std::max(50, h - 480));
    }
};
}
