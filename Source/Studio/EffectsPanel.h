#pragma once
#include "Effects.h"
namespace wingman::studio {
class EffectsPanel final : public juce::Component {
    juce::ComboBox track, preset;
    std::array<juce::ToggleButton, 11> enables;
    std::array<juce::Slider, 11> amounts;
    std::array<float, 8> meters{};
    juce::Label notice;
    Effects value;
    bool syncing = false;
public:
    std::function<void(int, const Effects&)> onChange;
    std::function<void(int)> onSelect;
    EffectsPanel() {
        addAndMakeVisible(track); addAndMakeVisible(preset); addAndMakeVisible(notice);
        for (int i = 0; i < 8; ++i) track.addItem("Track " + juce::String(i + 1), i + 1); track.setSelectedId(1);
        preset.addItem("Dry", 1); preset.addItem("Dub echo", 2); preset.addItem("Lo-fi", 3); preset.addItem("Space", 4); preset.setSelectedId(1);
        track.onChange = [this] { if (onSelect) onSelect(track.getSelectedId() - 1); };
        preset.onChange = [this] { if (syncing) return; Effects p; int id = preset.getSelectedId(); if (id == 2) { p.enabled[7] = true; p.amount[7] = .65f; } if (id == 3) { p.enabled[4] = p.enabled[5] = p.enabled[9] = true; p.amount[4] = .8f; p.amount[5] = .55f; } if (id == 4) p.enabled[3] = p.enabled[8] = true; setEffects(p); if (onChange) onChange(track.getSelectedId() - 1, value); };
        for (std::size_t i = 0; i < 11; ++i) {
            enables[i].setButtonText(effectNames[i]); addAndMakeVisible(enables[i]); addAndMakeVisible(amounts[i]); amounts[i].setRange(0, 1, .001); amounts[i].setSliderStyle(juce::Slider::LinearHorizontal); amounts[i].setTextBoxStyle(juce::Slider::TextBoxRight, false, 65, 25);
            auto change = [this, i] { if (syncing) return; value.enabled[i] = enables[i].getToggleState(); value.amount[i] = static_cast<float>(amounts[i].getValue()); if (onChange) onChange(track.getSelectedId() - 1, value); }; enables[i].onClick = change; amounts[i].onValueChange = change;
        }
        notice.setText("Amount controls are normalized. Utility: 0.5 = unity. EQ is a low-pass filter. Looper is a feedback repeat. Effects stop after a fixed 2-second tail. Automation is not implemented.", juce::dontSendNotification); setEffects({});
    }
    bool testControls() {
        bool called = false; onChange = [&](int track, const Effects& p) { called = track == 0 && p.enabled[7]; }; enables[7].setToggleState(true, juce::dontSendNotification); enables[7].onClick(); bool ok = called;
        onChange = [&](int, const Effects& p) { ok &= p.enabled[4] && p.enabled[5] && p.enabled[9]; }; preset.setSelectedId(3, juce::sendNotificationSync); onChange = {}; return ok;
    }
    int selectedTrack() const { return track.getSelectedId() - 1; }
    void setEffects(const Effects& p) { syncing = true; value = p; for (std::size_t i = 0; i < 11; ++i) { enables[i].setToggleState(p.enabled[i], juce::dontSendNotification); amounts[i].setValue(p.amount[i], juce::dontSendNotification); } syncing = false; }
    void setMeters(std::array<float, 8> peaks) { meters = peaks; repaint(); }
    void resized() override {
        track.setBounds(20, 20, 200, 30); preset.setBounds(240, 20, 200, 30);
        int half = (getWidth() - 60) / 2; for (int i = 0; i < 11; ++i) { int x = 20 + (i / 6) * (half + 20), y = 90 + (i % 6) * 59; enables[static_cast<std::size_t>(i)].setBounds(x, y, 150, 28); amounts[static_cast<std::size_t>(i)].setBounds(x + 155, y, half - 155, 28); }
        notice.setBounds(20, getHeight() - 48, getWidth() - 40, 40);
    }
    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0xff161e2a)); g.setColour(juce::Colours::lightgrey); g.setFont(13); g.drawText("TRACK EFFECTS / identical processing path for timeline playback and WAV bounce", 470, 20, getWidth() - 490, 30, juce::Justification::left);
        int cell = (getWidth() - 40) / 8; for (int i = 0; i < 8; ++i) { int x = 20 + i * cell; float peak = meters[static_cast<std::size_t>(i)]; g.setColour(juce::Colour(0xff0a111a)); g.fillRect(x, std::max(430, getHeight() - 130), cell - 12, 28); g.setColour(peak > 1 ? juce::Colours::orange : juce::Colour(0xff69d7ab)); g.fillRect(x, std::max(430, getHeight() - 130), static_cast<int>((cell - 12) * std::min(1.f, peak)), 28); g.setColour(juce::Colours::white); g.drawText("T" + juce::String(i + 1) + " " + juce::String(juce::Decibels::gainToDecibels(peak, -90.f), 1) + " dB", x, std::max(430, getHeight() - 130) + 33, cell - 12, 22, juce::Justification::left); }
    }
};
}
