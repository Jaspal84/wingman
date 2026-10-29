#pragma once
#include "DiscoveryPanel.h"
namespace wingman::studio {
class CreateWorkspace final : public juce::Component {
    juce::File database;
    DiscoveryPanel discovery;
    CreatePanel create;
    juce::Component libraryPanel;
    std::array<juce::TextButton, 9> categories;
    const std::array<const char*, 9> categoryNames{"Browse", "My Sounds", "Collections", "Favorites", "Drum Loops", "One-shots", "Instruments", "Vocals", "Local folders"};
    juce::ComboBox collections, folders;
    juce::TextButton index{"+ Index folder"}, cancelScan{"Cancel scan"};
    juce::Label libraryHeading, collectionHeading, folderHeading, scanStatus;
    std::unique_ptr<juce::FileChooser> chooser;
    BackgroundThread generator, scanner;
    Combination combination;
    bool working = false, scanning = false, hostBusy = false, cancelRequested = false;
    int category = 0, leftWidth = 184, rightWidth = 365;
    class Divider final : public juce::Component {
        CreateWorkspace& owner; bool left; int start = 0;
    public:
        Divider(CreateWorkspace& p, bool l) : owner(p), left(l) { setMouseCursor(juce::MouseCursor::LeftRightResizeCursor); }
        void mouseDown(const juce::MouseEvent&) override { start = left ? owner.leftWidth : owner.rightWidth; }
        void mouseDrag(const juce::MouseEvent& e) override { if (left) owner.leftWidth = juce::jlimit(160, 270, start + e.getDistanceFromDragStartX()); else owner.rightWidth = juce::jlimit(320, 500, start - e.getDistanceFromDragStartX()); owner.resized(); }
        void paint(juce::Graphics& g) override { g.setColour(juce::Colour(0xff313541)); g.fillRect(getLocalBounds().reduced(2, 0)); }
    } leftDivider{*this, true}, rightDivider{*this, false};
    void feedback(const juce::String& text) { create.report(text); if (onMessage) onMessage(text); }
    void sync() { create.update(combination, working || hostBusy); create.setCancellable(working); }
    void updateScope() {
        Query q; juce::String title = categoryNames[static_cast<std::size_t>(category)];
        if (category == 2) { q.collected = true; if (collections.getSelectedId() > 1) { q.collection = collections.getText(); title = q.collection; } }
        if (category == 3) q.favorites = true;
        if (category == 4) q.category = LibraryCategory::DrumLoops;
        if (category == 5) q.category = LibraryCategory::OneShots;
        if (category == 6) q.category = LibraryCategory::Instruments;
        if (category == 7) q.category = LibraryCategory::Vocals;
        if (category == 8 && folders.getSelectedId() > 0) { q.folder = folders.getText(); title = juce::File(q.folder).getFileName(); }
        for (int i = 0; i < 9; ++i) categories[static_cast<std::size_t>(i)].setToggleState(i == category, juce::dontSendNotification);
        discovery.setScope(q, title);
    }
    static std::vector<Sample> candidates(Library& library, const InspireSettings& settings, const Cancel& cancel) {
        Query q; q.genre = settings.genre; q.limit = 500; std::vector<Sample> result;
        for (;;) { if (cancel()) throw std::runtime_error("Generation cancelled"); q.offset = static_cast<int>(result.size()); auto page = library.search(q); result.insert(result.end(), page.begin(), page.end()); if (page.size() < 500) return result; if (result.size() >= 10000) throw std::runtime_error("More than 10,000 candidates. Refine the genre or library tags before generating."); }
    }
    void generate(int replacement, bool increment) {
        if (working || hostBusy) return;
        if (replacement >= 0 && combination.locked[static_cast<std::size_t>(replacement)]) { feedback("Unlock this layer before replacing its audio"); return; }
        if (onStop) onStop();
        if (increment) create.incrementSeed(); auto settings = create.settings(); auto previous = combination;
        cancelRequested = false; working = true; sync(); feedback("Finding compatible indexed audio...");
        generator.start([safe = juce::Component::SafePointer<CreateWorkspace>(this), db = database, settings, previous, replacement](CancellationToken token) {
            auto next = std::make_shared<Combination>(); juce::String error;
            try { Library library(db); auto cancelled = [&] { return token.stop_requested(); }; auto rows = candidates(library, settings, cancelled); if (rows.empty()) throw std::runtime_error("Your local library has no matching content. Index licensed WAV/AIFF/FLAC files and add instrument/genre tags in Edit tags."); *next = createCombination(rows, settings, previous, replacement, cancelled); }
            catch (const std::exception& e) { error = juce::String::fromUTF8(e.what()); }
            juce::MessageManager::callAsync([safe, next, error, replacement, cancelled = token.stop_requested()]() mutable { if (!safe) return; cancelled = cancelled || safe->cancelRequested; safe->working = false; if (error.isEmpty() && !cancelled) safe->combination = *next; safe->sync(); if (cancelled) safe->feedback("Cancelled; previous combination kept"); else if (error.isNotEmpty()) safe->feedback(error); else if (replacement >= 0) { const auto& note = safe->combination.explanation[static_cast<std::size_t>(replacement)]; safe->feedback(note.startsWith("No alternative") || note.startsWith("Missing") ? note : "Replaced " + juce::String(layerNames[static_cast<std::size_t>(replacement)]) + " with actual local audio. Other layers kept."); } else { juce::StringArray missing; for (int i = 0; i < 8; ++i) if (safe->combination.arrangement.lanes[static_cast<std::size_t>(i)].clips.empty()) missing.add(layerNames[static_cast<std::size_t>(i)]); safe->feedback(missing.isEmpty() ? "Eight real audio layers ready. Preview or send to Arrangement." : "Missing compatible audio: " + missing.joinIntoString(", ") + ". Add instrument tags or relax genre/key/BPM filters."); } });
        });
    }
    void assign(int role, juce::File file) {
        if (working || hostBusy || combination.locked[static_cast<std::size_t>(role)]) return;
        cancelRequested = false; working = true; sync(); auto settings = create.settings();
        generator.start([safe = juce::Component::SafePointer<CreateWorkspace>(this), role, file, settings](CancellationToken token) {
            auto region = std::make_shared<Region>(); juce::String error;
            try { auto result = importAsset(file, region->asset, [&] { return token.stop_requested(); }); if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString()); region->duration = std::min(region->asset->seconds(), settings.bars * 4 * 60 / settings.bpm); if (!audibleRegion(*region)) throw std::runtime_error("This selected bar range is silent; choose an audible source"); }
            catch (const std::exception& e) { error = juce::String::fromUTF8(e.what()); }
            juce::MessageManager::callAsync([safe, role, region, settings, error, cancelled = token.stop_requested()]() mutable { if (!safe) return; cancelled = cancelled || safe->cancelRequested; safe->working = false; if (error.isEmpty() && !cancelled) { auto n = static_cast<std::size_t>(role); auto& lane = safe->combination.arrangement.lanes[n]; lane.name = layerNames[n]; lane.clips = {*region}; safe->combination.arrangement.bpm = settings.bpm; safe->combination.arrangement.loopEnd = settings.bars * 4; safe->combination.explanation[n] = "Manually assigned " + region->asset->source.getFileName() + "; original speed/pitch"; } safe->sync(); safe->feedback(cancelled ? "Assignment cancelled" : error.isEmpty() ? "Assigned actual audio. Use Lock to preserve this layer." : error); });
        });
    }
    void exportFile(bool midi) {
        if (working) return; if (!combination.audible()) { feedback("Generate audible local layers before exporting"); return; }
        chooser = std::make_unique<juce::FileChooser>(midi ? "Export layer sample-trigger MIDI (not audio transcription)" : "Export complete combination WAV", juce::File{}, midi ? "*.mid" : "*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [safe = juce::Component::SafePointer<CreateWorkspace>(this), midi](const auto& dialog) { if (!safe || safe->working || dialog.getResult() == juce::File{}) return; auto file = dialog.getResult().withFileExtension(midi ? "mid" : "wav"); auto snapshot = safe->combination.arrangement; safe->cancelRequested = false; safe->working = true; safe->sync(); safe->feedback("Exporting real combination...");
                safe->generator.start([safe, file, snapshot, midi](CancellationToken token) { auto result = midi ? writeCombinationMidi(file, snapshot) : bounceArrangement(file, snapshot, [&] { return token.stop_requested(); }); juce::MessageManager::callAsync([safe, result, midi] { if (!safe) return; safe->working = false; safe->sync(); safe->feedback(result.failed() ? result.getErrorMessage() : midi ? "MIDI triggers exported. Map notes 36-43 to your eight audio layers; no transcription is embedded." : "Exported actual layers to 48 kHz / PCM24 stereo WAV."); }); });
            });
    }
    void chooseFolder() {
        if (scanning) return; chooser = std::make_unique<juce::FileChooser>("Index a local licensed sample folder", juce::File{}, "*");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe = juce::Component::SafePointer<CreateWorkspace>(this)](const auto& dialog) { if (safe && dialog.getResult().isDirectory()) safe->indexFolder(dialog.getResult()); });
    }
