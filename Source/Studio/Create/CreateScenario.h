#pragma once
#include "CreateWorkspace.h"
#include "StudioTheme.h"
#include <cstdio>
namespace wingman::studio {
// Explicit headless integration fixture. Never constructed during normal startup.
class CreateScenario final : private juce::Timer {
    struct Log final : juce::Logger { bool asserted = false; void logMessage(const juce::String& text) override { asserted |= text.containsIgnoreCase("Assertion failure"); std::fprintf(stderr, "%s\n", text.toRawUTF8()); } } log;
    juce::Logger* previousLogger = nullptr;
    StudioTheme theme;
    juce::File root;
    std::unique_ptr<CreateWorkspace> workspace, emptyWorkspace;
    juce::String emptyMessage, lastMessage, keptGuitar;
    class Frame final : public juce::Component { void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff11141a)); } } frame;
    juce::Label caption;
    std::function<void(bool)> completion;
    Combination original, regenerated;
    int stage = 0;
    double started = 0;
    bool previewed = false, sent = false;
    bool screenshots = false;
    std::string failure;
    static juce::Button* button(juce::Component& c, const juce::String& name) { if (auto* b = dynamic_cast<juce::Button*>(&c); b && b->getButtonText() == name) return b; for (auto* child : c.getChildren()) if (auto* found = button(*child, name)) return found; return nullptr; }
    static juce::Component* identified(juce::Component& c, const juce::String& id) { if (c.getComponentID() == id) return &c; for (auto* child : c.getChildren()) if (auto* found = identified(*child, id)) return found; return nullptr; }
    static bool contained(juce::Component& c) { if (dynamic_cast<juce::Viewport*>(&c)) return true; for (auto* child : c.getChildren()) if (child->isVisible()) { if (!c.getLocalBounds().contains(child->getBounds()) || !contained(*child)) return false; } return true; }
    static void sound(const juce::File& file, double frequency) {
        juce::WavAudioFormat format; auto stream = file.createOutputStream(); std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(), 8000, 1, 24, {}, 0)); if (!writer) throw std::runtime_error("Cannot write integration fixture"); stream.release(); juce::AudioBuffer<float> samples(1, 32000);
        for (int i = 0; i < 32000; ++i) { double t = i / 8000.0; double envelope = .15 + .85 * std::sin(juce::MathConstants<double>::pi * std::fmod(t, .5) / .5); samples.setSample(0, i, static_cast<float>(.07 * envelope * (std::sin(2 * juce::MathConstants<double>::pi * frequency * t) + .2 * std::sin(4 * juce::MathConstants<double>::pi * frequency * t)))); }
        if (!writer->writeFromAudioSampleBuffer(samples, 0, 32000)) throw std::runtime_error("Cannot write synthetic audio");
    }
    void require(bool ok, const char* reason) { if (!ok) throw std::runtime_error(reason); }
    void click(const juce::String& name, juce::Component* in = nullptr) { auto* b = button(in ? *in : *workspace, name); require(b && b->isEnabled(), ("Missing/disabled button: " + name).toRawUTF8()); if (auto* text = dynamic_cast<juce::TextButton*>(b); text && text->getClickingTogglesState()) text->setToggleState(!text->getToggleState(), juce::dontSendNotification); else if (dynamic_cast<juce::ToggleButton*>(b)) b->setToggleState(!b->getToggleState(), juce::dontSendNotification); if (b->onClick) b->onClick(); }
    void finish(bool passed) { stopTimer(); std::fprintf(stderr, "CREATE GUI workflow: %s%s\n", passed && !log.asserted ? "PASS" : "FAIL ", failure.c_str()); if (completion) completion(passed && !log.asserted); }
    void timerCallback() override {
        try {
            if (juce::Time::getMillisecondCounterHiRes() - started > 30000) throw std::runtime_error("Timed out waiting for UI generation");
            if (stage == 4) { if (emptyWorkspace->isWorking()) return; require(!emptyWorkspace->currentCombination().audible() && emptyMessage.contains("no matching content"), "Empty library did not show useful missing-content feedback"); finish(true); return; }
            if (workspace->isWorking()) return;
            const auto& current = workspace->currentCombination();
            if (stage == 0) {
                require(current.audible(), "Generate button produced no actual audio"); for (const auto& lane : current.arrangement.lanes) require(!lane.clips.empty(), "Missing fixture layer"); original = current;
                auto* card = identified(*workspace, "create-layer-0"); require(card, "Drums card missing"); click("Lock", card); require(workspace->currentCombination().locked[0], "Lock button did not preserve layer"); click("Regenerate"); stage = 1;
            } else if (stage == 1) {
                require(current.seed != original.seed, "Regenerate did not advance seed"); require(current.arrangement.lanes[0].clips[0].id == original.arrangement.lanes[0].clips[0].id, "Locked clips changed during regeneration"); regenerated = current;
                auto* bass = identified(*workspace, "create-layer-1"); require(bass, "Bass card missing"); click("Replace", bass); stage = 2;
            } else if (stage == 3) {
                require(current.arrangement.lanes[4].clips[0].id == keptGuitar && lastMessage.contains("No alternative"), "Unavailable replacement did not retain audio and explain why");
                emptyWorkspace = std::make_unique<CreateWorkspace>(root.getChildFile("empty.sqlite")); emptyWorkspace->onMessage = [this](const auto& text) { emptyMessage = text; }; click("Generate", emptyWorkspace.get()); stage = 4;
            } else if (stage == 2) {
                require(current.arrangement.lanes[1].clips[0].asset->source != regenerated.arrangement.lanes[1].clips[0].asset->source, "Replace did not choose an alternative source");
                for (int i = 0; i < 8; ++i) if (i != 1) require(current.arrangement.lanes[i].clips[0].id == regenerated.arrangement.lanes[i].clips[0].id, "Replace modified another layer");
                click("Preview combination"); require(previewed, "Preview button did not render actual audio"); click("To Arrangement"); require(sent, "Timeline transfer/persistence failed");
                for (auto size : {juce::Point<int>(1068, 575), juce::Point<int>(1468, 775), juce::Point<int>(2368, 1335)}) { workspace->setSize(size.x, size.y); require(contained(*workspace), "Three-column content escaped its panel"); }
                auto highDpi = workspace->createComponentSnapshot(workspace->getLocalBounds(), true, 2.0f); require(highDpi.isValid() && highDpi.getWidth() == workspace->getWidth() * 2, "High-DPI render size incorrect");
                if (screenshots) { frame.setSize(1500, 960); workspace->setBounds(16, 55, 1468, 880); caption.setBounds(20, 8, 1460, 34); auto image = frame.createComponentSnapshot(frame.getLocalBounds()); juce::File file("/tmp/jaspro-create-populated.png"); auto out = file.createOutputStream(); if (out) { out->setPosition(0); out->truncate(); juce::PNGImageFormat png; require(png.writeImageToStream(image, *out), "Cannot save screenshot"); } }
                keptGuitar = current.arrangement.lanes[4].clips[0].id; auto* guitar = identified(*workspace, "create-layer-4"); require(guitar, "Guitar card missing"); click("Replace", guitar); stage = 3;
            }
        } catch (const std::exception& e) { failure = e.what(); finish(false); }
    }
