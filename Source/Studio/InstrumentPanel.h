#pragma once
#include "Instrument.h"
#include "Library/Library.h"
namespace wingman::studio {
class InstrumentPanel final : public juce::Component {
    std::array<juce::TextButton, 16> pads, steps;
    std::array<juce::TextButton, 12> keys;
    std::array<bool, 12> held{};
    juce::TextButton savePreset{"Save preset"}, loadPreset{"Open preset"};
    juce::TextButton assign{"Assign selected library sample"}, assignHard{"Assign high-velocity layer"}, commit{"Commit Drum to timeline"}, midi{"Export Drum MIDI"}, silence{"All notes off"};
    juce::ComboBox preset, mode, midiDevice;
    juce::Array<juce::MidiDeviceInfo> midiDevices;
    std::array<juce::Slider, 5> sliders;
    juce::Label status;
    InstrumentState state;
    DrumPattern pattern;
    int selectedPad = 0;
    void refresh(bool publish = false) {
        for (int i = 0; i < 16; ++i) { auto n = static_cast<std::size_t>(i); pads[n].setButtonText(juce::String(i + 1) + (state.pads[n].soft ? ": " + state.pads[n].soft->source.getFileNameWithoutExtension().substring(0, 14) : " (empty)")); pads[n].setToggleState(selectedPad == i, juce::dontSendNotification); steps[n].setToggleState(pattern.steps[static_cast<std::size_t>(selectedPad)][n] > 0, juce::dontSendNotification); }
        if (publish && onChange) onChange(state);
    }
public:
    std::function<void(const InstrumentState&)> onChange;
    std::function<void(int, int)> onNote;
    std::function<void()> onSilence;
    std::function<void(const juce::String&)> onMidiDevice;
    std::function<void(bool, int)> onAssign;
    std::function<void(bool)> onPreset;
    std::function<void(const InstrumentState&, const DrumPattern&)> onCommit, onMidi;
    InstrumentPanel() {
        for (auto* c : std::initializer_list<juce::Component*>{&savePreset, &loadPreset, &assign, &assignHard, &commit, &midi, &silence, &preset, &mode, &midiDevice, &status}) addAndMakeVisible(c);
        midiDevice.addItem("No MIDI input", 1); midiDevices = juce::MidiInput::getAvailableDevices(); for (int i = 0; i < midiDevices.size(); ++i) midiDevice.addItem(midiDevices[i].name, i + 2); midiDevice.setSelectedId(1);
        midiDevice.onChange = [this] { if (onMidiDevice) { int i = midiDevice.getSelectedId() - 2; onMidiDevice(i >= 0 && i < midiDevices.size() ? midiDevices[i].identifier : juce::String{}); } };
        mode.addItem("Drum pads (MIDI 36-51)", 1); mode.addItem("Play: chromatic pad 1", 2); mode.setSelectedId(1);
        mode.onChange = [this] { state.chromatic = mode.getSelectedId() == 2; for (auto& pad : state.pads) pad.oneShot = !state.chromatic; refresh(true); };
        preset.addItem("Original modern groove", 1); preset.addItem("Original Bhangra/dhol-inspired", 2); preset.addItem("Original tabla-inspired", 3); preset.setSelectedId(1); pattern = drumPreset(0);
        preset.onChange = [this] { pattern = drumPreset(preset.getSelectedId() - 1); pattern.swing = sliders[4].getValue(); refresh(); };
        for (int i = 0; i < 16; ++i) {
            auto n = static_cast<std::size_t>(i); addAndMakeVisible(pads[n]); addAndMakeVisible(steps[n]);
            pads[n].onClick = [this, i] { selectedPad = i; if (onNote) onNote(state.chromatic ? 60 : 36 + i, 100); refresh(); };
            steps[n].setButtonText(juce::String(i + 1)); steps[n].setClickingTogglesState(true); steps[n].onClick = [this, i] { pattern.steps[static_cast<std::size_t>(selectedPad)][static_cast<std::size_t>(i)] = steps[static_cast<std::size_t>(i)].getToggleState() ? .85f : 0; };
        }
        for (int i = 0; i < 12; ++i) { auto n = static_cast<std::size_t>(i); addAndMakeVisible(keys[n]); keys[n].setButtonText(keyName(i, 0).upToFirstOccurrenceOf(" ", false, false)); keys[n].onStateChange = [this, i] { auto n = static_cast<std::size_t>(i); bool down = keys[n].isDown(); if (held[n] != down) { held[n] = down; if (onNote) onNote(60 + i, down ? 100 : 0); } }; }
        for (std::size_t i = 0; i < sliders.size(); ++i) { auto& slider = sliders[i]; addAndMakeVisible(slider); slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 25); slider.setRange(i == 2 ? 0 : .001, i == 2 ? 1 : i == 4 ? .49 : 2, .001); slider.onValueChange = [this] { state.attack = sliders[0].getValue(); state.decay = sliders[1].getValue(); state.sustain = sliders[2].getValue(); state.release = sliders[3].getValue(); pattern.swing = sliders[4].getValue(); refresh(true); }; }
        sliders[0].setValue(.005, juce::dontSendNotification); sliders[1].setValue(.1, juce::dontSendNotification); sliders[2].setValue(.8, juce::dontSendNotification); sliders[3].setValue(.15, juce::dontSendNotification); sliders[4].setValue(.001, juce::dontSendNotification);
        savePreset.onClick = [this] { if (onPreset) onPreset(true); }; loadPreset.onClick = [this] { if (onPreset) onPreset(false); };
        assign.onClick = [this] { if (onAssign) onAssign(false, selectedPad); }; assignHard.onClick = [this] { if (onAssign) onAssign(true, selectedPad); };
        commit.onClick = [this] { if (onCommit) onCommit(state, pattern); }; midi.onClick = [this] { if (onMidi) onMidi(state, pattern); }; silence.onClick = [this] { if (onSilence) onSilence(); };
        status.setText("Assign licensed samples from Library first. Pads 4/5 carry dhol/tabla-inspired parts. MIDI input follows selected mode. No samples bundled.", juce::dontSendNotification); refresh();
    }
    bool testControls(std::shared_ptr<const Asset> asset) {
        bool changed = false, triggered = false; onChange = [&](const auto& s) { changed = s.pads[0].soft == asset; }; onNote = [&](int n, int v) { triggered = n == 36 && v == 100; };
        assignSample(0, false, asset); pads[0].onClick(); steps[1].setToggleState(true, juce::dontSendNotification); steps[1].onClick(); bool ok = changed && triggered && pattern.steps[0][1] > 0;
        onChange = {}; onNote = {}; return ok;
    }
    const DrumPattern& drumPattern() const { return pattern; }
    void restore(InstrumentState next, DrumPattern nextPattern) {
        state = std::move(next); pattern = nextPattern; mode.setSelectedId(state.chromatic ? 2 : 1, juce::dontSendNotification);
        const double values[]{state.attack, state.decay, state.sustain, state.release, pattern.swing}; for (std::size_t i = 0; i < sliders.size(); ++i) sliders[i].setValue(values[i], juce::dontSendNotification); refresh(true);
    }
    const InstrumentState& instrumentState() const { return state; }
    void assignSample(int pad, bool hard, std::shared_ptr<const Asset> asset) { if (hard) state.pads[static_cast<std::size_t>(pad)].hard = std::move(asset); else state.pads[static_cast<std::size_t>(pad)].soft = std::move(asset); refresh(true); }
    void setLoading(bool loading) { for (auto* button : {&assign, &assignHard, &commit, &midi, &savePreset, &loadPreset}) button->setEnabled(!loading); }
    void report(const juce::String& text) { status.setText(text, juce::dontSendNotification); }
    void resized() override {
        int w = getWidth(); mode.setBounds(20, 20, 280, 30); preset.setBounds(315, 20, 310, 30); silence.setBounds(640, 20, 150, 30); midiDevice.setBounds(805, 20, getWidth() - 825, 30);
        for (int i = 0; i < 16; ++i) { int col = i % 4, row = i / 4; pads[static_cast<std::size_t>(i)].setBounds(20 + col * 180, 77 + row * 62, 168, 50); steps[static_cast<std::size_t>(i)].setBounds(20 + i * ((w - 40) / 16), 366, (w - 40) / 16 - 5, 35); }
        int right = 755; const double values[]{state.attack, state.decay, state.sustain, state.release, pattern.swing}; juce::ignoreUnused(values);
        for (int i = 0; i < 5; ++i) sliders[static_cast<std::size_t>(i)].setBounds(right, 88 + i * 48, std::max(230, w - right - 20), 29);
        assign.setBounds(20, 418, 265, 30); assignHard.setBounds(300, 418, 250, 30); commit.setBounds(565, 418, 220, 30); midi.setBounds(800, 418, 180, 30);
        for (int i = 0; i < 12; ++i) keys[static_cast<std::size_t>(i)].setBounds(20 + i * ((w - 40) / 12), 493, (w - 40) / 12 - 3, 58);
        savePreset.setBounds(20, 565, 140, 28); loadPreset.setBounds(175, 565, 140, 28); status.setBounds(330, getHeight() - 39, w - 350, 32);
    }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff151c28)); g.setColour(juce::Colours::lightgrey); g.setFont(13); const char* names[]{"Attack (seconds)", "Decay", "Sustain", "Release", "Swing"}; for (int i = 0; i < 5; ++i) g.drawText(names[i], 755, 67 + i * 48, getWidth() - 775, 20, juce::Justification::left); g.drawText("16 STEPS / selected pad - commit pattern to the editable timeline for transport playback", 20, 335, getWidth() - 40, 25, juce::Justification::left); g.drawText("PLAY / hold keys (chromatic mode, pad 1; C4 root)", 20, 460, getWidth() - 40, 25, juce::Justification::left); }
};
}
