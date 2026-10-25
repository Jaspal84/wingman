#pragma once
#include "../PluginProcessor.h"
#include "Music/CinematicIdeas.h"
#include "FileDragButton.h"
#include "Files/DragFileStore.h"
#include "Theme.h"
namespace wingman {
class CinematicPanel final:public juce::Component {
    WingmanProcessor& processor;
    juce::ComboBox style,bars,complexity,source;
    juce::Slider seed;
    juce::ToggleButton voiceLeading{"Smooth voice leading"};
    juce::TextEditor chords;
    juce::TextButton generate{"Generate idea"},regenerate{"Regenerate"},apply{"Apply to Project"},detect{"Detect from Audio"},exportMidi{"Export MIDI"};
    FileDragButton drag;
    juce::Label status;
    juce::Rectangle<int> chordArea;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Result applyIdea(){auto r=processor.setProgression(chords.getText());if(r.wasOk()){processor.setVoiceLeading(voiceLeading.getToggleState());processor.selectPart(-1);processor.generateMidi();}status.setText(r.wasOk()?"Progression applied; chord MIDI ready":r.getErrorMessage(),juce::dontSendNotification);repaint();return r;}
    void generateIdea(){try{const auto idea=cinematicIdeas(style.getSelectedId()-1,bars.getSelectedId(),static_cast<std::uint32_t>(seed.getValue()),complexity.getSelectedId()==2);juce::StringArray tokens;for(const auto& chord:idea)tokens.add(chordName(chord));chords.setText(tokens.joinIntoString(" "));status.setText("Editable idea generated using style rules. Apply to Project when ready.",juce::dontSendNotification);repaint();}catch(const std::exception& ex){status.setText(ex.what(),juce::dontSendNotification);}}
public:
    explicit CinematicPanel(WingmanProcessor& p):processor(p){
        style.addItemList({"Hollywood Emotional","Hollywood Suspense","Epic Cinematic","Romantic Film Score","Punjabi Cinematic","Bollywood","Jazz Fusion","Ambient"},1);style.setSelectedId(1);
        for(int n:{4,8,16,32})bars.addItem(juce::String(n)+" bars",n);bars.setSelectedId(8);complexity.addItemList({"Simple","Advanced"},1);complexity.setSelectedId(2);source.addItemList({"Vocals","Other","Bass","Drums","Guitar"},1);source.setSelectedId(2);
        seed.setName("Seed");seed.setRange(1,1000000,1);seed.setValue(42);seed.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,28);voiceLeading.setToggleState(p.voiceLeadingEnabled(),juce::dontSendNotification);
        chords.setMultiLine(true);chords.setText(p.progression());chords.setTooltip("Examples: Am9 Fmaj7 Cadd9 Gsus4 | Dm(add9) Bbmaj7 Fmaj9 C/E");chords.onTextChange=[this]{repaint();};
        for(auto* c:std::initializer_list<juce::Component*>{&style,&bars,&complexity,&source,&seed,&voiceLeading,&chords,&generate,&regenerate,&apply,&detect,&exportMidi,&drag,&status})addAndMakeVisible(c);
        generate.onClick=[this]{generateIdea();};regenerate.onClick=[this]{seed.setValue(static_cast<int>(seed.getValue())%1000000+1);generateIdea();};apply.onClick=[this]{applyIdea();};
        detect.onClick=[this]{auto r=processor.startAnalysis(source.getSelectedId()-1);status.setText(r.wasOk()?"Background detection started. Detected project chords appear in Stems & Tracks.":r.getErrorMessage(),juce::dontSendNotification);};
        exportMidi.onClick=[this]{if(applyIdea().failed())return;chooser=std::make_unique<juce::FileChooser>("Export chord MIDI",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("JASWINGMAN-Chords.mid"),"*.mid");chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe=juce::Component::SafePointer<CinematicPanel>(this)](const auto& fc){if(!safe||fc.getResult()==juce::File{})return;safe->processor.selectPart(-1);auto r=safe->processor.exportMidi(fc.getResult().withFileExtension("mid"));safe->status.setText(r.wasOk()?"Chord MIDI exported":r.getErrorMessage(),juce::dontSendNotification);});};
        drag.setButtonText("Drag MIDI");drag.prepare=[this](juce::File& file){auto r=applyIdea();if(r.failed())return r;juce::File dir;r=DragFileStore::createDirectory(dir);if(r.failed())return r;file=dir.getChildFile("Cinematic-Chords.mid");return processor.exportMidi(file);};drag.report=[this](const auto& text){status.setText(text,juce::dontSendNotification);};
        status.setText("Style rules, extensions and voice leading. No trained chord-generation model is installed.",juce::dontSendNotification);
    }
    void resized()override{auto area=getLocalBounds().reduced(24);area.removeFromTop(50);auto row=area.removeFromTop(34);style.setBounds(row.removeFromLeft(area.getWidth()/3).reduced(3));bars.setBounds(row.removeFromLeft(area.getWidth()/5).reduced(3));complexity.setBounds(row.removeFromLeft(area.getWidth()/5).reduced(3));voiceLeading.setBounds(row);area.removeFromTop(15);seed.setBounds(area.removeFromTop(32).withTrimmedLeft(70));area.removeFromTop(15);chords.setBounds(area.removeFromTop(70));area.removeFromTop(18);chordArea=area.removeFromTop(std::max(100,area.getHeight()-125));auto buttons=area.removeFromTop(38);for(auto* c:std::array<juce::Component*,7>{&generate,&regenerate,&apply,&source,&detect,&exportMidi,&drag})c->setBounds(buttons.removeFromLeft(area.getWidth()/7).reduced(2));status.setBounds(area.removeFromTop(60));}
    void paint(juce::Graphics& g)override{g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(22.0f,juce::Font::bold));g.drawText("CHORDS & CINEMATIC IDEAS",24,12,getWidth()-48,30,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(14.0f));g.drawText("Seed",24,seed.getY(),65,seed.getHeight(),juce::Justification::centredLeft);
        auto tokens=juce::StringArray::fromTokens(chords.getText()," ,|\n\t","");tokens.removeEmptyStrings();const int columns=std::min(4,std::max(1,tokens.size())),rows=(tokens.size()+columns-1)/columns;
        for(int i=0;i<tokens.size();++i){const auto rect=juce::Rectangle<int>(chordArea.getX()+i%columns*chordArea.getWidth()/columns,chordArea.getY()+i/columns*chordArea.getHeight()/std::max(1,rows),chordArea.getWidth()/columns,chordArea.getHeight()/std::max(1,rows)).reduced(3);g.setColour(trackColours[static_cast<std::size_t>(i%5)].withAlpha(0.2f));g.fillRoundedRectangle(rect.toFloat(),6);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(17.0f));g.drawText(tokens[i],rect,juce::Justification::centred);}
    }
};
}
