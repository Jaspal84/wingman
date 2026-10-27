#pragma once
#include "Generate.h"
namespace wingman::studio {
class ProductionPanel final : public juce::Component {
    juce::Label title, referenceLabel, status;
    juce::ComboBox genre, mode, key, slices, regenerate;
    juce::TextEditor mood, seed, progression;
    juce::TextButton guitar{"Generate guitar audio + MIDI"};
    juce::Slider energy, bars, probability, swing;
    juce::ToggleButton transient{"Use detected onsets (selected source)"}, reverse{"Reverse slices"};
    juce::TextButton generate{"Inspire: new arrangement"}, magic{"Magic Arrange + reference"}, regen{"Regenerate one track"}, flipButton{"Flip selected clip"}, midi{"Export slice MIDI"};
    juce::TextEditor explanation;
    std::optional<Sample> referenceSample;
    std::vector<Region> lastSlices;
public:
    std::function<void(InspireSettings, bool, int)> onGenerate;
    std::function<void(FlipSettings, bool)> onFlip;
    std::function<void()> onMidi;
    std::function<void(juce::String, InspireSettings)> onGuitar;
    ProductionPanel() {
        for (auto* c : std::initializer_list<juce::Component*>{&title, &referenceLabel, &status, &genre, &mode, &key, &slices, &regenerate, &mood, &seed, &progression, &guitar, &energy, &bars, &probability, &swing, &transient, &reverse, &generate, &magic, &regen, &flipButton, &midi, &explanation}) addAndMakeVisible(c);
        title.setText("INSPIRE / MAGIC ARRANGE / FLIP - deterministic local audio", juce::dontSendNotification);
        referenceLabel.setText("Reference: choose Use as reference in Library + Match. Manual tempo/key corrections are supported there.", juce::dontSendNotification);
        genre.addItem("Any user genre", 1); int id = 2; for (auto* name : {"Punjabi Pop", "Bhangra", "Bollywood", "Hip-Hop", "Pop", "Ambient"}) genre.addItem(name, id++); genre.setSelectedId(1);
        mood.setTextToShowWhenEmpty("Mood tag (blank = any)", juce::Colours::grey); seed.setText("42"); seed.setInputRestrictions(10, "0123456789");
        key.addItem("Any key", 1); for (int i = 0; i < 12; ++i) key.addItem(keyName(i, 0), i + 2); key.setSelectedId(1); mode.addItem("Major", 1); mode.addItem("Minor", 2); mode.setSelectedId(1);
        for (auto* slider : {&energy, &bars, &probability, &swing}) { slider->setSliderStyle(juce::Slider::LinearHorizontal); slider->setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 24); }
        energy.setRange(0, 1, .01); energy.setValue(.7); bars.setRange(1, 16, 1); bars.setValue(8); probability.setRange(0, 1, .01); probability.setValue(1); swing.setRange(0, .49, .01);
        for (int value : {4, 8, 16, 32}) slices.addItem(juce::String(value) + " slices", value); slices.setSelectedId(8);
        for (int i = 0; i < 8; ++i) regenerate.addItem("Track " + juce::String(i + 1), i + 1); regenerate.setSelectedId(1);
        auto settings = [this] { InspireSettings s; s.genre = genre.getSelectedId() == 1 ? juce::String{} : genre.getText(); s.mood = mood.getText().trim(); s.key = key.getSelectedId() - 2; s.mode = mode.getSelectedId() - 1; s.seed = static_cast<uint32_t>(seed.getText().getLargeIntValue()); s.energy = energy.getValue(); s.bars = static_cast<int>(bars.getValue()); return s; };
        progression.setText("C Am F G"); progression.setTooltip("Chord-aware synthetic guitar: space-separated chord symbols, optional :beats");
        guitar.onClick = [this, settings] { if (onGuitar) onGuitar(progression.getText(), settings()); };
        generate.onClick = [this, settings] { if (onGenerate) onGenerate(settings(), false, -1); };
        magic.onClick = [this, settings] { if (referenceSample && onGenerate) { auto s = settings(); if (referenceSample->bpm > 0) s.bpm = referenceSample->bpm; if (s.key < 0) { s.key = referenceSample->key; s.mode = referenceSample->mode; } onGenerate(s, true, -1); } };
        regen.onClick = [this, settings] { if (onGenerate) onGenerate(settings(), false, regenerate.getSelectedId() - 1); };
        flipButton.onClick = [this] { if (onFlip) { FlipSettings s; s.slices = slices.getSelectedId(); s.seed = static_cast<uint32_t>(seed.getText().getLargeIntValue()); s.probability = probability.getValue(); s.swing = swing.getValue(); s.reverse = reverse.getToggleState(); onFlip(s, transient.getToggleState()); } };
        midi.onClick = [this] { if (onMidi) onMidi(); }; explanation.setMultiLine(true); explanation.setReadOnly(true); magic.setEnabled(false);
        report("Uses actual user-imported samples and tags. Missing categories remain empty.\nTempo matching uses linked rate/pitch; independent stretch is not implemented.\nFLIP: select a timeline clip, then choose slices/probability/swing and apply.");
    }
    bool testControls() {
        bool generated = false, flipped = false; onGenerate = [&](const auto& s, bool, int) { generated = s.seed == 42 && s.bars == 8; }; onFlip = [&](const auto& s, bool) { flipped = s.slices == 8 && s.probability == 1; }; generate.onClick(); flipButton.onClick(); onGenerate = {}; onFlip = {}; return generated && flipped;
    }
    void setReference(const Sample& s) { referenceSample = s; magic.setEnabled(true); key.setSelectedId(s.key + 2); mode.setSelectedId(s.mode + 1); referenceLabel.setText("Reference: " + juce::File(s.path).getFileName() + " | " + juce::String(s.bpm, 1) + " BPM | " + keyName(s.key, s.mode) + " | correction in Library + Match", juce::dontSendNotification); }
    std::optional<Sample> reference() const { return referenceSample; }
    void report(const juce::String& text) { explanation.setText(text, false); }
    void setBusy(bool b) { for (auto* button : {&generate, &regen, &flipButton, &midi, &guitar}) button->setEnabled(!b); magic.setEnabled(!b && referenceSample.has_value()); }
    void resized() override {
        int w = getWidth(); title.setBounds(20, 10, w - 40, 30); referenceLabel.setBounds(20, 45, w - 40, 28);
        int cell = (w - 40) / 4; genre.setBounds(20, 100, cell - 10, 28); mood.setBounds(20 + cell, 100, cell - 10, 28); key.setBounds(20 + cell * 2, 100, cell - 10, 28); mode.setBounds(20 + cell * 3, 100, cell - 10, 28);
        energy.setBounds(20, 163, cell - 10, 28); bars.setBounds(20 + cell, 163, cell - 10, 28); seed.setBounds(20 + cell * 2, 163, cell - 10, 28); regenerate.setBounds(20 + cell * 3, 163, cell - 10, 28);
        generate.setBounds(20, 210, 230, 32); magic.setBounds(262, 210, 245, 32); regen.setBounds(519, 210, 220, 32);
        progression.setBounds(20, 255, 560, 28); guitar.setBounds(600, 255, 320, 28);
        slices.setBounds(20, 335, 140, 28); probability.setBounds(175, 335, 205, 28); swing.setBounds(395, 335, 205, 28); transient.setBounds(615, 335, 310, 28);
        reverse.setBounds(20, 375, 160, 28); flipButton.setBounds(190, 375, 210, 30); midi.setBounds(415, 375, 200, 30);
        explanation.setBounds(20, 430, w - 40, std::max(80, getHeight() - 450));
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff151d29)); g.setColour(juce::Colours::lightgrey); g.setFont(13); int cell = (getWidth() - 40) / 4;
        const char* top[]{"Genre filter (user tag)", "Mood filter (user tag)", "Target key", "Mode"}; const char* bottom[]{"Energy (RMS preference)", "Arrangement bars (4/4)", "Reproducible seed", "Regeneration destination"};
        for (int i = 0; i < 4; ++i) { g.drawText(top[i], 20 + i * cell, 78, cell - 10, 20, juce::Justification::left); g.drawText(bottom[i], 20 + i * cell, 140, cell - 10, 20, juce::Justification::left); }
        g.drawText("FLIP / original seeded slice order, probability and swing", 20, 293, getWidth() - 40, 28, juce::Justification::left);
        g.drawText("Probability", 175, 315, 200, 20, juce::Justification::left); g.drawText("Swing (offbeat delay)", 395, 315, 200, 20, juce::Justification::left);
    }
};
}
