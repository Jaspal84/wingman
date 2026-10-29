#include "Library.h"
namespace wingman::studio {
static juce::String floatsToJson(const std::vector<float>& values) { juce::Array<juce::var> a; for (float v : values) a.add(v); return juce::JSON::toString(a, true); }
static juce::String doublesToJson(const std::vector<double>& values) { juce::Array<juce::var> a; for (double v : values) a.add(v); return juce::JSON::toString(a, true); }
Library::Library(const juce::File& file) {
    auto result = file.getParentDirectory().createDirectory(); if (result.failed()) throw std::runtime_error(result.getErrorMessage().toStdString());
    if (sqlite3_open_v2(file.getFullPathName().toRawUTF8(), &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        juce::String error = db ? juce::String::fromUTF8(sqlite3_errmsg(db)) : "Cannot open library"; if (db) sqlite3_close(db); db = nullptr; throw std::runtime_error(error.toStdString());
    }
    try {
        sqlite3_busy_timeout(db, 5000); exec("PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;");
        Statement version(db, "PRAGMA user_version"); version.step();
        int schema = sqlite3_column_int(version.p, 0);
        if (schema > 2) throw std::runtime_error("Library schema is newer than this application");
        if (schema == 0) exec("BEGIN IMMEDIATE; CREATE TABLE IF NOT EXISTS samples("
             "id INTEGER PRIMARY KEY,path TEXT NOT NULL UNIQUE,size INTEGER NOT NULL,mtime INTEGER NOT NULL,hash TEXT NOT NULL,"
             "duration REAL NOT NULL,channels INTEGER NOT NULL,bpm REAL NOT NULL,tempoConfidence REAL NOT NULL,keyRoot INTEGER NOT NULL,mode INTEGER NOT NULL,keyConfidence REAL NOT NULL,"
             "rms REAL NOT NULL,brightness REAL NOT NULL,waveform TEXT NOT NULL,onsets TEXT NOT NULL,chords TEXT NOT NULL,"
             "instrument TEXT NOT NULL DEFAULT '',genre TEXT NOT NULL DEFAULT '',mood TEXT NOT NULL DEFAULT '',collection TEXT NOT NULL DEFAULT '',favorite INTEGER NOT NULL DEFAULT 0,"
             "manualTempo INTEGER NOT NULL DEFAULT 0,manualKey INTEGER NOT NULL DEFAULT 0);"
             "CREATE INDEX IF NOT EXISTS samples_hash ON samples(hash); CREATE INDEX IF NOT EXISTS samples_tempo ON samples(bpm);"
             "CREATE INDEX IF NOT EXISTS samples_key ON samples(keyRoot,mode); PRAGMA user_version=1; COMMIT;");
        if (schema < 2) exec("BEGIN IMMEDIATE; CREATE VIRTUAL TABLE IF NOT EXISTS samples_fts USING fts5(path,instrument,genre,mood,collection,content='samples',content_rowid='id');"
             "CREATE TRIGGER IF NOT EXISTS samples_ai AFTER INSERT ON samples BEGIN INSERT INTO samples_fts(rowid,path,instrument,genre,mood,collection) VALUES(new.id,new.path,new.instrument,new.genre,new.mood,new.collection); END;"
             "CREATE TRIGGER IF NOT EXISTS samples_ad AFTER DELETE ON samples BEGIN INSERT INTO samples_fts(samples_fts,rowid,path,instrument,genre,mood,collection) VALUES('delete',old.id,old.path,old.instrument,old.genre,old.mood,old.collection); END;"
             "CREATE TRIGGER IF NOT EXISTS samples_au AFTER UPDATE ON samples BEGIN INSERT INTO samples_fts(samples_fts,rowid,path,instrument,genre,mood,collection) VALUES('delete',old.id,old.path,old.instrument,old.genre,old.mood,old.collection); INSERT INTO samples_fts(rowid,path,instrument,genre,mood,collection) VALUES(new.id,new.path,new.instrument,new.genre,new.mood,new.collection); END;"
             "INSERT INTO samples_fts(samples_fts) VALUES('rebuild'); PRAGMA user_version=2; COMMIT;");
        // Additive side table; the existing v2 sample/FTS schema remains readable.
        exec("CREATE TABLE IF NOT EXISTS library_folders(path TEXT PRIMARY KEY NOT NULL)");
    } catch (...) { sqlite3_close(db); db = nullptr; throw; }
}
Sample Library::read(sqlite3_stmt* p) {
    Sample s; s.id = sqlite3_column_int64(p, 0); s.path = text(p, 1); s.hash = text(p, 2);
    s.duration = sqlite3_column_double(p, 3); s.channels = sqlite3_column_int(p, 4); s.bpm = sqlite3_column_double(p, 5);
    s.tempoConfidence = sqlite3_column_double(p, 6); s.key = sqlite3_column_int(p, 7); s.mode = sqlite3_column_int(p, 8);
    s.keyConfidence = sqlite3_column_double(p, 9); s.rms = sqlite3_column_double(p, 10); s.brightness = sqlite3_column_double(p, 11);
    auto wave = juce::JSON::parse(text(p, 12)); if (auto* a = wave.getArray()) for (auto& v : *a) s.waveform.push_back(static_cast<float>(v));
    auto onsets = juce::JSON::parse(text(p, 13)); if (auto* a = onsets.getArray()) for (auto& v : *a) s.onsets.push_back(static_cast<double>(v));
    s.chords = text(p, 14); s.instrument = text(p, 15); s.genre = text(p, 16); s.mood = text(p, 17); s.collection = text(p, 18);
    s.favorite = sqlite3_column_int(p, 19) != 0; s.manualTempo = sqlite3_column_int(p, 20) != 0; s.manualKey = sqlite3_column_int(p, 21) != 0; return s;
}
static constexpr const char* columns = "id,path,hash,duration,channels,bpm,tempoConfidence,keyRoot,mode,keyConfidence,rms,brightness,waveform,onsets,chords,instrument,genre,mood,collection,favorite,manualTempo,manualKey";
Sample Library::get(int64_t id) { Statement q(db, ("SELECT " + juce::String(columns) + " FROM samples WHERE id=?").toRawUTF8()); q.integer(1, id); if (q.step() == SQLITE_ROW) return read(q.p); throw std::runtime_error("Sample not found"); }
int Library::count() { Statement q(db, "SELECT COUNT(*) FROM samples"); q.step(); return sqlite3_column_int(q.p, 0); }
Sample Library::analyse(const juce::File& file, const Cancel& cancelled) {
    if (cancelled()) throw std::runtime_error("Cancelled");
    juce::AudioFormatManager fm; fm.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (!reader || reader->numChannels < 1 || reader->numChannels > 2 || reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples < 1)
        throw std::runtime_error("Unsupported audio format/rate/channels");
    Sample s; s.path = file.getFullPathName(); s.duration = reader->lengthInSamples / reader->sampleRate; s.channels = static_cast<int>(reader->numChannels); s.waveform.resize(256);
    // Read the entire file in bounded blocks for peaks. Analysis uses the first
    // 30 seconds at 22.05 kHz; results explicitly carry confidence, not AI labels.
    std::vector<float> mono; mono.reserve(30 * 22050); juce::AudioBuffer<float> block(s.channels, 8192);
    double nextAnalysisFrame = 0, squares = 0; int crossings = 0; float previous = 0;
    for (int64_t at = 0; at < reader->lengthInSamples; at += 8192) {
        if (cancelled()) throw std::runtime_error("Cancelled");
        int n = static_cast<int>(std::min<int64_t>(8192, reader->lengthInSamples - at));
        if (!reader->read(&block, 0, n, at, true, true)) throw std::runtime_error("Audio read failed");
        for (int i = 0; i < n; ++i) {
            float peak = 0; for (int c = 0; c < s.channels; ++c) { float v = block.getSample(c, i); if (!std::isfinite(v)) throw std::runtime_error("Non-finite audio"); peak = std::max(peak, std::abs(v)); }
            auto bucket = static_cast<std::size_t>((static_cast<long double>(at + i) * 256) / reader->lengthInSamples); s.waveform[std::min<std::size_t>(255, bucket)] = std::max(s.waveform[std::min<std::size_t>(255, bucket)], peak);
        }
        while (nextAnalysisFrame < at + n && mono.size() < 30 * 22050) {
            int index = std::clamp(static_cast<int>(nextAnalysisFrame - at), 0, n - 1); float v = 0;
            for (int c = 0; c < s.channels; ++c) v += block.getSample(c, index) / s.channels;
            mono.push_back(v); squares += v * static_cast<double>(v); crossings += (v >= 0) != (previous >= 0); previous = v;
            nextAnalysisFrame += reader->sampleRate / 22050;
        }
    }
    if (cancelled()) throw std::runtime_error("Cancelled");
    // Cancellation-aware stream also makes hashing a large source interruptible.
    struct HashStream final : juce::InputStream {
        juce::FileInputStream input; Cancel cancel;
        HashStream(const juce::File& f, Cancel c) : input(f), cancel(std::move(c)) { if (!input.openedOk()) throw std::runtime_error("Cannot hash audio"); }
        juce::int64 getTotalLength() override { return input.getTotalLength(); } juce::int64 getPosition() override { return input.getPosition(); }
        bool setPosition(juce::int64 p) override { return input.setPosition(p); } bool isExhausted() override { return input.isExhausted(); }
        int read(void* dest, int bytes) override { if (cancel()) throw std::runtime_error("Cancelled"); return input.read(dest, bytes); }
    } hashStream(file, cancelled);
    s.hash = juce::SHA256(hashStream).toHexString();
    s.rms = mono.empty() ? 0 : std::sqrt(squares / mono.size()); s.brightness = mono.empty() ? 0 : crossings / static_cast<double>(mono.size());
    auto tempo = analyseTempo(mono.data(), static_cast<int>(mono.size()), 22050, [&](double) { return !cancelled(); });
    if (tempo.valid) { s.bpm = tempo.bpm; s.tempoConfidence = tempo.confidence; } s.onsets = tempo.onsets;
    auto harmony = analyseHarmony(mono.data(), static_cast<int>(mono.size()), 22050, [&](double) { return !cancelled(); });
    s.keyConfidence = harmony.keyConfidence; if (s.keyConfidence >= .05) { s.key = harmony.keyRoot; s.mode = harmony.minor ? 1 : 0; }
    juce::StringArray chords;
    for (const auto& c : harmony.regions) if (c.confidence >= .05) chords.add(juce::String(c.start, 2) + "s: " + juce::String(chordName(c.chord)) + " (" + juce::String(c.confidence, 2) + ")");
    s.chords = chords.joinIntoString(" | "); return s;
}
Sample Library::index(const juce::File& file, const Cancel& cancelled) {
    const auto size = file.getSize(), modified = file.getLastModificationTime().toMilliseconds();
    { Statement q(db, "SELECT id,size,mtime FROM samples WHERE path=?"); q.text(1, file.getFullPathName());
      if (q.step() == SQLITE_ROW && sqlite3_column_int64(q.p, 1) == size && sqlite3_column_int64(q.p, 2) == modified) return get(sqlite3_column_int64(q.p, 0)); }
    auto s = analyse(file, cancelled); if (cancelled()) throw std::runtime_error("Cancelled");
    if (file.getSize() != size || file.getLastModificationTime().toMilliseconds() != modified) throw std::runtime_error("Source changed during indexing; rescan");
    Statement q(db, "INSERT INTO samples(path,size,mtime,hash,duration,channels,bpm,tempoConfidence,keyRoot,mode,keyConfidence,rms,brightness,waveform,onsets,chords) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) "
        "ON CONFLICT(path) DO UPDATE SET size=excluded.size,mtime=excluded.mtime,hash=excluded.hash,duration=excluded.duration,channels=excluded.channels,"
        "bpm=CASE WHEN manualTempo THEN bpm ELSE excluded.bpm END,tempoConfidence=excluded.tempoConfidence,keyRoot=CASE WHEN manualKey THEN keyRoot ELSE excluded.keyRoot END,"
        "mode=CASE WHEN manualKey THEN mode ELSE excluded.mode END,keyConfidence=excluded.keyConfidence,rms=excluded.rms,brightness=excluded.brightness,waveform=excluded.waveform,onsets=excluded.onsets,chords=excluded.chords");
    q.text(1, s.path); q.integer(2, size); q.integer(3, modified); q.text(4, s.hash); q.number(5, s.duration); q.integer(6, s.channels); q.number(7, s.bpm); q.number(8, s.tempoConfidence);
    q.integer(9, s.key); q.integer(10, s.mode); q.number(11, s.keyConfidence); q.number(12, s.rms); q.number(13, s.brightness); q.text(14, floatsToJson(s.waveform)); q.text(15, doublesToJson(s.onsets)); q.text(16, s.chords); q.step();
    Statement id(db, "SELECT id FROM samples WHERE path=?"); id.text(1, s.path); id.step(); return get(sqlite3_column_int64(id.p, 0));
}
ScanResult Library::scan(const juce::File& folder, const Cancel& cancelled, const std::function<void(int)>& progress) {
    ScanResult result; if (!folder.isDirectory()) { result.error = "Not a folder"; return result; }
    { Statement root(db, "INSERT OR IGNORE INTO library_folders(path) VALUES(?)"); root.text(1, folder.getFullPathName()); root.step(); }
    for (const auto& entry : juce::RangedDirectoryIterator(folder, true, "*", juce::File::findFiles, juce::File::FollowSymlinks::no)) {
        if (cancelled()) { result.cancelled = true; break; }
        auto file = entry.getFile(); if (!file.hasFileExtension("wav;aif;aiff;flac")) continue;
        try {
            bool unchanged = false;
            { Statement q(db, "SELECT size,mtime FROM samples WHERE path=?"); q.text(1, file.getFullPathName());
              unchanged = q.step() == SQLITE_ROW && sqlite3_column_int64(q.p, 0) == file.getSize() && sqlite3_column_int64(q.p, 1) == file.getLastModificationTime().toMilliseconds(); }
            if (unchanged) ++result.unchanged; else { index(file, cancelled); ++result.indexed; }
        } catch (const std::exception& e) { if (cancelled()) { result.cancelled = true; break; } ++result.rejected; result.error = juce::String::fromUTF8(e.what()); }
        if (progress) progress(result.indexed + result.unchanged + result.rejected);
    }
    return result;
}
std::vector<Sample> Library::search(const Query& query) {
    juce::String sql = "SELECT " + juce::String(columns) + " FROM samples WHERE 1=1";
    std::vector<juce::String> parameters;
    auto tokens = juce::StringArray::fromTokens(query.text.toLowerCase(), " \t", "\""); tokens.removeEmptyStrings();
    if (!tokens.isEmpty()) {
        juce::StringArray terms; for (auto token : tokens) terms.add("\"" + token.replace("\"", "\"\"") + "\"");
        sql += " AND id IN (SELECT rowid FROM samples_fts WHERE samples_fts MATCH ?)";
        parameters.push_back(terms.joinIntoString(" AND "));
    }
    for (auto pair : {std::pair<const char*, juce::String>{"instrument", query.instrument}, {"genre", query.genre}, {"mood", query.mood}, {"collection", query.collection}})
        if (pair.second.isNotEmpty()) { sql += " AND " + juce::String(pair.first) + "=? COLLATE NOCASE"; parameters.push_back(pair.second); }
    const juce::String drums = "(lower(instrument) LIKE '%drum%' OR lower(instrument) LIKE '%kick%' OR lower(instrument) LIKE '%snare%' OR lower(instrument) LIKE '%hat%' OR lower(instrument) LIKE '%perc%' OR lower(instrument) LIKE '%dhol%' OR lower(instrument) LIKE '%tabla%' OR lower(instrument) LIKE '%clap%')";
    if (query.category == LibraryCategory::DrumLoops) sql += " AND duration>=0.8 AND " + drums;
    if (query.category == LibraryCategory::OneShots) sql += " AND duration<0.8";
    if (query.category == LibraryCategory::Vocals) sql += " AND (lower(instrument) LIKE '%vocal%' OR lower(instrument) LIKE '%voice%')";
    if (query.category == LibraryCategory::Instruments) sql += " AND instrument<>'' AND NOT " + drums + " AND lower(instrument) NOT LIKE '%vocal%' AND lower(instrument) NOT LIKE '%voice%' AND lower(instrument) NOT LIKE '%fx%'";
    if (query.folder.isNotEmpty()) { auto prefix = juce::File(query.folder).getFullPathName(); if (!prefix.endsWith(juce::File::getSeparatorString())) prefix += juce::File::getSeparatorString(); sql += " AND substr(path,1,length(?))=?"; parameters.push_back(prefix); parameters.push_back(prefix); }
    if (query.collected) sql += " AND collection<>''";
    if (query.favorites) sql += " AND favorite=1";
    if (query.duplicates) sql += " AND hash IN (SELECT hash FROM samples GROUP BY hash HAVING COUNT(*)>1)";
    if (query.bpm > 0) sql += " AND abs(bpm-?)<=?";
    if (query.key >= 0) sql += " AND keyRoot=?";
    if (query.mode >= 0) sql += " AND mode=?";
    sql += " ORDER BY path COLLATE NOCASE LIMIT ? OFFSET ?";
    Statement q(db, sql.toRawUTF8()); int i = 1; for (const auto& p : parameters) q.text(i++, p);
    if (query.bpm > 0) { q.number(i++, query.bpm); q.number(i++, std::max(0.0, query.tolerance)); }
    if (query.key >= 0) q.integer(i++, query.key); if (query.mode >= 0) q.integer(i++, query.mode);
    q.integer(i++, std::clamp(query.limit, 1, 2000)); q.integer(i, std::max(0, query.offset));
    std::vector<Sample> results; while (q.step() == SQLITE_ROW) results.push_back(read(q.p)); return results;
}
std::vector<Sample> Library::similar(const Sample& reference, const Query& query) {
    auto rows = search(query);
    auto score = [&](const Sample& s) { return compatibility(s, reference.bpm, reference.key, reference.mode) - std::abs(s.rms - reference.rms) * 2 - std::abs(s.brightness - reference.brightness) * 5 - std::min(2.0, std::abs(std::log2((s.duration + .1) / (reference.duration + .1)))); };
    std::stable_sort(rows.begin(), rows.end(), [&](const auto& a, const auto& b) { return score(a) > score(b); }); return rows;
}
void Library::annotate(int64_t id, const juce::String& instrument, const juce::String& genre, const juce::String& mood, const juce::String& collection, bool favorite, double bpm, int key, int mode) {
    if (!std::isfinite(bpm) || bpm < 0 || bpm > 300 || key < -1 || key > 11 || mode < 0 || mode > 1) throw std::runtime_error("Invalid tempo/key metadata");
    Statement q(db, "UPDATE samples SET instrument=?,genre=?,mood=?,collection=?,favorite=?,bpm=?,keyRoot=?,mode=?,manualTempo=1,manualKey=1 WHERE id=?");
    q.text(1, instrument.trim().substring(0, 128)); q.text(2, genre.trim().substring(0, 128)); q.text(3, mood.trim().substring(0, 128)); q.text(4, collection.trim().substring(0, 128));
    q.integer(5, favorite); q.number(6, bpm); q.integer(7, key); q.integer(8, mode); q.integer(9, id); q.step(); if (sqlite3_changes(db) != 1) throw std::runtime_error("Sample no longer exists");
}
}

namespace wingman::studio {
void Library::setFavorite(int64_t id, bool value) { Statement q(db, "UPDATE samples SET favorite=? WHERE id=?"); q.integer(1, value); q.integer(2, id); q.step(); if (sqlite3_changes(db) != 1) throw std::runtime_error("Sample not found"); }
juce::StringArray Library::collections() { juce::StringArray result; Statement q(db, "SELECT DISTINCT collection FROM samples WHERE collection<>'' ORDER BY collection COLLATE NOCASE"); while (q.step() == SQLITE_ROW) result.add(text(q.p, 0)); return result; }
juce::StringArray Library::folders() { juce::StringArray result; Statement q(db, "SELECT path FROM library_folders ORDER BY path COLLATE NOCASE"); while (q.step() == SQLITE_ROW) result.add(text(q.p, 0)); if (result.isEmpty()) { Statement paths(db, "SELECT path FROM samples ORDER BY path"); while (paths.step() == SQLITE_ROW) result.addIfNotAlreadyThere(juce::File(text(paths.p, 0)).getParentDirectory().getFullPathName()); } return result; }
}