public:
    explicit CreateScenario(bool capture) : screenshots(capture) {
        previousLogger = juce::Logger::getCurrentLogger(); juce::Logger::setCurrentLogger(&log);
        root = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("jaspro-create-ui", "", true); root.createDirectory();
    }
    ~CreateScenario() override { stopTimer(); emptyWorkspace.reset(); workspace.reset(); frame.setLookAndFeel(nullptr); root.deleteRecursively(); juce::Logger::setCurrentLogger(previousLogger); }
    void start(std::function<void(bool)> done) {
        completion = std::move(done);
        try {
            auto db = root.getChildFile("library.sqlite"); Library library(db);
            for (int i = 0; i < 8; ++i) { auto file = root.getChildFile("Validation " + juce::String(layerNames[static_cast<std::size_t>(i)]) + ".wav"); sound(file, 80 + i * 31); auto sample = library.index(file); library.annotate(sample.id, layerNames[static_cast<std::size_t>(i)], "Original validation", "Warm", "Synthetic validation", i == 0, 120, 0, 0); }
            auto alt = root.getChildFile("Validation Bass alternative.wav"); sound(alt, 67); auto sample = library.index(alt); library.annotate(sample.id, "Bass", "Original validation", "Warm", "Synthetic validation", false, 120, 0, 0); library.scan(root);
            workspace = std::make_unique<CreateWorkspace>(db); frame.setLookAndFeel(&theme); frame.addAndMakeVisible(*workspace); frame.addAndMakeVisible(caption); caption.setText("JAS PRO / AI CREATE - synthetic integration-test audio, not a bundled catalogue", juce::dontSendNotification); caption.setColour(juce::Label::textColourId, juce::Colours::white); frame.setSize(1500, 960); workspace->setBounds(16, 55, 1468, 880);
            workspace->onMessage = [this](const auto& text) { lastMessage = text; };
            workspace->onPreview = [this](const Arrangement& arrangement) { TimelineEngine engine; engine.publish(arrangement); engine.playing = true; juce::AudioBuffer<float> block(2, 2048); block.clear(); engine.render(block, 48000); previewed = block.getMagnitude(0, 2048) > .01f; };
            workspace->onSend = [this](const Arrangement& arrangement) { Editor editor; auto before = editor.state; editor.state = arrangement; editor.commit(before); auto file = root.getChildFile("sent.jaspro"); Arrangement loaded; auto saved = saveArrangement(file, editor.state); auto opened = loadArrangement(file, loaded); sent = saved.wasOk() && opened.wasOk() && loaded.lanes[4].name == "Guitar" && editor.undo() && editor.count() == 0; };
            require(!button(*workspace, "Export WAV")->isEnabled(), "Empty export must be disabled"); click("Generate"); started = juce::Time::getMillisecondCounterHiRes(); startTimerHz(20);
        } catch (const std::exception& e) { failure = e.what(); finish(false); }
    }
};
}
