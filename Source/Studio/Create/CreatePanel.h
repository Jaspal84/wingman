#pragma once
#include "Combination.h"
namespace wingman::studio {
inline juce::Colour roleColour(int role) { return juce::Colour(layerColours[static_cast<std::size_t>(juce::jlimit(0, 7, role))]); }
class CreatePanel final : public juce::Component {
    class Card final : public juce::Component, public juce::SettableTooltipClient, public juce::DragAndDropTarget {
        CreatePanel& owner; int role;
        juce::ToggleButton lock{"Lock"}; juce::TextButton replace{"Replace"};
    public:
        Card(CreatePanel& p, int r) : owner(p), role(r) {
            setComponentID("create-layer-" + juce::String(role)); addAndMakeVisible(lock); addAndMakeVisible(replace); lock.setTooltip("Keep this layer's actual clips when generating. Global tempo/key changes may affect compatibility.");
            lock.onClick = [this] { if (owner.onLock) owner.onLock(role, lock.getToggleState()); }; replace.onClick = [this] { if (owner.onReplace) owner.onReplace(role); };
        }
        void update() { auto n = static_cast<std::size_t>(role); bool present = !owner.combination.arrangement.lanes[n].clips.empty(); lock.setEnabled(present && !owner.busy); lock.setToggleState(owner.combination.locked[n], juce::dontSendNotification); replace.setEnabled(!owner.busy && !owner.combination.locked[n]); repaint(); }
        bool isInterestedInDragSource(const SourceDetails& d) override { return !owner.busy && !owner.combination.locked[static_cast<std::size_t>(role)] && juce::File(d.description.toString()).hasFileExtension("wav;aif;aiff;flac"); }
        void itemDropped(const SourceDetails& d) override { if (owner.onAssign) owner.onAssign(role, juce::File(d.description.toString())); }
        void resized() override { lock.setBounds(getWidth() - 80, 5, 76, 24); replace.setBounds(getWidth() - 80, 32, 74, 23); }
        void paint(juce::Graphics& g) override {
            auto n = static_cast<std::size_t>(role); bool present = !owner.combination.arrangement.lanes[n].clips.empty(); auto r = getLocalBounds().toFloat().reduced(1);
            g.setColour(roleColour(role).withAlpha(present ? .21f : .07f)); g.fillRoundedRectangle(r, 7);
            g.setColour(roleColour(role).withAlpha(present ? .95f : .4f)); g.fillRoundedRectangle(1, 1, 4, r.getHeight(), 2);
            g.setFont(juce::FontOptions(14, juce::Font::bold)); g.setColour(present ? roleColour(role).brighter(.2f) : juce::Colour(0xffabb0c2)); g.drawText(layerNames[n], 15, 6, getWidth() - 103, 20, juce::Justification::left);
            g.setFont(12); g.setColour(juce::Colour(0xffc8cad5));
            auto text = present ? sourceSummary(owner.combination.arrangement.lanes[n]) : juce::String("No matching local audio");
            g.drawText(text, 15, 30, getWidth() - 105, 22, juce::Justification::left, true);
            setTooltip(owner.combination.explanation[n]);
        }
    };
    juce::ComboBox genre, style, key;
    juce::Slider bpm;
    juce::TextEditor seed;
    juce::ComboBox bars;
    juce::TextButton generate{"Generate"}, regenerate{"Regenerate"}, preview{"Preview combination"}, send{"To Arrangement"}, wav{"Export WAV"}, midi{"MIDI triggers"}, cancel{"Cancel"};
    juce::Label status;
    juce::Viewport viewport;
    juce::Component cards;
    std::array<std::unique_ptr<Card>, 8> layers;
    Combination combination;
    bool busy = false;
public:
    std::function<void(bool)> onGenerate;
    std::function<void(int)> onReplace;
    std::function<void(int, bool)> onLock;
    std::function<void(int, const juce::File&)> onAssign;
    std::function<void()> onPreview, onSend, onWav, onMidi, onCancel, onTargetChange;
    CreatePanel() {
        for (auto* c : std::initializer_list<juce::Component*>{&genre, &style, &key, &bpm, &seed, &bars, &generate, &regenerate, &preview, &send, &wav, &midi, &cancel, &status, &viewport}) addAndMakeVisible(c);
        viewport.setViewedComponent(&cards, false); viewport.setScrollBarsShown(true, false);
        for (int i = 0; i < 8; ++i) { layers[static_cast<std::size_t>(i)] = std::make_unique<Card>(*this, i); cards.addAndMakeVisible(*layers[static_cast<std::size_t>(i)]); }
        genre.addItem("Any genre", 1); int id = 2; for (auto* name : {"Punjabi Pop", "Bhangra", "Bollywood", "Hip-Hop", "Pop", "Ambient"}) genre.addItem(name, id++); genre.setSelectedId(1, juce::dontSendNotification);
        style.addItem("Balanced", 1); style.addItem("Sparse", 2); style.addItem("Driving", 3); style.setSelectedId(1, juce::dontSendNotification);
        key.addItem("Any musical key", 1); for (int mode = 0; mode < 2; ++mode) for (int root = 0; root < 12; ++root) key.addItem(keyName(root, mode), root + mode * 12 + 2); key.setSelectedId(1, juce::dontSendNotification);
        bpm.setRange(40, 240, .1); bpm.setValue(120, juce::dontSendNotification); bpm.setSliderStyle(juce::Slider::LinearHorizontal); bpm.setTextValueSuffix(" BPM"); bpm.setTextBoxStyle(juce::Slider::TextBoxRight, false, 66, 27); bpm.setTooltip("Composition BPM. Tempo matching uses linked sample rate and pitch.");
        for (int count : {1, 2, 4, 8}) bars.addItem(juce::String(count) + " bars", count); bars.setSelectedId(4, juce::dontSendNotification);
        seed.setText("42"); seed.setInputRestrictions(9, "0123456789"); seed.setTooltip("Reproducible selection seed. Regenerate increments it.");
        generate.onClick = [this] { if (onGenerate) onGenerate(false); }; regenerate.onClick = [this] { if (onGenerate) onGenerate(true); };
        preview.onClick = [this] { if (onPreview) onPreview(); }; send.onClick = [this] { if (onSend) onSend(); }; wav.onClick = [this] { if (onWav) onWav(); }; midi.onClick = [this] { if (onMidi) onMidi(); }; cancel.onClick = [this] { if (onCancel) onCancel(); };
        key.onChange = [this] { if (onTargetChange) onTargetChange(); }; bpm.onValueChange = [this] { if (onTargetChange) onTargetChange(); };
        midi.setTooltip("Paired sample-trigger events for actual generated layers. No audio-to-MIDI transcription. Map notes 36-43 to the source layers in your sampler.");
        status.setFont(juce::FontOptions(12)); status.setMinimumHorizontalScale(1.f); status.setJustificationType(juce::Justification::topLeft); status.setText("Index licensed sounds, add instrument tags, then Generate. Empty roles stay empty.", juce::dontSendNotification); update({}, false);
    }
    ~CreatePanel() override { viewport.setViewedComponent(nullptr, false); }
    InspireSettings settings() const {
        InspireSettings s; s.bpm = bpm.getValue(); int id = key.getSelectedId() - 2; s.key = id < 0 ? -1 : id % 12; s.mode = id < 0 ? 0 : id / 12;
        s.genre = genre.getSelectedId() == 1 ? juce::String{} : genre.getText(); s.bars = bars.getSelectedId(); s.seed = static_cast<uint32_t>(seed.getText().getLargeIntValue()); s.energy = style.getSelectedId() == 2 ? .35 : style.getSelectedId() == 3 ? .95 : .7; return s;
    }
    void setBpm(double value) { bpm.setValue(value, juce::sendNotificationSync); }
    void incrementSeed() { auto value = static_cast<uint32_t>(seed.getText().getLargeIntValue()); seed.setText(juce::String((value + 1) % 1000000000U)); }
    void report(const juce::String& text) { status.setText(text, juce::dontSendNotification); status.setTooltip(text); }
    void setCancellable(bool allowed) { cancel.setEnabled(allowed); }
    void setPlaying(bool playing) { preview.setButtonText(playing ? "Stop combination" : "Preview combination"); }
    void update(const Combination& next, bool working) {
        combination = next; busy = working; bool present = combination.audible(); bool allLocked = std::all_of(combination.locked.begin(), combination.locked.end(), [](bool locked) { return locked; }); generate.setEnabled(!busy && !allLocked); regenerate.setEnabled(!busy && !allLocked); generate.setTooltip(allLocked ? "Unlock a layer to generate alternatives" : "Build from real indexed local audio"); regenerate.setTooltip(allLocked ? "Unlock a layer to regenerate" : "Advance the seed and keep locked clips");
        for (auto* b : {&preview, &send, &wav, &midi}) b->setEnabled(present && !busy); cancel.setEnabled(busy);
        for (auto* c : std::initializer_list<juce::Component*>{&genre, &style, &key, &bpm, &seed, &bars}) c->setEnabled(!busy);
        for (auto& layer : layers) layer->update();
    }
    bool testControls() {
        bool generated = false, replaced = false, sent = false; onGenerate = [&](bool) { generated = settings().bpm == 120; }; onReplace = [&](int role) { replaced = role == 1; }; onSend = [&] { sent = true; };
        generate.onClick(); if (onReplace) onReplace(1); send.onClick(); onGenerate = {}; onReplace = {}; onSend = {}; return generated && replaced && sent;
    }
    void resized() override {
        int w = getWidth(); genre.setBounds(18, 67, (w - 42) / 2, 30); style.setBounds(24 + (w - 42) / 2, 67, (w - 42) / 2, 30);
        bpm.setBounds(18, 105, w / 2 - 24, 30); key.setBounds(w / 2 + 1, 105, w / 2 - 19, 30);
        generate.setBounds(18, 148, (w - 42) / 2, 32); regenerate.setBounds(24 + (w - 42) / 2, 148, (w - 42) / 2, 32);
        viewport.setBounds(14, 192, w - 28, std::max(64, getHeight() - 354)); cards.setSize(viewport.getWidth() - 14, 8 * 66);
        for (int i = 0; i < 8; ++i) layers[static_cast<std::size_t>(i)]->setBounds(0, i * 66, cards.getWidth(), 60);
        int y = getHeight() - 152; seed.setBounds(51, y, 48, 26); bars.setBounds(109, y, 95, 26); cancel.setBounds(w - 102, y, 84, 26);
        preview.setBounds(18, y + 34, (w - 42) / 2, 30); send.setBounds(24 + (w - 42) / 2, y + 34, (w - 42) / 2, 30);
        wav.setBounds(18, y + 72, (w - 42) / 2, 29); midi.setBounds(24 + (w - 42) / 2, y + 72, (w - 42) / 2, 29);
        status.setBounds(18, y + 107, w - 36, 42);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff1d2028)); g.setColour(juce::Colour(0xff373b47)); g.drawVerticalLine(0, 0, static_cast<float>(getHeight()));
        g.setFont(juce::FontOptions(25, juce::Font::bold)); g.setColour(juce::Colours::white); g.drawText("AI CREATE", 18, 13, getWidth() - 36, 32, juce::Justification::left);
        g.setFont(12); g.setColour(juce::Colour(0xffa6a9b9)); g.drawText("Local sounds. Your musical choices.", 18, 44, getWidth() - 36, 18, juce::Justification::left);
        g.setFont(11); g.drawText("Seed", 18, getHeight() - 151, 31, 24, juce::Justification::left);
    }
};
}
