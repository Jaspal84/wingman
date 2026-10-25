#pragma once
#include "../PluginProcessor.h"
#include "FileDragButton.h"
#include "Files/DragFileStore.h"
namespace wingman {
class GuitarStudioPanel final:public juce::Component,private juce::Timer {
    WingmanProcessor& processor;
    juce::ComboBox style,type,technique,length,variation,meter,rate;
    juce::Slider energy,density,swing,seed;
    juce::ToggleButton humanize{"Humanize"};
    juce::Label status,notice;
    juce::TextButton generate{"Generate MIDI"},regenerate{"Regenerate"},preview{"Preview"},render{"Render WAV"},exportMidi{"Export MIDI"},exportWav{"Export WAV"},cancel{"Cancel"};
    FileDragButton dragMidi,dragWav;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> notesArea;
    juce::String lastMessage;
    bool previewWhenReady=false;
    GuitarSettings config()const{auto s=processor.guitarConfig();s.style=static_cast<GuitarStyle>(style.getSelectedId()-1);s.type=static_cast<GuitarType>(type.getSelectedId()-1);s.technique=static_cast<GuitarTechnique>(technique.getSelectedId()-1);s.bars=length.getSelectedId();s.variation=variation.getSelectedId()-1;s.numerator=meter.getSelectedId()==2?3:meter.getSelectedId()==3?6:4;s.denominator=meter.getSelectedId()==3?8:4;s.energy=energy.getValue();s.density=density.getValue();s.swing=swing.getValue();s.seed=static_cast<std::uint32_t>(seed.getValue());s.humanize=humanize.getToggleState();return s;}
    bool start(bool audio){const auto r=processor.generateGuitarPart(config(),audio,rate.getSelectedId()==1?44100:48000);if(r.failed())status.setText(r.getErrorMessage(),juce::dontSendNotification);return r.wasOk();}
    void chooseExport(bool midi){
        chooser=std::make_unique<juce::FileChooser>(midi?"Export Guitar MIDI":"Export Guitar WAV",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(midi?"JASWINGMAN-Guitar.mid":"JASWINGMAN-Guitar.wav"),midi?"*.mid":"*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe=juce::Component::SafePointer<GuitarStudioPanel>(this),midi](const auto& fc){if(!safe||fc.getResult()==juce::File{})return;if(midi)safe->processor.selectPart(3);const auto r=midi?safe->processor.exportMidi(fc.getResult().withFileExtension("mid")):safe->processor.exportWav(fc.getResult().withFileExtension("wav"),4,safe->rate.getSelectedId()==1?44100:48000);safe->status.setText(r.wasOk()?"Guitar exported":r.getErrorMessage(),juce::dontSendNotification);});
    }
    void timerCallback()override{const auto job=processor.guitarStatus();if(job.running||job.message!=lastMessage){lastMessage=job.message;status.setText(job.message+(job.running?" | "+juce::String(job.progress*100,0)+"%":""),juce::dontSendNotification);}if(previewWhenReady&&!job.running){previewWhenReady=false;if(job.progress==1){processor.setPreview(false);processor.play();}}generate.setEnabled(!job.running);regenerate.setEnabled(!job.running);render.setEnabled(!job.running);cancel.setEnabled(job.running);repaint();}
public:
    explicit GuitarStudioPanel(WingmanProcessor& p):processor(p){
        style.addItemList({"Hollywood Cinematic","Punjabi Pop","Bollywood Romantic","Indie Acoustic","Folk","Rock","Jazz Fusion","Ambient"},1);
        type.addItemList({"Steel-string Acoustic","Nylon Classical","12-string Acoustic","Clean Electric","Distorted Electric"},1);
        technique.addItemList({"Strumming","Downstrokes","Upstrokes","Fingerpicking","Arpeggios","Palm-muted","Hybrid picking","Rhythmic accents"},1);
        for(int bars:{1,2,4,8,16,32})length.addItem(juce::String(bars)+" bars",bars);
        variation.addItemList({"Variation 1","Variation 2","Variation 3","Variation 4"},1);meter.addItemList({"4/4","3/4","6/8"},1);rate.addItemList({"44.1 kHz","48 kHz"},1);
        const auto s=p.guitarConfig();style.setSelectedId(static_cast<int>(s.style)+1);type.setSelectedId(static_cast<int>(s.type)+1);technique.setSelectedId(static_cast<int>(s.technique)+1);length.setSelectedId(s.bars);variation.setSelectedId(s.variation+1);meter.setSelectedId(s.denominator==8?3:s.numerator==3?2:1);rate.setSelectedId(2);
        energy.setName("Energy");density.setName("Density");swing.setName("Swing");seed.setName("Seed");
        for(auto* slider:{&energy,&density,&swing}){slider->setRange(0,1,0.01);slider->textFromValueFunction=[](double v){return juce::String(v*100,0)+"%";};slider->valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100;};slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24);}
        energy.setValue(s.energy);density.setValue(s.density);swing.setValue(s.swing);seed.setRange(1,1000000,1);seed.setValue(s.seed);seed.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,24);humanize.setToggleState(s.humanize,juce::dontSendNotification);
        for(auto* c:std::initializer_list<juce::Component*>{&style,&type,&technique,&length,&variation,&meter,&rate,&energy,&density,&swing,&seed,&humanize,&status,&notice,&generate,&regenerate,&preview,&render,&exportMidi,&exportWav,&dragMidi,&dragWav,&cancel})addAndMakeVisible(c);
        notice.setText("Follows project chords and key | Original plucked-string synthesis preview; all five timbres are basic synthesis, not sampled guitar realism.",juce::dontSendNotification);
        generate.onClick=[this]{start(false);};regenerate.onClick=[this]{seed.setValue(static_cast<int>(seed.getValue())%1000000+1);variation.setSelectedId(variation.getSelectedId()%4+1);start(false);};render.onClick=[this]{start(true);};cancel.onClick=[this]{processor.cancelGuitar();};
        preview.setTooltip("Render basic guitar audio, then play the session. Unmute/Solo Guitar to hear it alone.");
        preview.onClick=[this]{previewWhenReady=start(true);};
        exportMidi.onClick=[this]{chooseExport(true);};exportWav.onClick=[this]{chooseExport(false);};
        dragMidi.setButtonText("Drag MIDI");dragWav.setButtonText("Drag WAV");
        dragMidi.prepare=[this](juce::File& file){processor.selectPart(3);juce::File folder;auto r=DragFileStore::createDirectory(folder);if(r.failed())return r;file=folder.getChildFile("Guitar.mid");return processor.exportMidi(file);};
        dragWav.prepare=[this](juce::File& file){return processor.trackDragFile(4,file);};
        dragMidi.report=[this](const auto& text){status.setText(text,juce::dontSendNotification);};dragWav.report=dragMidi.report;
        timerCallback();startTimerHz(5);
    }
    void reloadSettings(){const auto s=processor.guitarConfig();style.setSelectedId(static_cast<int>(s.style)+1);type.setSelectedId(static_cast<int>(s.type)+1);technique.setSelectedId(static_cast<int>(s.technique)+1);length.setSelectedId(s.bars);variation.setSelectedId(s.variation+1);meter.setSelectedId(s.denominator==8?3:s.numerator==3?2:1);energy.setValue(s.energy);density.setValue(s.density);swing.setValue(s.swing);seed.setValue(s.seed);humanize.setToggleState(s.humanize,juce::dontSendNotification);}
    void resized()override{
        auto area=getLocalBounds().reduced(18);area.removeFromTop(40);const int w=(area.getWidth()-20)/3;
        for(auto* c:std::array<juce::Component*,3>{&type,&style,&technique}){c->setBounds(area.getX()+static_cast<int>(c==&type?0:c==&style?1:2)*(w+10),area.getY(),w,32);}area.removeFromTop(48);
        auto row=area.removeFromTop(32);for(auto* c:std::array<juce::Component*,4>{&length,&variation,&meter,&rate})c->setBounds(row.removeFromLeft(area.getWidth()/4).reduced(3,0));area.removeFromTop(26);
        for(auto* slider:{&energy,&density,&swing,&seed}){auto line=area.removeFromTop(36);line.removeFromLeft(100);slider->setBounds(line);}area.removeFromTop(5);humanize.setBounds(area.removeFromTop(28));notice.setBounds(area.removeFromTop(34));
        notesArea=area.removeFromTop(std::max(70,area.getHeight()-110));auto buttons=area.removeFromTop(38);for(auto* c:std::array<juce::Component*,9>{&generate,&regenerate,&preview,&render,&cancel,&exportMidi,&dragMidi,&exportWav,&dragWav})c->setBounds(buttons.removeFromLeft(area.getWidth()/9).reduced(2));status.setBounds(area.removeFromTop(45));
    }
    void paint(juce::Graphics& g)override{
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(22.0f,juce::Font::bold));g.drawText("GUITAR STUDIO",18,10,getWidth()-36,30,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.0f));
        for(auto* c:std::array<juce::Component*,7>{&type,&style,&technique,&length,&variation,&meter,&rate}){const juce::String label=c==&type?"Guitar type":c==&style?"Style rules":c==&technique?"Technique":c==&length?"Pattern length":c==&variation?"Variation":c==&meter?"Meter":"Render rate";g.drawText(label,c->getX(),c->getY()-17,c->getWidth(),16,juce::Justification::centredLeft);}
        g.setFont(juce::FontOptions(14.0f));for(auto* slider:{&energy,&density,&swing,&seed})g.drawText(slider->getName(),18,slider->getY(),100,slider->getHeight(),juce::Justification::centredLeft);
        g.setColour(background);g.fillRoundedRectangle(notesArea.toFloat(),5);const auto notes=processor.partNotes(3);const auto s=processor.guitarConfig();const double beats=s.bars*s.numerator*4.0/s.denominator;
        for(const auto& n:notes){g.setColour(trackColours[4]);g.fillRect(notesArea.getX()+static_cast<float>(n.beat/beats*notesArea.getWidth()),notesArea.getBottom()-8-static_cast<float>(n.note-40)/48*(notesArea.getHeight()-16),std::max(1.0f,static_cast<float>(n.duration/beats*notesArea.getWidth())),3.0f);}
    }
};
}