public:
    std::function<void(const Sample&, bool)> onAudition;
    std::function<void(const Sample&)> onEditSample, onAddSample;
    std::function<void(const std::optional<Sample>&)> onSelection;
    std::function<void(const Arrangement&)> onPreview, onSend;
    std::function<void()> onStop, onTargetSettings;
    std::function<void(const juce::String&)> onMessage;
    explicit CreateWorkspace(juce::File db) : database(std::move(db)), discovery(database) {
        for (auto* c : std::initializer_list<juce::Component*>{&libraryPanel, &discovery, &create, &leftDivider, &rightDivider}) addAndMakeVisible(c);
        for (auto* c : std::initializer_list<juce::Component*>{&collections, &folders, &index, &cancelScan, &libraryHeading, &collectionHeading, &folderHeading, &scanStatus}) libraryPanel.addAndMakeVisible(c);
        libraryHeading.setText("LIBRARY", juce::dontSendNotification); libraryHeading.setFont(juce::FontOptions(13, juce::Font::bold));
        collectionHeading.setText("YOUR COLLECTIONS", juce::dontSendNotification); folderHeading.setText("LOCAL SAMPLE FOLDERS", juce::dontSendNotification); for (auto* label : {&collectionHeading, &folderHeading}) { label->setFont(juce::FontOptions(10, juce::Font::bold)); label->setColour(juce::Label::textColourId, juce::Colour(0xff8c91a3)); }
        for (int i = 0; i < 9; ++i) { auto n = static_cast<std::size_t>(i); libraryPanel.addAndMakeVisible(categories[n]); categories[n].setButtonText(categoryNames[n]); categories[n].setComponentID("library-nav"); categories[n].onClick = [this, i] { category = i; updateScope(); }; }
        categories[0].setTooltip("All indexed local sounds. No proprietary catalogue is connected."); categories[1].setTooltip("Your user-imported local sample catalogue");
        collections.onChange = [this] { category = 2; updateScope(); }; folders.onChange = [this] { category = 8; updateScope(); };
        index.onClick = [this] { chooseFolder(); }; cancelScan.onClick = [this] { scanner.request_stop(); scanStatus.setText("Cancelling scan...", juce::dontSendNotification); }; cancelScan.setEnabled(false);
        discovery.onSelection = [this](const auto& sample) { if (onSelection) onSelection(sample); };
        discovery.onAudition = [this](const auto& sample, bool matched) { if (onAudition) onAudition(sample, matched); }; discovery.onEdit = [this](const auto& sample) { if (onEditSample) onEditSample(sample); }; discovery.onAdd = [this](const auto& sample) { if (onAddSample) onAddSample(sample); };
        create.onGenerate = [this](bool increment) { generate(-1, increment); }; create.onReplace = [this](int role) { generate(role, true); };
        create.onLock = [this](int role, bool locked) { combination.locked[static_cast<std::size_t>(role)] = locked; sync(); };
        create.onAssign = [this](int role, const auto& file) { assign(role, file); };
        create.onPreview = [this] { if (onPreview && combination.audible()) onPreview(combination.arrangement); }; create.onSend = [this] { if (onSend && combination.audible()) onSend(combination.arrangement); };
        create.onWav = [this] { exportFile(false); }; create.onMidi = [this] { exportFile(true); }; create.onCancel = [this] { if (working) { cancelRequested = true; generator.request_stop(); } };
        create.onTargetChange = [this] { auto s = create.settings(); discovery.setTarget(s.bpm, s.key, s.mode); if (onTargetSettings) onTargetSettings(); };
        refreshLibrary(); sync();
    }
    ~CreateWorkspace() override { generator.request_stop(); scanner.request_stop(); generator.join(); scanner.join(); }
    void refreshLibrary() {
        try { Library library(database); auto oldCollection = collections.getText(), oldFolder = folders.getText(); collections.clear(juce::dontSendNotification); collections.addItem("All collections", 1); auto groups = library.collections(); for (int i = 0; i < groups.size(); ++i) collections.addItem(groups[i], i + 2); int selected = groups.indexOf(oldCollection); collections.setSelectedId(selected < 0 ? 1 : selected + 2, juce::dontSendNotification);
            folders.clear(juce::dontSendNotification); auto roots = library.folders(); for (int i = 0; i < roots.size(); ++i) folders.addItem(roots[i], i + 1); folders.setTextWhenNothingSelected("Index a local folder"); int root = roots.indexOf(oldFolder); if (!roots.isEmpty()) folders.setSelectedId(root < 0 ? 1 : root + 1, juce::dontSendNotification);
        } catch (const std::exception& e) { feedback(juce::String::fromUTF8(e.what())); } updateScope();
    }
    void setHostBusy(bool busy) { if (hostBusy != busy) { hostBusy = busy; sync(); } }
    void setPlaying(bool playingCombination, int64_t sampleId) { create.setPlaying(playingCombination); discovery.setPlaying(sampleId); }
    void previewSelection() { discovery.auditionCurrent(); }
    void setBpm(double value) { create.setBpm(value); }
    void exportCurrent(bool midi) { exportFile(midi); }
    InspireSettings targetSettings() const { return create.settings(); }
    std::optional<Sample> currentSample() const { return discovery.currentSample(); }
    bool isWorking() const { return working; }
    const Combination& currentCombination() const { return combination; }
    void indexFolder(const juce::File& folder) {
        if (scanning) return; scanning = true; index.setEnabled(false); cancelScan.setEnabled(true); scanStatus.setText("Indexing local audio...", juce::dontSendNotification);
        scanner.start([safe = juce::Component::SafePointer<CreateWorkspace>(this), db = database, folder](CancellationToken token) { ScanResult result;
            try { Library library(db); result = library.scan(folder, [&] { return token.stop_requested(); }); } catch (const std::exception& e) { result.error = juce::String::fromUTF8(e.what()); }
            juce::MessageManager::callAsync([safe, result] { if (!safe) return; safe->scanning = false; safe->index.setEnabled(true); safe->cancelScan.setEnabled(false); safe->refreshLibrary(); auto text = (result.cancelled ? "Cancelled | " : "Indexed ") + juce::String(result.indexed) + ", unchanged " + juce::String(result.unchanged) + ", rejected " + juce::String(result.rejected) + ". " + result.error; safe->scanStatus.setText(text, juce::dontSendNotification); safe->scanStatus.setTooltip(text); safe->feedback(text); });
        });
    }
    void resized() override {
        int w = getWidth(), h = getHeight(); int left = juce::jlimit(160, 270, leftWidth), right = juce::jlimit(320, 500, rightWidth); int available = w - 430 - 12;
        if (left + right > available) { right = std::max(320, available - left); if (left + right > available) left = std::max(150, available - right); }
        libraryPanel.setBounds(0, 0, left, h); leftDivider.setBounds(left, 0, 6, h); discovery.setBounds(left + 6, 0, w - left - right - 12, h); rightDivider.setBounds(w - right - 6, 0, 6, h); create.setBounds(w - right, 0, right, h);
        int row = h < 620 ? 28 : 34, collectionY = 53 + 9 * row + 14;
        libraryHeading.setBounds(17, 16, left - 32, 25); for (int i = 0; i < 9; ++i) categories[static_cast<std::size_t>(i)].setBounds(12, 53 + i * row, left - 24, row - 4);
        collectionHeading.setBounds(17, collectionY, left - 32, 20); collections.setBounds(14, collectionY + 26, left - 28, 27); folderHeading.setBounds(17, collectionY + 66, left - 32, 20); folders.setBounds(14, collectionY + 93, left - 28, 27);
        index.setBounds(14, h - 102, left - 28, 30); cancelScan.setBounds(14, h - 65, left - 28, 26); scanStatus.setBounds(12, h - 34, left - 24, 30);
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff11141a)); }
};
}
