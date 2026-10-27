#include "Studio/Library/Library.h"
#include "Studio/Generate.h"
#include "Studio/Instrument.h"
#include "Studio/Cloud/Supabase.h"
#include <iostream>
using namespace wingman::studio;
static int checks = 0;
static void check(bool value, const char* name) { ++checks; if (!value) throw std::runtime_error(name); }
static void fixture(const juce::File& file, bool pulses) {
    juce::WavAudioFormat wav; auto stream = file.createOutputStream(); std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 22050, 1, 24, {}, 0));
    if (!writer) throw std::runtime_error("Fixture writer"); stream.release(); juce::AudioBuffer<float> samples(1, 22050 * 4);
    for (int i = 0; i < samples.getNumSamples(); ++i) {
        double t = i / 22050.0;
        float v = pulses ? static_cast<float>((i % 11025 < 1000) ? .5 * std::exp(-(i % 11025) / 150.0) : 0)
                         : static_cast<float>(.12 * (std::sin(2 * 3.141592653589793 * 261.625565 * t) + std::sin(2 * 3.141592653589793 * 329.627557 * t) + std::sin(2 * 3.141592653589793 * 391.995436 * t)));
        samples.setSample(0, i, v);
    }
    check(writer->writeFromAudioSampleBuffer(samples, 0, samples.getNumSamples()), "write fixture");
}
int main() {
    auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-workflow", "", true); folder.createDirectory();
    try {
        auto kick = folder.getChildFile("kick 100% 'local'.wav"), chord = folder.getChildFile("chord.wav"), duplicate = folder.getChildFile("duplicate.wav");
        fixture(kick, true); fixture(chord, false); kick.copyFileTo(duplicate);
        const auto db = folder.getChildFile("catalogue.sqlite");
        Library library(db); auto scan = library.scan(folder); check(scan.indexed == 3 && scan.rejected == 0 && library.count() == 3, "folder index");
        scan = library.scan(folder); check(scan.unchanged == 3 && scan.indexed == 0, "incremental scan");
        Query query; query.duplicates = true; check(library.search(query).size() == 2, "SHA-256 duplicate detection");
        query = {}; query.text = "100% 'local'"; auto samples = library.search(query); check(samples.size() == 1, "literal percent and quote search");
        auto k = samples.front(); check(k.waveform.size() == 256 && k.hash.length() == 64 && k.bpm > 118 && k.bpm < 122, "waveform hash and tempo detection");
        library.annotate(k.id, "Kick", "Bhangra", "Energetic", "Set A", true, 120, -1, 0);
        query = {}; query.favorites = true; query.collection = "set a"; query.bpm = 120; check(library.search(query).size() == 1, "favorite collection tempo filter");
        query = {}; query.text = "energetic bhangra kick"; check(library.search(query).size() == 1, "all-word tag search");
        query.text = "' OR 1=1 --"; check(library.search(query).empty(), "SQL injection is literal");
        auto indexedChord = library.index(chord); library.annotate(indexedChord.id, "Chords", "Bhangra", "Energetic", "Set A", false, 120, 0, 0);
        query = {}; query.key = 0; query.mode = 0; auto tonal = library.search(query); check(tonal.size() == 1, "key/mode filter");
        check(!indexedChord.chords.isEmpty() && indexedChord.rms > .01, "real harmonic and audio descriptors");
        kick.setLastModificationTime(juce::Time::getCurrentTime() + juce::RelativeTime::seconds(2)); auto refreshed = library.index(kick);
        check(refreshed.favorite && refreshed.manualTempo && refreshed.instrument == "Kick" && refreshed.bpm == 120, "reindex retains annotations and corrections");
        query = {}; query.limit = 1; check(library.search(query).size() == 1, "pagination page size"); query.offset = 1; check(library.search(query).front().id != library.search(Query{}).front().id, "pagination offset");
        check(library.scan(folder, [] { return true; }).cancelled, "cancel scan");
        bool cancelled = false; try { Library::analyse(chord, [] { return true; }); } catch (...) { cancelled = true; } check(cancelled, "cancel decode/analysis");
        { Library reopened(db); query = {}; query.favorites = true; check(reopened.search(query).size() == 1, "persistent reopen"); }
        auto ranked = library.similar(library.get(k.id)); check(!ranked.empty() && ranked.front().hash == k.hash, "descriptor ranking");
        check(compatibility(tonal.front(), 120, 0, 0) > compatibility(tonal.front(), 120, 1, 0), "harmonic compatibility");
        InspireSettings settings; settings.bars = 4; settings.key = 0; settings.genre = "Bhangra"; settings.mood = "Energetic";
        auto generated = inspire(library.search(), settings, [] { return false; }); auto repeated = inspire(library.search(), settings, [] { return false; });
        check(!generated.arrangement.lanes[0].clips.empty() && !generated.arrangement.lanes[4].clips.empty() && generated.arrangement.lanes[3].clips.empty(), "generation uses tagged assets and leaves unsupported tracks empty");
        check(generated.arrangement.lanes[0].clips.front().asset->source == repeated.arrangement.lanes[0].clips.front().asset->source && generated.arrangement.lanes[0].clips.size() == repeated.arrangement.lanes[0].clips.size(), "reproducible sample and pattern selection");
        check(generated.explanation.contains("no ML model") && generated.explanation.contains("left empty"), "generation explanation");
        std::shared_ptr<const Asset> asset; check(importAsset(chord, asset).wasOk(), "cache tonal source");
        Region reference; reference.asset = asset; reference.duration = 4;
        auto magic = inspire(library.search(), settings, [] { return false; }, reference); check(magic.arrangement.lanes[6].clips.size() == 1, "Magic Arrange keeps actual reference");
        FlipSettings flipSettings; flipSettings.slices = 8; flipSettings.swing = .2;
        auto slices = flip(reference, 120, flipSettings); auto same = flip(reference, 120, flipSettings);
        check(slices.size() == 8 && slices[1].beat > .99, "Flip slice count and swing");
        for (std::size_t i = 0; i < slices.size(); ++i) check(valid(slices[i]) && slices[i].offset == same[i].offset && slices[i].beat == same[i].beat, "Flip bounded reproducibility");
        flipSettings.probability = 0; check(flip(reference, 120, flipSettings).empty(), "probability zero"); flipSettings.probability = 1; flipSettings.reverse = true;
        slices = flip(reference, 120, flipSettings); check(slices.front().reverse, "reverse slices");
        auto midi = regionMidi(slices); check(midi.getNumEvents() == 16 && midi.getEventPointer(0)->noteOffObject != nullptr, "slice MIDI has paired triggers");
        auto midiFile = folder.getChildFile("flip.mid"); check(wingman::writeMidi(midiFile, midi, 120, 4, 4, slices.back().beat + .1, "Flip").wasOk(), "MIDI file export");
        Arrangement state; reference.speed = 2; reference.duration = 2; reference.reverse = true; state.lanes[0].clips.push_back(reference);
        auto file = folder.getChildFile("processed.jaspro"); check(saveArrangement(file, state).wasOk(), "v3 save"); Arrangement restored;
        check(loadArrangement(file, restored).wasOk() && restored.lanes[0].clips[0].speed == 2 && restored.lanes[0].clips[0].reverse, "speed/reverse persistence");
        Editor edit; edit.state = state; check(edit.split(reference.id, 2), "reverse rate split"); for (const auto& c : edit.state.lanes[0].clips) check(valid(c), "reverse split bounds");
        TimelineEngine engine; engine.publish(state); engine.playing = true; juce::AudioBuffer<float> out(2, 4096); out.clear(); engine.render(out, 22050);
        check(out.getMagnitude(0, 4096) > .05f, "audible reverse/rate rendering");
        InstrumentState instrument; instrument.chromatic = true; instrument.pads[0].soft = asset; instrument.pads[0].oneShot = false; instrument.attack = .001; instrument.decay = .01; instrument.release = .02;
        InstrumentEngine sampler; sampler.publish(instrument); check(sampler.note(60, 100), "sampler note accepted");
        juce::AudioBuffer<float> played(2, 1024); played.clear(); sampler.render(played, 22050); check(played.getMagnitude(0, 1024) > .05f, "Play audible sample");
        sampler.note(60, 0); played.clear(); sampler.render(played, 22050); check(played.getMagnitude(0, 800, 224) == 0, "ADSR release stops voice");
        sampler.note(60, 100); sampler.note(64, 80); played.clear(); sampler.render(played, 22050); check(played.getMagnitude(0, 1024) > .05f, "polyphonic chromatic voices");
        sampler.allNotesOff(); played.clear(); sampler.render(played, 22050); check(played.getMagnitude(0, 1024) == 0, "all notes off");
        for (int i = 0; i < 256; ++i) check(sampler.note(i % 128, 100), "bounded queue capacity"); check(!sampler.note(60, 100), "queue overflow reported"); played.clear(); sampler.render(played, 22050); check(played.getMagnitude(0, 1024) == 0, "overflow safely silences pending voices");
        auto pattern = drumPreset(1); auto drumEvents = drumMidi(pattern); check(drumEvents.getNumEvents() > 24 && drumEvents.getEventPointer(0)->message.getChannel() == 10, "original Bhangra-inspired groove MIDI");
        pattern.swing = .2; pattern.steps[0][1] = .5; auto swung = drumMidi(pattern); bool swungOnset = false; for (int i = 0; i < swung.getNumEvents(); ++i) if (swung.getEventPointer(i)->message.isNoteOn() && std::abs(swung.getEventPointer(i)->message.getTimeStamp() - .3) < .0001) swungOnset = true; check(swungOnset, "drum swing timing");
        instrument.chromatic = false; auto drumTracks = drumArrangement(instrument, pattern, 120, 0); check(!drumTracks[0].clips.empty() && drumTracks[1].clips.empty(), "drum commit only uses assigned audio");
        auto presetXml = instrumentXml(instrument, pattern); InstrumentState reloaded; DrumPattern reloadedPattern; loadInstrumentXml(presetXml, reloaded, reloadedPattern);
        check(reloaded.pads[0].soft && reloaded.pads[0].soft->source == chord && reloadedPattern.steps[0][1] == .5f && reloadedPattern.swing == .2, "instrument and pattern persistence");
        bool invalidPreset = false; try { loadInstrumentXml("not XML", reloaded, reloadedPattern); } catch (...) { invalidPreset = true; } check(invalidPreset && reloaded.pads[0].soft, "bad preset preserves current instrument");
        for (std::size_t fx = 0; fx < 11; ++fx) {
            Effects parameters; parameters.enabled[fx] = true; parameters.amount[fx] = .7f; EffectsProcessor processor; processor.prepare(8000); processor.configure(parameters); bool changed = false;
            for (int i = 0; i < 10000; ++i) { float input = static_cast<float>(.2 * std::sin(i * .12)); float left = input, right = input; processor.process(left, right, parameters, i / 4000.0); if (!std::isfinite(left) || !std::isfinite(right)) throw std::runtime_error("nonfinite FX"); changed |= std::abs(left - input) > .00001f; }
            check(changed, "each enabled effect changes signal");
        }
        auto effected = state; effected.lanes[0].effects.enabled[7] = true; effected.lanes[0].effects.enabled[8] = true; effected.lanes[0].effects.amount[7] = .6f;
        file = folder.getChildFile("fx.jaspro"); check(saveArrangement(file, effected).wasOk() && loadArrangement(file, restored).wasOk() && restored.lanes[0].effects.enabled[8] && std::abs(restored.lanes[0].effects.amount[7] - .6f) < .001f, "effect project persistence");
        auto bounce = folder.getChildFile("fx.wav"); check(bounceArrangement(bounce, effected, [] { return false; }).wasOk(), "effected offline rendering");
        juce::AudioFormatManager formats; formats.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(bounce)); check(reader && reader->lengthInSamples == 192000, "fixed effects tail exported");
        juce::AudioBuffer<float> rendered(2, 192000); reader->read(&rendered, 0, 192000, 0, true, true); TimelineEngine sameRenderer; sameRenderer.publish(effected); sameRenderer.playing = true; juce::AudioBuffer<float> live(2, 192000); live.clear(); sameRenderer.render(live, 48000);
        float error = 0; for (int i = 0; i < 192000; ++i) error = std::max(error, std::abs(live.getSample(0, i) - rendered.getSample(0, i))); check(error < .00001f, "effect real-time/offline parity across block sizes");
        using namespace wingman::studio::cloud;
        juce::String fixtureUser = "11111111-1111-1111-1111-111111111111";
        int requests = 0; bool conflict = false; juce::var remoteRecord; juce::MemoryBlock stored;
        Config config{"https://fixture.invalid", "fixture-publishable"};
        auto transport = [&](const Request& request) -> Response {
            ++requests; check(request.headers["apikey"] == "fixture-publishable", "cloud publishable header");
            if (request.path.startsWith("/auth/v1/token")) return {200, object({{"access_token", "fixture-access"}, {"refresh_token", "fixture-refresh"}, {"expires_in", 3600}, {"user", object({{"id", fixtureUser}})}}), {}};
            check(request.headers["Authorization"] == "Bearer fixture-access", "authenticated cloud request");
            if (request.path.startsWith("/rest/v1/rpc")) {
                if (conflict) return {409, {}, {}};
                remoteRecord = object({{"content_hash", request.json["p_hash"]}, {"metadata", request.json["p_metadata"]}, {"revision", static_cast<int>(request.json["p_revision"]) + 1}, {"deleted", request.json["p_deleted"]}}); return {200, remoteRecord, {}};
            }
            if (request.path.startsWith("/rest/")) { juce::Array<juce::var> rows; if (remoteRecord.isObject()) rows.add(remoteRecord); return {200, rows, {}}; }
            if (request.path.startsWith("/storage/")) { if (request.method == "POST") { stored = request.bytes; return {200, {}, {}}; } return {200, {}, stored}; }
            return {204, {}, {}};
        };
        Supabase remote(config, transport); Supabase offline({}, transport); bool noConfig = false; try { offline.signIn("fixture", "fixture"); } catch (...) { noConfig = true; } check(noConfig && requests == 0, "unset cloud never contacts network");
        remote.signIn("fixture@example.invalid", "fixture-password"); check(!remote.currentSession().access.isEmpty(), "authentication response"); remote.refresh();
        auto outboxFile = folder.getChildFile("outbox.json"); SyncStore outbox(outboxFile); auto hash = library.get(k.id).hash; outbox.queue(hash, object({{"favorite", true}}));
        { SyncStore persisted(outboxFile); check(persisted.entries().size() == 1 && persisted.entries()[0].pending, "offline outbox survives restart"); }
        outbox.synchronize(remote); check(outbox.entries()[0].revision == 1 && !outbox.entries()[0].pending, "sync acknowledges revision");
        outbox.queue(hash, object({{"favorite", false}})); conflict = true; bool conflicted = false; try { outbox.synchronize(remote); } catch (...) { conflicted = true; } check(conflicted && outbox.entries()[0].pending, "conflict retains pending local edit"); conflict = false; outbox.synchronize(remote); check(outbox.entries()[0].revision == 2, "retry pending sync after resolution");
        bool consentRequired = false; try { remote.upload(hash, kick, false); } catch (...) { consentRequired = true; } check(consentRequired, "no audio upload without consent");
        remote.upload(hash, kick, true); check(remote.download(hash, kick.getFileName()) == stored && stored.getSize() > 0, "protected storage upload/download and checksum");
        bool traversal = false; try { remote.download(hash, "../audio.wav"); } catch (...) { traversal = true; } check(traversal, "storage path traversal rejected"); stored.fillWith(0); bool badHash = false; try { remote.download(hash, kick.getFileName()); } catch (...) { badHash = true; } check(badHash, "storage checksum corruption rejected");
        fixtureUser = "22222222-2222-2222-2222-222222222222"; Supabase other(config, transport); other.signIn("other@example.invalid", "fixture"); bool isolated = false; try { outbox.synchronize(other); } catch (...) { isolated = true; } check(isolated, "outbox cannot cross accounts");
        remote.signOut(); check(remote.currentSession().access.isEmpty(), "logout clears in-memory tokens");
        std::cout << checks << " offline studio and mocked-cloud assertions passed\n";
    } catch (const std::exception& e) { std::cerr << "FAIL after " << checks << ": " << e.what() << '\n'; folder.deleteRecursively(); return 1; }
    folder.deleteRecursively(); return 0;
}
