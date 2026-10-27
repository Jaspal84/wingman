#include "Studio/Arrangement.h"
#include <iostream>
#include <stdexcept>
using namespace wingman::studio;
static int assertions = 0;
static void check(bool value, const char* name) { ++assertions; if (!value) throw std::runtime_error(name); }
int main() {
    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-arrangement-test", "", true);
    dir.createDirectory();
    try {
        auto source = dir.getChildFile("fixture.wav");
        { juce::WavAudioFormat wav; auto stream = source.createOutputStream();
          std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(stream.get(), 8000, 1, 24, {}, 0));
          check(bool(writer), "fixture writer"); stream.release();
          juce::AudioBuffer<float> data(1, 8000); for (int i = 0; i < 8000; ++i) data.setSample(0, i, .25f);
          for (int second = 0; second < 35; ++second) check(writer->writeFromAudioSampleBuffer(data, 0, 8000), "write long fixture"); }
        std::shared_ptr<const Asset> asset;
        check(importAsset(source, asset).wasOk(), "import exceeds old 30-second limit");
        check(asset->frames == 280000 && asset->peaks.size() == 137, "disk-backed cache and peaks");
        check(std::abs(asset->sample(1, 16000.5) - .25f) < .00001f, "mapped stereo interpolation");
        auto retained = asset;
        check(importAsset(source, asset, [] { return true; }).failed() && retained == asset, "cancelled import is atomic");
        Editor editor; Region c; c.asset = asset; c.duration = 2; c.beat = 2;
        check(editor.add(0, c), "add first clip");
        check(!editor.add(1, c), "reject duplicate IDs on insert");
        Region second = c; second.id = juce::Uuid().toString(); second.beat = 8;
        check(editor.add(0, second) && editor.state.lanes[0].clips.size() == 2, "multiple clips on track");
        check(editor.move(c.id, 7, 4) && editor.state.lanes[7].clips.size() == 1, "cross-track move");
        check(editor.undo() && editor.state.lanes[0].clips.size() == 2 && editor.state.find(c.id)->beat == 2, "undo move");
        check(editor.redo() && editor.state.find(c.id)->beat == 4, "redo move");
        check(editor.split(c.id, 6) && editor.state.lanes[7].clips.size() == 2, "split at beat");
        auto left = *editor.state.find(c.id); auto right = editor.state.lanes[7].clips[1];
        check(left.duration == 1 && right.duration == 1 && right.offset == 1 && right.asset == left.asset, "non-destructive split preserves source");
        check(editor.trim(c.id, .25, 0), "trim left");
        check(editor.state.find(c.id)->beat == 4.5 && editor.state.find(c.id)->offset == .25, "trim timing and source offset");
        check(editor.trim(c.id, -.25, 0), "restore trimmed material");
        check(!editor.trim(c.id, -20, 0) && !editor.split(c.id, 4), "reject invalid edits");
        check(editor.remove(second.id) && editor.undo() && editor.state.find(second.id), "delete undo");
        check(editor.undo() && editor.add(1, [&] { auto fresh = second; fresh.id = juce::Uuid().toString(); return fresh; }()) && !editor.canRedo(), "new edit clears redo");
        // A deterministic two-clip arrangement checks silence, source offset,
        // clip gain, fade, pan and exact beat scheduling at the sample boundary.
        Arrangement a; a.bpm = 120; a.lanes[0].gain = 1;
        c.beat = 2; c.duration = .25; c.offset = 1; c.gain = .5f; c.fadeIn = .125;
        a.lanes[0].clips = {c};
        TimelineEngine engine; engine.publish(a); engine.playing = true;
        juce::AudioBuffer<float> audio(2, 12000); audio.clear(); engine.render(audio, 8000);
        check(audio.getSample(0, 7999) == 0 && std::abs(audio.getSample(0, 8000)) < .00000001f, "beat onset and fade begins at zero");
        check(std::abs(audio.getSample(0, 8500) - .0625f) < .0001f, "clip gain and fade midpoint");
        check(std::abs(audio.getSample(0, 9001) - .125f) < .0001f, "unity fade plateau");
        check(audio.getSample(0, 10001) == 0 && !engine.playing, "trim end and transport stop");
        a.lanes[0].pan = 1; engine.publish(a); engine.seek(1.2); engine.playing = true;
        juce::AudioBuffer<float> pan(2, 64); pan.clear(); engine.render(pan, 8000);
        check(pan.getMagnitude(0, 0, 64) == 0 && pan.getMagnitude(1, 0, 64) > .12f, "pan routing");
        a.lanes[0].mute = true; engine.publish(a); engine.seek(1.2); engine.playing = true; pan.clear(); engine.render(pan, 8000);
        check(pan.getMagnitude(0, 64) == 0, "mute");
        a.lanes[0].mute = false; a.lanes[1].solo = true; engine.publish(a); engine.seek(1.2); engine.playing = true; pan.clear(); engine.render(pan, 8000);
        check(pan.getMagnitude(0, 64) == 0, "solo excludes non-solo clips");
        a.lanes[1].solo = false; a.loop = true; a.loopStart = 2; a.loopEnd = 2.5;
        engine.publish(a); engine.seek(1.249); engine.playing = true; pan.clear(); engine.render(pan, 8000);
        check(engine.position() < 1.01 && engine.playing, "loop wraps at configured beat boundary");
        a.loop = false; a.lanes[0].pan = 0;
        auto layered = a; auto overlap = c; overlap.id = juce::Uuid().toString();
        layered.lanes[0].clips.push_back(overlap); engine.publish(layered); engine.seek(1.2); engine.playing = true;
        pan.clear(); engine.render(pan, 8000); check(std::abs(pan.getSample(0, 0) - .25f) < .00001f, "overlapping regions sum");
        auto fade = a; fade.lanes[0].clips[0].fadeOut = .125; engine.publish(fade); engine.seek(1.1875); engine.playing = true;
        pan.clear(); engine.render(pan, 8000); check(std::abs(pan.getSample(0, 0) - .0625f) < .00001f, "fade-out midpoint");
        auto slower = a; slower.bpm = 60; engine.publish(slower); engine.seek(1.2); engine.playing = true;
        pan.clear(); engine.render(pan, 8000); check(pan.getMagnitude(0, 64) == 0, "BPM changes beat anchors");
        check(bounceArrangement(source, a, [] { return false; }).failed() && saveArrangement(source, a).failed(), "source audio cannot be overwritten");
        auto session = dir.getChildFile("song.jaspro");
        check(saveArrangement(session, a).wasOk(), "save v2");
        Arrangement restored; check(loadArrangement(session, restored).wasOk(), "load v2");
        check(restored.lanes[0].clips[0].id == c.id && restored.lanes[0].clips[0].offset == c.offset && restored.bpm == 120 && restored.loopEnd == 2.5, "round trip complete timing");
        auto oldAsset = restored.lanes[0].clips[0].asset;
        check(loadArrangement(session, restored, [] { return true; }).failed() && restored.lanes[0].clips[0].asset == oldAsset, "cancelled load preserves arrangement");
        auto xml = session.loadFileAsString(); session.replaceWithText(xml.replace("bpm=\"120.0\"", "bpm=\"nan\""));
        // Explicit malformed timing fixture independent of XML float formatting.
        auto tree = juce::ValueTree::fromXml(xml); tree.setProperty("bpm", 0, nullptr); session.replaceWithText(tree.toXmlString());
        check(loadArrangement(session, restored).failed() && restored.lanes[0].clips[0].asset == oldAsset, "reject invalid BPM atomically");
        tree = juce::ValueTree::fromXml(xml); auto extra = tree.getChild(0).getChild(0).createCopy(); tree.getChild(1).addChild(extra, -1, nullptr); session.replaceWithText(tree.toXmlString());
        check(loadArrangement(session, restored).failed(), "reject duplicate region IDs");
        auto output = dir.getChildFile("bounce.wav"); check(bounceArrangement(output, a, [] { return false; }).wasOk(), "offline bounce");
        juce::AudioFormatManager formats; formats.registerBasicFormats(); std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(output));
        check(reader && reader->lengthInSamples == 60000, "bounce includes leading silence and exact end");
        juce::AudioBuffer<float> offline(2, 60000); reader->read(&offline, 0, 60000, 0, true, true);
        TimelineEngine live; live.publish(a); live.playing = true; juce::AudioBuffer<float> reference(2, 60000); reference.clear(); live.render(reference, 48000);
        float difference = 0; for (int i = 0; i < 60000; ++i) difference = std::max(difference, std::abs(offline.getSample(0, i) - reference.getSample(0, i)));
        check(difference < .000001f, "offline/live sample parity");
        juce::MemoryBlock before, after; output.loadFileAsData(before);
        check(bounceArrangement(output, a, [] { return true; }).failed(), "cancel bounce"); output.loadFileAsData(after); check(before == after, "cancel preserves destination");
        // Version-one sessions remain readable by the new standalone.
        juce::ValueTree v1("JasProProject"); v1.setProperty("version", 1, nullptr);
        for (int i = 0; i < 8; ++i) { juce::ValueTree lane("Track"); lane.setProperty("gain", .8, nullptr); lane.setProperty("pan", 0, nullptr); if (i == 7) lane.setProperty("source", source.getFullPathName(), nullptr); v1.addChild(lane, -1, nullptr); }
        session.replaceWithText(v1.toXmlString()); check(loadArrangement(session, restored).wasOk() && restored.lanes[7].clips[0].duration == 35, "v1 import migration");
        source.deleteFile(); check(loadArrangement(session, restored).failed(), "missing asset diagnosed");
        std::cout << assertions << " arrangement assertions passed\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << " after " << assertions << " assertions\n"; dir.deleteRecursively(); return 1; }
    dir.deleteRecursively(); return 0;
}
