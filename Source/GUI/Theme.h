#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
namespace wingman {
inline const juce::Colour background{0xff10151a}, panel{0xff1c2329}, accent{0xff9b8aff};
inline const std::array<juce::Colour, 4> trackColours{juce::Colour{0xffb09aff}, juce::Colour{0xff69c9ef}, juce::Colour{0xfff18d89}, juce::Colour{0xff66d8b3}};
class Theme : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& colour, bool over, bool down) override {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        const auto fill = down ? colour.brighter(0.12f) : over ? colour.brighter(0.06f) : colour;
        g.setGradientFill(juce::ColourGradient(fill.brighter(0.05f),0,bounds.getY(),fill.darker(0.12f),0,bounds.getBottom(),false));
        g.fillRoundedRectangle(bounds,7);
        g.setColour(button.hasKeyboardFocus(true) ? accent : juce::Colour{0xff43515e});
        g.drawRoundedRectangle(bounds,7,1);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float position, float start, float end, juce::Slider&) override {
        auto bounds = juce::Rectangle<float>(static_cast<float>(x),static_cast<float>(y),
                                            static_cast<float>(width),static_cast<float>(height)).reduced(6);
        const float radius = std::min(bounds.getWidth(),bounds.getHeight())/2;
        juce::Path arc; arc.addCentredArc(bounds.getCentreX(),bounds.getCentreY(),radius,radius,0,start,end,true);
        g.setColour(juce::Colour{0xff384550}); g.strokePath(arc,juce::PathStrokeType(3));
        juce::Path value; value.addCentredArc(bounds.getCentreX(),bounds.getCentreY(),radius,radius,0,start,start+position*(end-start),true);
        g.setColour(juce::Colour{0xff8dd5e7}); g.strokePath(value,juce::PathStrokeType(3));
        g.setColour(juce::Colour{0xff29333d}); g.fillEllipse(bounds.reduced(4));
        const float angle = start+position*(end-start)-juce::MathConstants<float>::halfPi;
        g.setColour(juce::Colours::white);
        g.drawLine(bounds.getCentreX(),bounds.getCentreY(),bounds.getCentreX()+std::cos(angle)*(radius-6),bounds.getCentreY()+std::sin(angle)*(radius-6),2);
    }
    Theme() {
        setColour(juce::ResizableWindow::backgroundColourId, background);
        setColour(juce::TextButton::buttonColourId, juce::Colour{0xff30364b});
        setColour(juce::TextButton::buttonOnColourId, accent.darker());
        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accent);
        setColour(juce::ComboBox::backgroundColourId, panel);
        setColour(juce::TextEditor::backgroundColourId, panel);
        setColour(juce::TextEditor::textColourId, juce::Colours::white);
    }
};
}
