#pragma once
#include "Arrangement.h"
namespace wingman::studio {
class MixerPanel final : public juce::Component {
    std::array<juce::Slider, 8> volume, pan;
    std::array<juce::TextButton, 8> mute, solo;
    std::array<juce::String, 8> names;
    std::array<float, 8> peaks{};
    float master = 0;
    bool syncing = false;
public:
    std::function<void(int, float, float, bool, bool)> onChange;
    MixerPanel() {
        for (std::size_t i = 0; i < 8; ++i) {
            volume[i].setRange(0, 1, .001); volume[i].setSliderStyle(juce::Slider::LinearVertical); volume[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 25); volume[i].setTooltip("Track gain; 1 = unity");
            pan[i].setRange(-1, 1, .01); pan[i].setSliderStyle(juce::Slider::LinearHorizontal); pan[i].setTextBoxStyle(juce::Slider::TextBoxBelow, false, 65, 22); pan[i].setTooltip("Stereo pan; 0 = centre");
            mute[i].setButtonText("Mute"); solo[i].setButtonText("Solo"); mute[i].setClickingTogglesState(true); solo[i].setClickingTogglesState(true);
            for (auto* c : std::initializer_list<juce::Component*>{&volume[i], &pan[i], &mute[i], &solo[i]}) addAndMakeVisible(c);
            auto update = [this, i] { if (!syncing && onChange) onChange(static_cast<int>(i), static_cast<float>(volume[i].getValue()), static_cast<float>(pan[i].getValue()), mute[i].getToggleState(), solo[i].getToggleState()); };
            volume[i].onValueChange = update; pan[i].onValueChange = update; mute[i].onClick = update; solo[i].onClick = update;
        }
    }
    void setState(const Arrangement& state) { syncing = true; static constexpr const char* legacy[]{"Kick", "Snare", "Hi-Hat", "Bass", "Chords", "Melody", "Vocals", "FX"}; for (std::size_t i = 0; i < 8; ++i) { auto& lane = state.lanes[i]; names[i] = lane.name.isEmpty() ? legacy[i] : lane.name; volume[i].setValue(lane.gain, juce::dontSendNotification); pan[i].setValue(lane.pan, juce::dontSendNotification); mute[i].setToggleState(lane.mute, juce::dontSendNotification); solo[i].setToggleState(lane.solo, juce::dontSendNotification); } syncing = false; repaint(); }
    void setMeters(std::array<float, 8> values, float output) { peaks = values; master = output; repaint(); }
    bool testControls() { bool changed = false; onChange = [&](int i, float v, float, bool, bool) { changed = i == 2 && std::abs(v - .4f) < .001f; }; volume[2].setValue(.4, juce::sendNotificationSync); onChange = {}; return changed; }
    void resized() override { int cell = (getWidth() - 52) / 8; for (int i = 0; i < 8; ++i) { auto n = static_cast<std::size_t>(i); int x = 22 + i * cell; volume[n].setBounds(x + 12, 90, cell - 42, std::max(120, getHeight() - 310)); pan[n].setBounds(x + 5, getHeight() - 195, cell - 18, 45); mute[n].setBounds(x + 5, getHeight() - 137, cell - 18, 29); solo[n].setBounds(x + 5, getHeight() - 101, cell - 18, 29); } }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff16191f)); g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(21, juce::Font::bold)); g.drawText("MIXER", 24, 12, 250, 32, juce::Justification::left); int cell = (getWidth() - 52) / 8;
        for (int i = 0; i < 8; ++i) { int x = 22 + i * cell; g.setColour(juce::Colour(0xff343a48)); g.drawVerticalLine(x + cell - 5, 66, static_cast<float>(getHeight() - 65)); g.setColour(juce::Colours::white); g.setFont(13); g.drawText(names[static_cast<std::size_t>(i)], x, 54, cell - 12, 23, juce::Justification::centred); int height = std::max(120, getHeight() - 336); g.setColour(juce::Colour(0xff252d38)); g.fillRect(x + cell - 27, 96, 9, height); g.setColour(juce::Colour(0xff7bd8b6)); int level = static_cast<int>(height * std::min(1.f, peaks[static_cast<std::size_t>(i)])); g.fillRect(x + cell - 27, 96 + height - level, 9, level); }
        g.setColour(master >= 1 ? juce::Colours::orange : juce::Colour(0xffa4d6c8)); g.setFont(12); g.drawText("Master output peak: " + juce::String(juce::Decibels::gainToDecibels(master, -90.f), 1) + " dBFS | Audio device and export settings use the transport bar.", 24, getHeight() - 45, getWidth() - 48, 27, juce::Justification::left);
    }
};
}
