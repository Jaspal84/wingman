#pragma once
#include "GUI/Theme.h"
namespace wingman::studio {
// Standalone-only original styling; the existing plugin look-and-feel is untouched.
class StudioTheme final : public Theme {
public:
    StudioTheme() {
        setColour(juce::TextButton::buttonColourId, juce::Colour(0xff292d38)); setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff514c83));
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff242832)); setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff20242e));
        setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff393e4d)); setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xffb2a2f5));
    }
    juce::Font getTextButtonFont(juce::TextButton&, int) override { return juce::Font(juce::FontOptions(13)); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour& c, bool over, bool down) override {
        auto r = b.getLocalBounds().toFloat().reduced(.5f); auto fill = b.getToggleState() ? b.findColour(juce::TextButton::buttonOnColourId) : c;
        if (b.getComponentID() == "workspace-tab") { g.setColour(b.getToggleState() ? juce::Colour(0xff313048) : juce::Colour(0xff191c23)); g.fillRoundedRectangle(r, 6); if (b.getToggleState()) { g.setColour(juce::Colour(0xffb9a6ff)); g.fillRoundedRectangle(r.getX() + 10, r.getBottom() - 3, r.getWidth() - 20, 2, 1); } return; }
        g.setColour((down ? fill.brighter(.13f) : over ? fill.brighter(.07f) : fill).withMultipliedAlpha(b.isEnabled() ? 1.f : .4f)); g.fillRoundedRectangle(r, 5);
        g.setColour(b.hasKeyboardFocus(true) ? juce::Colour(0xffb2a2f5) : juce::Colour(0xff3c414f)); g.drawRoundedRectangle(r, 5, 1);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override {
        if (b.getComponentID() == "favourite") { juce::Path star; star.addStar(b.getLocalBounds().toFloat().getCentre(), 5, 4, 9); g.setColour(b.getToggleState() ? juce::Colour(0xffedca7a) : juce::Colour(0xff858b9d)); g.fillPath(star); return; }
        g.setFont(getTextButtonFont(b, b.getHeight())); g.setColour(b.findColour(b.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId).withMultipliedAlpha(b.isEnabled() ? 1.f : .42f));
        bool navigation = b.getComponentID() == "library-nav"; g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(navigation ? 12 : 6, 3), navigation ? juce::Justification::centredLeft : juce::Justification::centred, 1);
    }
};
}
