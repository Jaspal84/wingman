#pragma once
#include "../PluginProcessor.h"
#include "FileDragButton.h"
#include "Files/DragFileStore.h"
namespace wingman {
class GuitarStudioPanel final:public juce::Component,private juce::Timer {
    WingmanProcessor& processor;
    juce::ToggleButton followSong{"Follow Imported Song"};
    juce::ComboBox songSource;
    juce::TextButton analyzeSong{"Analyze Song"},songTempo{"Use Detected Tempo"},songKey{"Use Detected Key"},songChords{"Use Detected Chords"},fromSong{"Generate from Song"},midiPreview{"Preview MIDI"};
    juce::TextButton loadSamples{"Load / Verify SFZ"};
    juce::ComboBox engine;
    juce::Slider velocityResponse;
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
    bool start(bool audio){const auto r=processor.generateGuitarPart(config(),audio,rate.getSelectedId()==1?44100:48000,followSong.getToggleState());if(r.failed())status.setText(r.getErrorMessage(),juce::dontSendNotification);return r.wasOk();}
    void chooseExport(bool midi){
        chooser=std::make_unique<juce::FileChooser>(midi?"Export Guitar MIDI":"Export Guitar WAV",juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile(midi?"JASWINGMAN-Guitar.mid":"JASWINGMAN-Guitar.wav"),midi?"*.mid":"*.wav");
        chooser->launchAsync(juce::FileBrowserComponent::saveMode|juce::FileBrowserComponent::canSelectFiles|juce::FileBrowserComponent::warnAboutOverwriting,[safe=juce::Component::SafePointer<GuitarStudioPanel>(this),midi](const auto& fc){if(!safe||fc.getResult()==juce::File{})return;if(midi)safe->processor.selectPart(3);const auto r=midi?safe->processor.exportMidi(fc.getResult().withFileExtension("mid")):safe->processor.exportWav(fc.getResult().withFileExtension("wav"),4,safe->rate.getSelectedId()==1?44100:48000);safe->status.setText(r.wasOk()?"Guitar exported":r.getErrorMessage(),juce::dontSendNotification);});
    }
    void timerCallback()override{notice.setText(processor.guitarEngineLabel()+" | SFZ articulation mapping follows library; type selector applies to synthesis only",juce::dontSendNotification);const auto job=processor.guitarStatus();if(job.running||job.message!=lastMessage){lastMessage=job.message;status.setText(job.message+(job.running?" | "+juce::String(job.progress*100,0)+"%":""),juce::dontSendNotification);}if(previewWhenReady&&!job.running){previewWhenReady=false;if(job.progress==1){processor.setPreview(false);processor.play();}}generate.setEnabled(!job.running);regenerate.setEnabled(!job.running);render.setEnabled(!job.running);cancel.setEnabled(job.running);repaint();}
public:
    explicit GuitarStudioPanel(WingmanProcessor& p):processor(p){
        style.addItemList({"Hollywood Cinematic","Punjabi Pop","Bollywood Romantic","Indie Acoustic","Folk","Rock","Jazz Fusion","Ambient"},1);
        type.addItemList({"Steel-string Acoustic","Nylon Classical","12-string Acoustic","Clean Electric","Distorted Electric"},1);
        technique.addItemList({"Strumming","Downstrokes","Upstrokes","Fingerpicking","Arpeggios","Palm-muted","Hybrid picking","Rhythmic accents"},1);
        for(int bars:{1,2,4,8,16,32,64,128})length.addItem(juce::String(bars)+" bars",bars);
        variation.addItemList({"Variation 1","Variation 2","Variation 3","Variation 4"},1);meter.addItemList({"4/4","3/4","6/8"},1);rate.addItemList({"44.1 kHz","48 kHz"},1);
        const auto s=p.guitarConfig();style.setSelectedId(static_cast<int>(s.style)+1);type.setSelectedId(static_cast<int>(s.type)+1);technique.setSelectedId(static_cast<int>(s.technique)+1);length.setSelectedId(s.bars);variation.setSelectedId(s.variation+1);meter.setSelectedId(s.denominator==8?3:s.numerator==3?2:1);rate.setSelectedId(2);
        energy.setName("Energy");density.setName("Density");swing.setName("Swing");seed.setName("Seed");
        for(auto* slider:{&energy,&density,&swing}){slider->setRange(0,1,0.01);slider->textFromValueFunction=[](double v){return juce::String(v*100,0)+"%";};slider->valueFromTextFunction=[](const juce::String& text){return text.getDoubleValue()/100;};slider->setTextBoxStyle(juce::Slider::TextBoxRight,false,65,24);}
        energy.setValue(s.energy);density.setValue(s.density);swing.setValue(s.swing);seed.setRange(1,1000000,1);seed.setValue(s.seed);seed.setTextBoxStyle(juce::Slider::TextBoxRight,false,90,24);humanize.setToggleState(s.humanize,juce::dontSendNotification);
        for(auto* c:std::initializer_list<juce::Component*>{&style,&type,&technique,&length,&variation,&meter,&rate,&energy,&density,&swing,&seed,&humanize,&status,&notice,&generate,&regenerate,&preview,&render,&exportMidi,&exportWav,&dragMidi,&dragWav,&cancel})addAndMakeVisible(c);
        engine.addItemList({"Basic Synthesis","Sampled (user SFZ)"},1);engine.setSelectedId(p.guitarEngineLabel().startsWith("Basic")?1:2);
        velocityResponse.setRange(.25,4,.05);velocityResponse.setValue(p.guitarSampleVelocity());velocityResponse.setName("Velocity response");velocityResponse.textFromValueFunction=[](double value){return "Velocity "+juce::String(value,2);};velocityResponse.valueFromTextFunction=[](const juce::String& text){return text.fromLastOccurrenceOf(" ",false,false).getDoubleValue();};velocityResponse.setTextBoxStyle(juce::Slider::TextBoxRight,false,120,24);velocityResponse.setTooltip("Sampled velocity response exponent; 1 preserves generated velocity");
        for(auto* c:std::initializer_list<juce::Component*>{&loadSamples,&engine,&velocityResponse})addAndMakeVisible(c);
        engine.onChange=[this]{processor.useSampledGuitar(engine.getSelectedId()==2,velocityResponse.getValue());};velocityResponse.onValueChange=engine.onChange;
        loadSamples.onClick=[this]{chooser=std::make_unique<juce::FileChooser>("Choose your licensed SFZ instrument",processor.guitarSampleFile(),"*.sfz");chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<GuitarStudioPanel>(this)](const auto& fc){if(!safe||fc.getResult()==juce::File{})return;auto r=safe->processor.loadGuitarLibrary(fc.getResult());if(r.failed())safe->status.setText(r.getErrorMessage(),juce::dontSendNotification);else safe->engine.setSelectedId(2);});};
        songSource.addItemList({"Vocals","Other","Bass","Drums","Guitar"},1);songSource.setSelectedId(1);
        for(auto* c:std::initializer_list<juce::Component*>{&followSong,&songSource,&analyzeSong,&songTempo,&songKey,&songChords,&fromSong,&midiPreview})addAndMakeVisible(c);
        auto report=[this](juce::Result r){if(r.failed())status.setText(r.getErrorMessage(),juce::dontSendNotification);};
        analyzeSong.onClick=[this,report]{report(processor.startTempoAnalysis(songSource.getSelectedId()-1));report(processor.startAnalysis(songSource.getSelectedId()-1));};
        songTempo.onClick=[this,report]{report(processor.applyDetectedTempo());};songKey.onClick=[this,report]{report(processor.applyDetectedKey());};songChords.onClick=[this,report]{report(processor.applyDetectedChords());};
        fromSong.onClick=[this]{followSong.setToggleState(true,juce::dontSendNotification);start(false);};
        midiPreview.onClick=[this]{if(processor.partNotes(3).empty()){status.setText("Generate Guitar MIDI first",juce::dontSendNotification);return;}processor.selectPart(3);processor.setPreview(true);processor.stop();processor.play();};
        preview.setButtonText("Preview Audio");
        generate.onClick=[this]{start(false);};regenerate.onClick=[this]{seed.setValue(static_cast<int>(seed.getValue())%1000000+1);variation.setSelectedId(variation.getSelectedId()%4+1);start(false);};render.onClick=[this]{start(true);};cancel.onClick=[this]{processor.cancelGuitar();};
        preview.setTooltip("Render selected guitar engine, then play the session. Unmute/Solo Guitar to hear it alone.");
        preview.onClick=[this]{previewWhenReady=start(true);};
        exportMidi.onClick=[this]{chooseExport(true);};exportWav.onClick=[this]{chooseExport(false);};
        dragMidi.setButtonText("Drag MIDI");dragWav.setButtonText("Drag WAV");
        dragMidi.prepare=[this](juce::File& file){processor.selectPart(3);juce::File folder;auto r=DragFileStore::createDirectory(folder);if(r.failed())return r;file=folder.getChildFile("Guitar.mid");return processor.exportMidi(file);};
        dragWav.prepare=[this](juce::File& file){return processor.trackDragFile(4,file);};
        dragMidi.report=[this](const auto& text){status.setText(text,juce::dontSendNotification);};dragWav.report=dragMidi.report;
        timerCallback();startTimerHz(5);
    }
    void mouseDown(const juce::MouseEvent& event)override{
        if(!followSong.getToggleState()||event.y<notesArea.getY()||event.y>notesArea.getY()+22||!notesArea.contains(event.getPosition()))return;
        const auto song=processor.analysisSnapshot();if(song.regions.empty())return;double at=(event.x-notesArea.getX())*song.regions.back().end/notesArea.getWidth();
        for(std::size_t i=0;i<song.regions.size();++i)if(at>=song.regions[i].start&&at<song.regions[i].end){auto* dialog=new juce::AlertWindow("Correct detected chord","The region timing stays unchanged",juce::AlertWindow::NoIcon);dialog->addTextEditor("chord",juce::String(chordName(song.regions[i].chord)));dialog->addButton("Apply",1);dialog->addButton("Cancel",0);dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<GuitarStudioPanel>(this),dialog,i](int result){if(safe&&result){auto r=safe->processor.correctDetectedChord(static_cast<int>(i),dialog->getTextEditorContents("chord"));safe->status.setText(r.wasOk()?"Detected chord corrected":r.getErrorMessage(),juce::dontSendNotification);}}),true);break;}
    }
    void reloadSettings(){const auto s=processor.guitarConfig();style.setSelectedId(static_cast<int>(s.style)+1);type.setSelectedId(static_cast<int>(s.type)+1);technique.setSelectedId(static_cast<int>(s.technique)+1);length.setSelectedId(s.bars);variation.setSelectedId(s.variation+1);meter.setSelectedId(s.denominator==8?3:s.numerator==3?2:1);energy.setValue(s.energy);density.setValue(s.density);swing.setValue(s.swing);seed.setValue(s.seed);humanize.setToggleState(s.humanize,juce::dontSendNotification);}
    void resized()override{
        auto area=getLocalBounds().reduced(18);area.removeFromTop(40);const int w=(area.getWidth()-20)/3;
        for(auto* c:std::array<juce::Component*,3>{&type,&style,&technique}){c->setBounds(area.getX()+static_cast<int>(c==&type?0:c==&style?1:2)*(w+10),area.getY(),w,32);}area.removeFromTop(48);
        auto row=area.removeFromTop(32);for(auto* c:std::array<juce::Component*,4>{&length,&variation,&meter,&rate})c->setBounds(row.removeFromLeft(area.getWidth()/4).reduced(3,0));area.removeFromTop(26);
        for(auto* slider:{&energy,&density,&swing,&seed}){auto line=area.removeFromTop(36);line.removeFromLeft(100);slider->setBounds(line);}auto libraryRow=area.removeFromTop(32);humanize.setBounds(libraryRow.removeFromLeft(130));loadSamples.setBounds(libraryRow.removeFromLeft(180));engine.setBounds(libraryRow.removeFromLeft(210));velocityResponse.setBounds(libraryRow);auto songRow=area.removeFromTop(34);for(auto* c:std::array<juce::Component*,7>{&followSong,&songSource,&analyzeSong,&songTempo,&songKey,&songChords,&fromSong})c->setBounds(songRow.removeFromLeft(area.getWidth()/7).reduced(2));notice.setBounds(area.removeFromTop(34));
        notesArea=area.removeFromTop(std::max(70,area.getHeight()-110));auto buttons=area.removeFromTop(38);for(auto* c:std::array<juce::Component*,10>{&generate,&regenerate,&midiPreview,&preview,&render,&cancel,&exportMidi,&dragMidi,&exportWav,&dragWav})c->setBounds(buttons.removeFromLeft(area.getWidth()/10).reduced(2));status.setBounds(area.removeFromTop(45));
    }
    void paint(juce::Graphics& g)override{
        g.setColour(panel);g.fillRoundedRectangle(getLocalBounds().toFloat(),12);g.setColour(juce::Colours::white);g.setFont(juce::FontOptions(22.0f,juce::Font::bold));g.drawText("GUITAR STUDIO",18,10,getWidth()-36,30,juce::Justification::centredLeft);
        g.setFont(juce::FontOptions(11.0f));
        for(auto* c:std::array<juce::Component*,7>{&type,&style,&technique,&length,&variation,&meter,&rate}){const juce::String label=c==&type?"Guitar type":c==&style?"Style rules":c==&technique?"Technique":c==&length?"Pattern length":c==&variation?"Variation":c==&meter?"Meter":"Render rate";g.drawText(label,c->getX(),c->getY()-17,c->getWidth(),16,juce::Justification::centredLeft);}
        g.setFont(juce::FontOptions(14.0f));for(auto* slider:{&energy,&density,&swing,&seed})g.drawText(slider->getName(),18,slider->getY(),100,slider->getHeight(),juce::Justification::centredLeft);
        g.setColour(background);g.fillRoundedRectangle(notesArea.toFloat(),5);if(followSong.getToggleState()){const auto song=processor.analysisSnapshot();if(!song.regions.empty()){const double total=song.regions.back().end;for(const auto& region:song.regions){auto r=juce::Rectangle<float>(notesArea.getX()+static_cast<float>(region.start/total*notesArea.getWidth()),static_cast<float>(notesArea.getY()),static_cast<float>((region.end-region.start)/total*notesArea.getWidth()),22);g.setColour(accent.withAlpha(.35f));g.fillRect(r);g.setColour(juce::Colours::white);g.drawText(juce::String(chordName(region.chord)),r,juce::Justification::centred);}}}const auto notes=processor.partNotes(3);const auto s=processor.guitarConfig();const double beats=s.bars*s.numerator*4.0/s.denominator;
        for(const auto& n:notes){g.setColour(trackColours[4]);g.fillRect(notesArea.getX()+static_cast<float>(n.beat/beats*notesArea.getWidth()),notesArea.getBottom()-8-static_cast<float>(n.note-40)/48*(notesArea.getHeight()-16),std::max(1.0f,static_cast<float>(n.duration/beats*notesArea.getWidth())),3.0f);}
    }
};
}
