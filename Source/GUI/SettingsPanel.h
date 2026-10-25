#pragma once
#include "../PluginProcessor.h"
#include "Theme.h"
namespace wingman {
class SettingsPanel final:public juce::Component {
    WingmanProcessor& processor;
    juce::TextButton models{"Configure local model"},about{"About JASWINGMAN PRO"};
    juce::TextEditor details;
public:
    SettingsPanel(WingmanProcessor& p,std::function<void()> openModels):processor(p){
        addAndMakeVisible(models);addAndMakeVisible(about);addAndMakeVisible(details);details.setMultiLine(true);details.setReadOnly(true);details.setScrollbarsShown(true);
        models.onClick=std::move(openModels);about.onClick=[] {juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"JASWINGMAN PRO 1.0","AI music assistant • Stems • Chords • Guitar • MIDI\nOriginal C++20/JUCE application.\nGuitar audio: basic original synthesis. Ideas: seeded style rules.\nHTDemucs: local verified MIT model, explicit installation only.\nLogic/M1 acceptance requires physical Mac testing.");};refresh();
    }
    void refresh(){const auto config=processor.modelConfig();details.setText("JASWINGMAN PRO 1.0\n\nFive independent input tracks. The four-stem model does not isolate Guitar.\n\nPython runtime:\n"+config.first+"\n\nModel directory:\n"+config.second+"\n\nNo automatic model downloads. Configure and verify the installed model in Model Manager.\n\nGuitar Studio uses original plucked-string synthesis, not realistic commercial samples.\nCinematic ideas use transparent style rules rather than a trained generative model.\n\nDAW Sync follows the host. Disable it to apply analyzed or tapped project tempo.\nUse the standalone Options menu to configure audio devices; plugin audio devices are managed by the DAW.\n\nAudio/MIDI external dragging requires host acceptance. Export WAV and Export MIDI remain available.\nDrag files expire after seven days through background cleanup. Copy imported audio into your DAW project.\n\nAU identity: aumf / Wgm1 / Wgma\nBundle: com.wingmanaudio.wingman\nLegacy four-track states remain readable.");}
    void resized()override{auto area=getLocalBounds().reduced(24);area.removeFromTop(50);auto buttons=area.removeFromTop(36);models.setBounds(buttons.removeFromLeft(260));buttons.removeFromLeft(16);about.setBounds(buttons.removeFromLeft(280));area.removeFromTop(20);details.setBounds(area);}
    void paint(juce::Graphics& g)override{g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(22.0f,juce::Font::bold));g.drawText("SETTINGS & ABOUT",24,10,getWidth()-48,32,juce::Justification::centredLeft);}
};
}
