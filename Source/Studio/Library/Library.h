#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
#include "Analysis/TempoAnalysis.h"
#include "AI/ChordDetection/HarmonicAnalysis.h"
#include <sqlite3.h>
#include <functional>
#include <stdexcept>

namespace wingman::studio {
using Cancel = std::function<bool()>;
struct Sample {
    int64_t id = 0;
    juce::String path, hash, instrument, genre, mood, collection, chords;
    double duration = 0, bpm = 0, tempoConfidence = 0, keyConfidence = 0, rms = 0, brightness = 0;
    int key = -1, mode = 0, channels = 0;
    bool favorite = false, manualTempo = false, manualKey = false;
    std::vector<float> waveform;
    std::vector<double> onsets;
};
struct Query {
    juce::String text, instrument, genre, mood, collection;
    double bpm = 0, tolerance = 6;
    int key = -1, mode = -1;
    bool favorites = false, duplicates = false;
    int limit = 500, offset = 0;
};
struct ScanResult { int indexed = 0, unchanged = 0, rejected = 0; bool cancelled = false; juce::String error; };
// Each worker/GUI creates its own connection. No SQLite calls on the audio thread.
class Library {
    sqlite3* db = nullptr;
    struct Statement {
        sqlite3_stmt* p = nullptr;
        Statement(sqlite3* db, const char* sql) { if (sqlite3_prepare_v2(db, sql, -1, &p, nullptr) != SQLITE_OK) throw std::runtime_error(sqlite3_errmsg(db)); }
        ~Statement() { sqlite3_finalize(p); }
        void text(int i, const juce::String& s) { if (sqlite3_bind_text(p, i, s.toRawUTF8(), -1, SQLITE_TRANSIENT) != SQLITE_OK) throw std::runtime_error("SQLite bind failed"); }
        void number(int i, double v) { sqlite3_bind_double(p, i, v); }
        void integer(int i, int64_t v) { sqlite3_bind_int64(p, i, v); }
        int step() { int rc = sqlite3_step(p); if (rc != SQLITE_ROW && rc != SQLITE_DONE) throw std::runtime_error(sqlite3_errmsg(sqlite3_db_handle(p))); return rc; }
    };
    void exec(const char* sql) { char* error = nullptr; if (sqlite3_exec(db, sql, nullptr, nullptr, &error) != SQLITE_OK) { juce::String message = juce::String::fromUTF8(error); sqlite3_free(error); throw std::runtime_error(message.toStdString()); } }
    static juce::String text(sqlite3_stmt* p, int c) { auto* v = sqlite3_column_text(p, c); return v ? juce::String::fromUTF8(reinterpret_cast<const char*>(v)) : juce::String{}; }
    static Sample read(sqlite3_stmt* p);
public:
    explicit Library(const juce::File& file);
    ~Library() { if (db) sqlite3_close(db); }
    Library(const Library&) = delete;
    Library& operator=(const Library&) = delete;
    ScanResult scan(const juce::File& folder, const Cancel& cancelled = [] { return false; }, const std::function<void(int)>& progress = {});
    Sample index(const juce::File& file, const Cancel& cancelled = [] { return false; });
    std::vector<Sample> search(const Query& query = {});
    std::vector<Sample> similar(const Sample& reference, const Query& query = {});
    Sample get(int64_t id);
    void annotate(int64_t id, const juce::String& instrument, const juce::String& genre, const juce::String& mood, const juce::String& collection, bool favorite, double bpm, int key, int mode);
    int count();
    static Sample analyse(const juce::File&, const Cancel&);
};
inline juce::String keyName(int key, int mode) {
    static const char* notes[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return key < 0 || key > 11 ? "Unknown" : juce::String(notes[key]) + (mode ? " minor" : " major");
}
inline double compatibility(const Sample& sample, double bpm, int key, int mode) {
    double score = 0;
    if (bpm > 0 && sample.bpm > 0) {
        double distance = std::min({std::abs(std::log2(sample.bpm / bpm)), std::abs(std::log2(sample.bpm * 2 / bpm)), std::abs(std::log2(sample.bpm / 2 / bpm))});
        score += std::max(-2.0, 1 - distance * 10);
    }
    if (key >= 0 && sample.key >= 0) {
        int distance = (sample.key - key + 12) % 12;
        if (distance == 0 && sample.mode == mode) score += 2;
        else if ((distance == 7 || distance == 5) && sample.mode == mode) score += 1;
        else if ((mode == 0 && sample.mode == 1 && distance == 9) || (mode == 1 && sample.mode == 0 && distance == 3)) score += 1.5;
        else score -= 2;
    }
    return score;
}
}
