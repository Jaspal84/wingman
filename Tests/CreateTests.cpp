#include "Studio/Create/Combination.h"
#include <iostream>
using namespace wingman::studio;
static int count = 0;
static void check(bool value, const char* what) { ++count; if (!value) throw std::runtime_error(what); }
static void writeSound(const juce::File& file, double frequency, double duration = 2, bool silent = false) {
    juce::WavAudioFormat format; auto stream = file.createOutputStream(); std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), 8000, 1, 24, {}, 0)); if (!writer) throw std::runtime_error("Fixture writer"); stream.release(); juce::AudioBuffer<float> samples(1, static_cast<int>(8000 * duration));
    for (int i = 0; i < samples.getNumSamples(); ++i) samples.setSample(0, i, silent ? 0.f : static_cast<float>(.08 * std::sin(i * frequency * 2 * juce::MathConstants<double>::pi / 8000)));
    if (!writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples())) throw std::runtime_error("Fixture write");
}
static std::string snapshot(const juce::File& file, const char* sql) {
    sqlite3* db = nullptr;
    if (sqlite3_open_v2(file.getFullPathName().toRawUTF8(), &db, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) throw std::runtime_error("snapshot open");
    std::string result;
    auto callback = [](void* context, int columns, char** values, char**) {
        auto& out = *static_cast<std::string*>(context);
        for (int i = 0; i < columns; ++i) { std::string value = values[i] ? values[i] : "<NULL>"; out += std::to_string(value.size()) + ":" + value; }
        out += "\n"; return 0;
    };
    int status = sqlite3_exec(db, sql, callback, &result, nullptr); sqlite3_close(db);
    if (status != SQLITE_OK) throw std::runtime_error("snapshot query");
    return result;
}
int main() {
    auto root = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-create-tests", "", true); root.createDirectory();
    try {
        Library library(root.getChildFile("library.sqlite")); std::vector<Sample> sources;
        for (int role = 0; role < 8; ++role) { auto file = root.getChildFile(juce::String(layerNames[static_cast<std::size_t>(role)]) + ".wav"); writeSound(file, 130 + role * 47); auto row = library.index(file); library.annotate(row.id, layerNames[static_cast<std::size_t>(role)], "Test genre", "Warm", "My set", role == 1, 120, 0, 0); sources.push_back(library.get(row.id)); }
        for (int role = 0; role < 8; ++role) check(musicalRole(sources[static_cast<std::size_t>(role)].instrument) == role, "eight role classifications");
        check(musicalRole("synth bass") == 1 && musicalRole("electric guitar chords") == 4 && musicalRole("piano keys") == 3, "role precedence");
        auto alternative = root.getChildFile("Bass-alternative.wav"); writeSound(alternative, 73); auto alt = library.index(alternative); library.annotate(alt.id, "Bass", "Test genre", "Warm", "My set", false, 120, 0, 0);
        InspireSettings settings; settings.bars = 2; settings.genre = "Test genre"; settings.key = 0;
        Combination blank; auto generated = createCombination(library.search(), settings, blank, -1, [] { return false; }); check(generated.audible(), "real combination exists");
        for (int role = 0; role < 8; ++role) { const auto& lane = generated.arrangement.lanes[static_cast<std::size_t>(role)]; check(!lane.clips.empty() && lane.name == layerNames[static_cast<std::size_t>(role)] && audibleRegion(lane.clips.front()), "all eight roles contain real audible regions"); }
        auto again = createCombination(library.search(), settings, blank, -1, [] { return false; }); for (int i = 0; i < 8; ++i) check(generated.arrangement.lanes[i].clips[0].asset->source == again.arrangement.lanes[i].clips[0].asset->source && generated.arrangement.lanes[i].clips[0].beat == again.arrangement.lanes[i].clips[0].beat, "seed reproducibility");
        generated.locked[0] = true; auto lockedId = generated.arrangement.lanes[0].clips.front().id; settings.seed++;
        auto refreshed = createCombination(library.search(), settings, generated, -1, [] { return false; }); check(refreshed.arrangement.lanes[0].clips.front().id == lockedId && refreshed.arrangement.lanes[0].clips.front().asset == generated.arrangement.lanes[0].clips.front().asset, "lock preserves exact source and clip identity");
        check(refreshed.arrangement.lanes[1].clips.front().asset->source != generated.arrangement.lanes[1].clips.front().asset->source, "regenerate prefers a genuinely different eligible source");
        auto oldBass = refreshed.arrangement.lanes[1].clips.front().asset->source; auto replaced = createCombination(library.search(), settings, refreshed, 1, [] { return false; });
        check(replaced.arrangement.lanes[1].clips.front().asset->source != oldBass, "replace chooses an actual alternative");
        for (int i = 0; i < 8; ++i) if (i != 1) check(replaced.arrangement.lanes[i].clips.front().id == refreshed.arrangement.lanes[i].clips.front().id, "replacement leaves other seven layers intact");
        auto noAlternative = createCombination({sources[4]}, settings, replaced, 4, [] { return false; }); check(noAlternative.arrangement.lanes[4].clips[0].id == replaced.arrangement.lanes[4].clips[0].id && noAlternative.explanation[4].contains("No alternative"), "missing alternative retains actual current audio");
        auto lockedReplace = createCombination(library.search(), settings, generated, 0, [] { return false; }); check(lockedReplace.arrangement.lanes[0].clips.front().id == lockedId, "locked replace does not mutate");
        auto empty = createCombination({}, settings, blank, -1, [] { return false; }); check(!empty.audible() && empty.explanation[0].contains("Missing"), "missing content has no placeholder audio"); auto missingReplacement = createCombination({}, settings, blank, 2, [] { return false; }); check(missingReplacement.explanation[2].startsWith("Missing") && !missingReplacement.audible(), "empty layer replacement explains missing content without claiming retained audio");
        auto tempoChanged = settings; tempoChanged.bpm = 100; auto keySafe = createCombination(library.search(), tempoChanged, blank, -1, [] { return false; }); check(keySafe.arrangement.lanes[1].clips.empty() && !keySafe.arrangement.lanes[0].clips.empty(), "pitched loops at another tempo are not falsely key-matched");
        Sample silent = sources[0]; silent.rms = 0; auto noSilence = createCombination({silent}, settings, blank, -1, [] { return false; }); check(!noSilence.audible(), "silent indexed content excluded");
        bool cancelled = false; try { createCombination(library.search(), settings, generated, -1, [] { return true; }); } catch (...) { cancelled = true; } check(cancelled && generated.arrangement.lanes[0].clips[0].id == lockedId, "cancellation is atomic");
        TimelineEngine engine; engine.publish(generated.arrangement); engine.playing = true; juce::AudioBuffer<float> block(2, 4096); block.clear(); engine.render(block, 48000); check(block.getMagnitude(0, 4096) > .01f, "complete combination renders audible audio");
        auto wav = root.getChildFile("combination.wav"); check(bounceArrangement(wav, generated.arrangement, [] { return false; }).wasOk(), "combination WAV export"); juce::AudioFormatManager formats; formats.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(wav)); check(reader && reader->lengthInSamples == 192000, "actual arrangement duration exported");
        auto midi = root.getChildFile("combination.mid"); check(writeCombinationMidi(midi, generated.arrangement).wasOk(), "sample-trigger MIDI export"); juce::MidiFile smf; auto stream = midi.createInputStream(); check(stream && smf.readFrom(*stream) && smf.getNumTracks() == 9, "tempo plus eight named MIDI layer tracks");
        for (int i = 1; i <= 8; ++i) { auto events = *smf.getTrack(i); events.updateMatchedPairs(); int attacks = 0; for (int e = 0; e < events.getNumEvents(); ++e) if (events.getEventPointer(e)->message.isNoteOn()) { ++attacks; check(events.getEventPointer(e)->noteOffObject != nullptr, "MIDI has paired layer triggers"); } check(attacks == static_cast<int>(generated.arrangement.lanes[static_cast<std::size_t>(i - 1)].clips.size()), "MIDI follows actual clip count"); }
        auto project = root.getChildFile("combination.jaspro"); check(saveArrangement(project, generated.arrangement).wasOk(), "project export uses existing v3 schema"); Arrangement restored; check(loadArrangement(project, restored).wasOk() && restored.lanes[4].name == "Guitar", "musical layer labels and source clips survive reopening");
        Editor editor; auto before = editor.state; editor.state = generated.arrangement; editor.commit(before); check(editor.undo() && editor.count() == 0 && editor.redo() && editor.count() > 0, "send to editor is undoable");
        Query q; q.favorites = true; check(library.search(q).size() == 1, "favorite scope"); library.setFavorite(sources[0].id, true); auto favored = library.get(sources[0].id); check(favored.favorite && favored.manualTempo, "favorite update retains metadata provenance");
        auto natural = library.index(root.getChildFile("Keys.wav")); bool provenance = natural.manualKey; library.setFavorite(natural.id, true); check(library.get(natural.id).manualKey == provenance, "favorite never changes key provenance");
        q = {}; q.category = LibraryCategory::DrumLoops; check(library.search(q).size() == 1, "drum loops category"); q.category = LibraryCategory::Vocals; check(library.search(q).size() == 1, "vocals category"); q.category = LibraryCategory::Instruments; check(library.search(q).size() == 6, "instruments category excludes drums vocals FX");
        auto shortFile = root.getChildFile("one-shot.wav"); writeSound(shortFile, 550, .2); auto shortSample = library.index(shortFile); library.setFavorite(shortSample.id, true); check(!library.get(shortSample.id).manualKey && !library.get(shortSample.id).manualTempo, "favorite action never fabricates manual corrections"); q = {}; q.category = LibraryCategory::OneShots; check(library.search(q).size() == 1, "one-shots category");
        q = {}; q.collected = true; check(library.search(q).size() == 9 && library.collections().contains("My set"), "collections are actual persisted tags");
        auto child = root.getChildFile("folder%_local"); child.createDirectory(); auto nested = child.getChildFile("nested.wav"); writeSound(nested, 200); auto scan = library.scan(child); check(scan.indexed == 1 && library.folders().contains(child.getFullPathName()), "registered local folder survives scan"); q = {}; q.folder = child.getFullPathName(); check(library.search(q).size() == 1, "folder scope treats SQL wildcard characters literally");
        check(writeCombinationMidi(midi, {}).failed(), "empty MIDI export is unavailable");
        auto catalogue = root.getChildFile("library.sqlite");
        const auto rows = snapshot(catalogue, "SELECT * FROM samples ORDER BY id");
        const auto favorites = snapshot(catalogue, "SELECT id FROM samples WHERE favorite=1 ORDER BY id");
        const auto folders = snapshot(catalogue, "SELECT * FROM library_folders ORDER BY path");
        auto searchIds = [](Library& source) { Query query; query.text = "nested"; std::vector<int64_t> ids; for (const auto& row : source.search(query)) ids.push_back(row.id); return ids; };
        const auto results = searchIds(library);
        sqlite3* raw = nullptr; check(sqlite3_open(catalogue.getFullPathName().toRawUTF8(), &raw) == SQLITE_OK, "open legacy migration fixture");
        auto migrated = sqlite3_exec(raw, "DROP TRIGGER samples_ai; DROP TRIGGER samples_ad; DROP TRIGGER samples_au; DROP TABLE samples_fts; PRAGMA user_version=1;", nullptr, nullptr, nullptr); sqlite3_close(raw); check(migrated == SQLITE_OK, "construct prior catalogue schema fixture");
        check(snapshot(catalogue, "PRAGMA user_version") == "1:1\n", "legacy fixture starts at schema one");
        { Library reopened(catalogue); check(searchIds(reopened) == results, "v1 FTS migration preserves exact search identities"); }
        check(snapshot(catalogue, "SELECT * FROM samples ORDER BY id") == rows, "migration preserves every sample column including provenance hash and timestamps");
        check(snapshot(catalogue, "SELECT id FROM samples WHERE favorite=1 ORDER BY id") == favorites, "migration preserves all favorites");
        check(snapshot(catalogue, "PRAGMA user_version") == "1:2\n", "migration advances schema to two");
        const char* schemaQuery = "SELECT type,name,tbl_name,rootpage,sql FROM sqlite_master ORDER BY type,name; PRAGMA schema_version; PRAGMA user_version";
        const auto schema = snapshot(catalogue, schemaQuery);
        const auto fts = snapshot(catalogue, "SELECT rowid,* FROM samples_fts ORDER BY rowid");
        for (int reopen = 0; reopen < 3; ++reopen) {
            { Library reopened(catalogue); Query favoriteQuery; favoriteQuery.favorites = true;
              check(searchIds(reopened) == results && reopened.search(favoriteQuery).size() == 4, "reopening retains search and favorite filters"); }
            check(snapshot(catalogue, "SELECT * FROM samples ORDER BY id") == rows, "reopening never rewrites sample data");
            check(snapshot(catalogue, schemaQuery) == schema, "reopening leaves schema objects and both schema versions unchanged");
            check(snapshot(catalogue, "SELECT rowid,* FROM samples_fts ORDER BY rowid") == fts, "reopening preserves search index content");
            check(snapshot(catalogue, "SELECT * FROM library_folders ORDER BY path") == folders, "reopening preserves registered folders");
        }

        std::cout << count << " Create composition, discovery and export assertions passed\n";
    } catch (const std::exception& e) { std::cerr << "FAIL after " << count << ": " << e.what() << '\n'; root.deleteRecursively(); return 1; }
    root.deleteRecursively(); return 0;
}
