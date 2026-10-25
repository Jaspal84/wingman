#include "PluginEditor.h"
#include "GUI/GeneratorPanel.h"
#include "GUI/ModelPanel.h"
#include "GUI/GuitarStudioPanel.h"
#include "GUI/CinematicPanel.h"
#include "GUI/TempoPanel.h"
#include "GUI/SettingsPanel.h"
#include "Files/DragFileStore.h"
namespace {
const std::array<juce::String,5> names{"Vocals", "Other", "Bass", "Drums", "Guitar"};
const juce::StringArray keyNames{"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
}
WingmanEditor::WingmanEditor(WingmanProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&theme);
    addAndMakeVisible(projectTitle);projectTitle.setText(processor.sessionName());projectTitle.setTooltip("Project/session name saved in plugin state");
    projectTitle.setFont(juce::FontOptions(11.0f));projectTitle.setColour(juce::TextEditor::backgroundColourId,juce::Colours::transparentBlack);
    projectTitle.onFocusLost=[this]{processor.setSessionName(projectTitle.getText());};projectTitle.onReturnKey=projectTitle.onFocusLost;
    const auto size = processor.editorSize();
    setResizable(true,false); setResizeLimits(960,660,1920,1318);
    setSize(size.first,size.second);
    addAndMakeVisible(audioLoop);audioLoop.onClick=[this]{processor.setLooping(audioLoop.getToggleState());};audioLoop.setTooltip("Loop imported audio to the longest lane; waveform click seeks while DAW sync is off");
    addAndMakeVisible(sync); sync.setTooltip("Follow the DAW playhead; local Play/BPM controls are disabled while synchronized"); sync.onClick=[this]{processor.setHostSync(sync.getToggleState());};
    addAndMakeVisible(midiBar); midiBar.setRange(1,32,1); midiBar.setValue(1); midiBar.setTooltip("Drum step-sequencer bar (1-based)"); midiBar.onValueChange=[this]{repaint();};
    addAndMakeVisible(partChoice); addAndMakeVisible(dragMidi);
    partChoice.addItemList({"Chord MIDI","Bass MIDI","Drum MIDI","Melody MIDI","Guitar MIDI"},1);
    partChoice.setSelectedId(processor.selectedPart()+2,juce::dontSendNotification);
    partChoice.onChange = [this] { processor.selectPart(partChoice.getSelectedId()-2); repaint(); };
    dragMidi.setTooltip("Drag the selected MIDI part into a compatible DAW");
    dragMidi.prepare = [this](juce::File& file) {
        juce::File dir;
        const auto allocated=wingman::DragFileStore::createDirectory(dir);
        if(allocated.failed())return allocated;
        file=dir.getChildFile(partChoice.getText().replaceCharacter(' ','-')+".mid");
        return processor.exportMidi(file);
    };
    dragMidi.report=[this](const auto& message){status.setText(message,juce::dontSendNotification);};
    for(auto* c:std::initializer_list<juce::Component*>{&splitChoose,&splitStart,&splitCancel,&splitStatus,&splitProgressBar})addAndMakeVisible(c);
    splitChoose.onClick=[this]{
        chooser=std::make_unique<juce::FileChooser>("Choose audio to split (up to 120 seconds)",juce::File{},"*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a");
        chooser->launchAsync(juce::FileBrowserComponent::openMode|juce::FileBrowserComponent::canSelectFiles,[safe=juce::Component::SafePointer<WingmanEditor>(this)](const auto& fc){
            if(safe&&fc.getResult()!=juce::File{}){safe->splitInput=fc.getResult();safe->splitChoose.setTooltip(safe->splitInput.getFullPathName());safe->splitStatus.setText("Ready: "+safe->splitInput.getFileName()+" | Separate replaces four stems; Guitar is preserved",juce::dontSendNotification);}
        });
    };
    splitStart.onClick=[this]{startSplit(splitInput);};
    splitCancel.onClick=[this]{processor.cancelModelJob();};
    splitStatus.setText("STEM SPLITTER | Select audio. Configure runtime in Model Manager. Replaces four stems; Guitar is independent.",juce::dontSendNotification);
    addAndMakeVisible(models); addAndMakeVisible(detect); addAndMakeVisible(cancel);
    detect.setTooltip("Analyse the selected input lane in a background worker; replaces the chord progression");
    detect.onClick = [this] { showResult(processor.startAnalysis(recordTarget.getSelectedId()-1),"Harmonic analysis started"); };
    cancel.onClick = [this] { processor.cancelAnalysis(); };
    models.onClick = [this] {
        auto safe=juce::Component::SafePointer<WingmanEditor>(this);
        juce::DialogWindow::LaunchOptions options;options.dialogTitle="HTDemucs Model Manager";
        options.content.setOwned(new wingman::ModelPanel(processor.modelConfig(),
            [safe](const auto& python,const auto& models){if(safe)safe->processor.configureModel(python,models);},
            [safe](const juce::String& action,const juce::File& input,bool consent){return safe?safe->processor.startModelJob(action,input,consent):juce::Result::fail("Editor closed");},
            [safe]{if(safe)safe->processor.cancelModelJob();},
            [safe]{return safe?safe->processor.modelJobStatus():wingman::ModelJobInfo{false,0,"Editor closed"};}));
        options.dialogBackgroundColour=wingman::background;options.useNativeTitleBar=true;options.resizable=true;options.launchAsync();
    };
    for (auto* component : std::initializer_list<juce::Component*>{&record,&playButton,&stopButton,&keyBox,&recordTarget,&bpm,&status,&chordInput,&apply,&generate,&exportMix,&exportMidi,&preview,&ideas}) addAndMakeVisible(component);
    addAndMakeVisible(timelineZoom); addAndMakeVisible(timelineScroll);
    timelineZoom.setRange(1,8,0.25); timelineZoom.setValue(1); timelineZoom.setTooltip("Chord timeline zoom");
    timelineScroll.setRange(0,1,0.001); timelineScroll.setTooltip("Chord timeline horizontal scroll");
    for (auto* control : {&timelineZoom,&timelineScroll}) {
        control->setSliderStyle(juce::Slider::LinearHorizontal); control->setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        control->onValueChange = [this] { repaint(); };
    }
    keyBox.addItemList(keyNames,1);
    keyBox.setTooltip("Transpose the written chord progression by this many semitones from C");
    keyBox.onChange = [this] { processor.setKey(keyBox.getSelectedId()-1); };
    recordTarget.addItemList({"Vocals", "Other", "Bass", "Drums", "Guitar"},1); recordTarget.setSelectedId(1);
    bpm.setRange(20,400,0.1); bpm.setSliderStyle(juce::Slider::IncDecButtons);
    bpm.setTextBoxStyle(juce::Slider::TextBoxLeft,false,65,28);
    bpm.onValueChange = [this] { processor.setTempo(bpm.getValue()); };
    record.setColour(juce::TextButton::buttonOnColourId,juce::Colour{0xffa43848});
    playButton.setColour(juce::TextButton::buttonColourId,juce::Colour{0xff0b766a});
    playButton.setColour(juce::TextButton::buttonOnColourId,juce::Colour{0xff237d60});
    record.onClick = [this] {
        if (processor.isRecording()) showResult(processor.finishRecording(),"Recording captured (maximum 120 seconds)");
        else showResult(processor.beginRecording(recordTarget.getSelectedId()-1),"Recording hardware/plugin input; click Record or Stop to finish");
        refresh();
    };
    playButton.onClick = [this] { processor.play(); };
    stopButton.onClick = [this] {
        if (processor.isRecording()) showResult(processor.finishRecording(),"Recording captured");
        processor.stop(); refresh();
    };
    for (int i = 0; i < 5; ++i) {
        const auto n = static_cast<std::size_t>(i);
        for (auto* c : std::initializer_list<juce::Component*>{&imports[n],&solos[n],&mutes[n],&volumes[n],&pans[n],&stemExports[n]}) addAndMakeVisible(c);
        imports[n].setButtonText("Import Audio"); imports[n].onClick = [this,i] { importTrack(i); };
        solos[n].setButtonText("S"); mutes[n].setButtonText("M");
        solos[n].setClickingTogglesState(true); mutes[n].setClickingTogglesState(true);
        solos[n].setTooltip("Solo this track (multiple solos allowed)"); mutes[n].setTooltip("Mute this track");
        solos[n].onClick = [this,i] { updateMix(i); }; mutes[n].onClick = [this,i] { updateMix(i); };
        volumes[n].setRange(0,1,0.001); volumes[n].setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        volumes[n].setTextBoxStyle(juce::Slider::NoTextBox,false,0,0);
        volumes[n].setTooltip("Track volume, 0-100%"); volumes[n].onValueChange = [this,i] { updateMix(i); };
        pans[n].setRange(-1,1,0.01); pans[n].setSliderStyle(juce::Slider::LinearHorizontal);
        pans[n].setTextBoxStyle(juce::Slider::NoTextBox,false,0,0); pans[n].setTooltip("Stereo balance: left / centre / right");
        pans[n].setName(names[n]+" stereo balance"); pans[n].setDoubleClickReturnValue(true,0);
        pans[n].onValueChange = [this,i,n] { processor.setTrackPan(i,static_cast<float>(pans[n].getValue())); };
        addAndMakeVisible(stemDrags[n]);
        stemDrags[n].setTooltip("Drag audio WAV to DAW (before lane volume/pan). Export WAV includes mixer settings.");
        stemDrags[n].prepare=[this,n](juce::File& file){
            return processor.trackDragFile(static_cast<int>(n),file);
        };
        stemDrags[n].report=dragMidi.report;
        stemExports[n].setButtonText("Export " + names[n]); stemExports[n].onClick = [this,i] { chooseExport(false,i); };
    }
    chordInput.setMultiLine(false); chordInput.setText(processor.progression());
    chordInput.setTooltip("1-128 chords: C Am G7 Cmaj7 Dm7 Csus2 Csus4 Bdim. Cmaj7^1:2 sets inversion 1, duration 2 beats.");
    auto applyChords = [this] { showResult(processor.setProgression(chordInput.getText()),"Progression updated; one bar per chord"); repaint(); };
    apply.onClick = applyChords; chordInput.onReturnKey = applyChords;
    generate.onClick = [this] {
        const int part = partChoice.getSelectedId()-2;
        if(part==3){showResult(processor.generateGuitarPart(processor.guitarConfig()),"Generating guitar MIDI");return;}
        if (part >= 0) {
            auto initial = processor.partSettings(part);
            const auto harmony = processor.analysisSnapshot();
            if (!harmony.regions.empty()) { initial.keyRoot = harmony.keyRoot; initial.minor = harmony.minor; }
            juce::DialogWindow::LaunchOptions options;
            options.dialogTitle = part == 0 ? "Bass generator" : part == 1 ? "Drum generator" : "Melody generator";
            options.content.setOwned(new wingman::GeneratorPanel(initial,[safe = juce::Component::SafePointer<WingmanEditor>(this),part](const auto& settings) {
                if (!safe) return juce::Result::fail("Editor closed");
                auto result = safe->processor.generatePart(part,settings); safe->repaint(); return result;
            },[safe = juce::Component::SafePointer<WingmanEditor>(this),part](int first,int length){
                return safe ? safe->processor.varyPart(part,first,length) : juce::Result::fail("Editor closed");
            }));
            options.dialogBackgroundColour = wingman::background; options.useNativeTitleBar = true;
            options.resizable = true; options.launchAsync(); return;
        }
        auto result = processor.setProgression(chordInput.getText());
        if (result.wasOk()) processor.generateMidi();
        showResult(result,"MIDI generated. Play to audition; export for your DAW."); repaint();
    };
    preview.onClick = [this] { processor.setPreview(preview.getToggleState()); };
    exportMix.onClick = [this] { chooseExport(false); };
    exportMidi.onClick = [this] { chooseExport(true); };
    ideas.addItem("Choose a progression",1); ideas.addItem("Warm | C Am F G",2);
    ideas.addItem("Reflective | Am F C G",3); ideas.addItem("Bright | C F G C",4);
    ideas.setSelectedId(1);
    ideas.onChange = [this] {
        const juce::StringArray presets{"C Am F G", "Am F C G", "C F G C"};
        const int i = ideas.getSelectedId()-2;
        if (i >= 0 && i < presets.size()) {
            chordInput.setText(presets[i]); showResult(processor.setProgression(presets[i]),"Musical idea applied");
        }
    };
    status.setColour(juce::Label::textColourId,juce::Colour{0xffc4cadb});
    status.setText("Import WAVs or record input. Edit a progression, then Generate MIDI.",juce::dontSendNotification);
    const juce::StringArray tabs{"Stems & Tracks","Guitar Studio","Chords & Ideas","Tempo & Analysis","Settings"};
    for(int i=0;i<5;++i){addAndMakeVisible(viewTabs[static_cast<std::size_t>(i)]);viewTabs[static_cast<std::size_t>(i)].setButtonText(tabs[i]);viewTabs[static_cast<std::size_t>(i)].onClick=[this,i]{selectView(i);};}
    guitarPanel=std::make_unique<wingman::GuitarStudioPanel>(processor);cinematicPanel=std::make_unique<wingman::CinematicPanel>(processor);tempoPanel=std::make_unique<wingman::TempoPanel>(processor);
    settingsPanel=std::make_unique<wingman::SettingsPanel>(processor,[safe=juce::Component::SafePointer<WingmanEditor>(this)]{if(safe&&safe->models.onClick)safe->models.onClick();});
    for(auto* c:std::initializer_list<juce::Component*>{guitarPanel.get(),cinematicPanel.get(),tempoPanel.get(),settingsPanel.get()})addAndMakeVisible(c);
    resized();
    for(auto* child:getChildren())if(child->getY()>=124 && child!=guitarPanel.get()&&child!=cinematicPanel.get()&&child!=tempoPanel.get()&&child!=settingsPanel.get())mainContent.push_back(child);
    selectView(0);refresh(); startTimerHz(20);
}
WingmanEditor::~WingmanEditor() { stopTimer(); setLookAndFeel(nullptr); }
void WingmanEditor::updateMix(int i) {
    const auto n = static_cast<std::size_t>(i);
    processor.setTrackMix(i,static_cast<float>(volumes[n].getValue()),mutes[n].getToggleState(),solos[n].getToggleState());
}
void WingmanEditor::showResult(const juce::Result& result, const juce::String& success) {
    status.setText(result.wasOk() ? success : result.getErrorMessage(),juce::dontSendNotification);
}
void WingmanEditor::importTrack(int i) {
    chooser = std::make_unique<juce::FileChooser>("Import audio into " + names[static_cast<std::size_t>(i)],juce::File{},"*.wav;*.aif;*.aiff;*.flac;*.mp3;*.m4a");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<WingmanEditor>(this),i](const juce::FileChooser& fc) {
            if (!safe || fc.getResult() == juce::File{}) return;
            auto result = safe->processor.importWav(i,fc.getResult());
            safe->showResult(result,"Imported " + fc.getResult().getFileName());
            if (result.wasOk()) safe->status.setText("Audio loaded; tempo, key and chord analysis started",juce::dontSendNotification);
            safe->refresh();
        });
}
void WingmanEditor::chooseExport(bool midi, int track) {
    const auto extension = midi ? ".mid" : ".wav";
    chooser = std::make_unique<juce::FileChooser>(midi ? "Export standard MIDI file" : "Export WAV audio",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("JASWINGMAN" + juce::String(extension)), "*" + juce::String(extension));
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<WingmanEditor>(this),midi,track,extension](const juce::FileChooser& fc) {
            if (!safe || fc.getResult() == juce::File{}) return;
            const auto file = fc.getResult().withFileExtension(extension);
            safe->showResult(midi ? safe->processor.exportMidi(file) : safe->processor.exportWav(file,track),"Exported " + file.getFileName());
        });
}
void WingmanEditor::refresh() {
    if(!projectTitle.hasKeyboardFocus(true)&&projectTitle.getText()!=processor.sessionName())projectTitle.setText(processor.sessionName());
    audioLoop.setToggleState(processor.looping(),juce::dontSendNotification);audioLoop.setEnabled(!processor.hostSync());
    cancel.setEnabled(processor.analysisIsRunning());
    partChoice.setSelectedId(processor.selectedPart()+2,juce::dontSendNotification);
    midiBar.setEnabled(processor.selectedPart()==1);
    sync.setToggleState(processor.hostSync(),juce::dontSendNotification);
    bpm.setEnabled(!processor.hostSync()); playButton.setEnabled(!processor.hostSync()); stopButton.setEnabled(!processor.hostSync());
    keyBox.setSelectedId(processor.key()+1,juce::dontSendNotification);
    bpm.setValue(processor.tempo(),juce::dontSendNotification);
    preview.setToggleState(processor.preview(),juce::dontSendNotification);
    record.setButtonText(processor.isRecording() ? "Finish recording" : "Record audio");
    record.setToggleState(processor.isRecording(),juce::dontSendNotification);
    playButton.setToggleState(processor.isPlaying(),juce::dontSendNotification);
    bool changed = false;
    for (int i = 0; i < 5; ++i) {
        const auto n = static_cast<std::size_t>(i); const auto t = processor.trackSnapshot(i);
        pans[n].setValue(t.pan,juce::dontSendNotification);
        volumes[n].setValue(t.gain,juce::dontSendNotification); mutes[n].setToggleState(t.mute,juce::dontSendNotification);
        solos[n].setToggleState(t.solo,juce::dontSendNotification);
        if (cachedClips[n] != t.clip) { cachedClips[n] = t.clip; changed = true; }
    }
    if (changed) for (int i = 0; i < 5; ++i) rebuildWaveform(i);
    repaint();
}
void WingmanEditor::timerCallback() {
    const auto job=processor.modelJobStatus();
    splitProgress=juce::jlimit(0.0,1.0,job.progress);
    splitProgressBar.setTextToDisplay(job.running?juce::String(splitProgress*100,0)+"%":"Stem Splitter");
    splitStart.setEnabled(!job.running && splitInput.existsAsFile()); splitChoose.setEnabled(!job.running);splitCancel.setEnabled(job.running);
    if(job.running || job.message!=splitLastMessage){
        splitLastMessage=job.message;
        if(job.message!="No model operation has run")splitStatus.setText(job.message+(job.running?" | "+juce::String(job.progress*100,0)+"%":""),juce::dontSendNotification);
    }
    for(int lane=0;lane<5;++lane){processor.prepareTrackDrag(lane);stemDrags[static_cast<std::size_t>(lane)].setEnabled(processor.trackSnapshot(lane).clip!=nullptr);}
    const bool running = processor.analysisIsRunning();
    cancel.setEnabled(running); detect.setEnabled(!running);
    if (running) status.setText("Analysing harmony: "+juce::String(processor.analysisPercent()*100,0)+"%",juce::dontSendNotification);
    else if (analysisWasRunning) status.setText(processor.analysisMessage(),juce::dontSendNotification);
    analysisWasRunning = running;
    if(processor.hostSync()&&!running){
        if(auto host=processor.hostPosition()) {
            const double quartersPerBar=host->numerator*4.0/host->denominator;
            const auto bar=static_cast<juce::int64>(std::floor(host->ppq/quartersPerBar));
            const int beat=static_cast<int>(std::floor((host->ppq-bar*quartersPerBar)*host->denominator/4))+1;
            status.setText("DAW "+juce::String(host->playing?"playing":"stopped")+" | "+juce::String(host->numerator)+"/"+juce::String(host->denominator)+" | bar "+juce::String(bar+1)+" beat "+juce::String(beat),juce::dontSendNotification);
        } else status.setText("DAW sync: waiting for a valid host PPQ position",juce::dontSendNotification);
    }
    // Host state can change while the editor remains open; preserve active text edits.
    if (!chordInput.hasKeyboardFocus(true) && chordInput.getText() != processor.progression()) chordInput.setText(processor.progression());
    refresh();
}
void WingmanEditor::rebuildWaveform(int i) {
    const auto n = static_cast<std::size_t>(i); auto& path = waveformPaths[n]; path.clear();
    if (!cachedClips[n]) return;
    const auto& clip = *cachedClips[n]; const auto bounds = waveformBounds[n].toFloat();
    const int width = static_cast<int>(bounds.getWidth());
    if (width <= 0) return;
    double duration = 1;
    for (const auto& item : cachedClips) if (item) duration = std::max(duration,item->audio.getNumSamples()/item->sampleRate);
    const auto* samples = clip.audio.getReadPointer(0); const int total = clip.audio.getNumSamples();
    for (int x = 0; x < width; ++x) {
        int begin = static_cast<int>(static_cast<double>(x)*duration*clip.sampleRate/width);
        if (begin >= total) break;
        int end = std::max(begin+1,static_cast<int>(static_cast<double>(x+1)*duration*clip.sampleRate/width));
        end = std::min(total,end); float peak = 0;
        if(!clip.peaks.empty()){
            for(int bin=begin/wingman::Clip::peakHop;bin<(end+wingman::Clip::peakHop-1)/wingman::Clip::peakHop&&bin<static_cast<int>(clip.peaks.size());++bin)peak=std::max(peak,clip.peaks[static_cast<std::size_t>(bin)]);
        }else{
            const int stride=std::max(1,(end-begin)/64);for(int sample=begin;sample<end;sample+=stride)peak=std::max(peak,std::abs(samples[sample]));
        }
        const float height = std::min(1.0f,peak)*bounds.getHeight()*0.42f;
        path.startNewSubPath(bounds.getX()+x,bounds.getCentreY()-height);
        path.lineTo(bounds.getX()+x,bounds.getCentreY()+std::max(0.7f,height));
    }
}
void WingmanEditor::resized() {
    audioLoop.setBounds(990,584,75,24);
    uiScale=std::min(getWidth()/1200.0f,getHeight()/840.0f);
    processor.setEditorSize(getWidth(),getHeight());
    projectTitle.setBounds(28,55,174,18);models.setBounds(942,24,222,34);sync.setBounds(870,24,68,34);
    record.setBounds(206,24,142,34);recordTarget.setBounds(354,24,118,34);keyBox.setBounds(493,27,72,30);bpm.setBounds(581,27,120,30);playButton.setBounds(719,24,68,34);stopButton.setBounds(795,24,68,34);
    for(int i=0;i<5;++i)viewTabs[static_cast<std::size_t>(i)].setBounds(12+i*236,86,230,32);
    splitChoose.setBounds(28,132,260,30);splitStart.setBounds(300,132,150,30);splitCancel.setBounds(462,132,100,30);splitProgressBar.setBounds(584,132,580,30);splitStatus.setBounds(28,165,1136,24);
    const std::array<int,5> rows{0,3,2,1,4};
    for(int i=0;i<5;++i){const auto n=static_cast<std::size_t>(i);const int y=230+rows[n]*66;
        imports[n].setBounds(28,y+27,100,22);solos[n].setBounds(142,y+8,32,25);mutes[n].setBounds(180,y+8,32,25);pans[n].setBounds(142,y+40,125,15);volumes[n].setBounds(218,y-1,47,47);
        waveformBounds[n]={282,y,598,55};stemExports[n].setBounds(912,y+12,186,31);stemDrags[n].setBounds(1104,y+12,60,31);
    }
    exportMix.setBounds(912,558,252,30);ideas.setBounds(124,606,240,30);detect.setBounds(382,606,190,30);cancel.setBounds(584,606,180,30);exportMidi.setBounds(1044,606,120,30);
    chordInput.setBounds(28,660,594,32);apply.setBounds(638,660,126,32);timelineZoom.setBounds(768,656,112,18);timelineScroll.setBounds(768,682,112,18);
    chordBounds={28,706,852,90};partChoice.setBounds(912,640,252,25);midiBounds={914,668,248,58};midiBar.setBounds(914,728,248,18);
    generate.setButtonText("Generate selected part");generate.setBounds(912,750,252,28);preview.setBounds(912,782,160,24);dragMidi.setBounds(1090,782,74,24);status.setBounds(24,812,1152,24);
    for(auto* c:std::initializer_list<juce::Component*>{guitarPanel.get(),cinematicPanel.get(),tempoPanel.get(),settingsPanel.get()})if(c)c->setBounds(12,128,1176,680);
    for(int i=0;i<5;++i)rebuildWaveform(i);
    for(auto* child:getChildren())child->setTransform(juce::AffineTransform::scale(uiScale));
}
void WingmanEditor::paint(juce::Graphics& g) {
    g.fillAll(wingman::background);
    g.addTransform(juce::AffineTransform::scale(uiScale));
    g.setColour(wingman::panel); g.fillRoundedRectangle(12,12,1176,65,12);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(18.0f,juce::Font::bold)); g.drawText("JASWINGMAN",28,26,128,29,juce::Justification::centredLeft);g.setColour(wingman::accent);g.drawText("PRO",157,26,45,29,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(11.0f)); g.setColour(juce::Colours::lightgrey);
    const bool processing=processor.analysisIsRunning()||processor.modelJobStatus().running||processor.guitarStatus().running||processor.tempoAnalysisStatus().running;
    g.setColour(processing?wingman::accent:juce::Colours::grey);g.fillEllipse(1040,64,5,5);
    bool audioLoaded=false;for(const auto& clip:cachedClips)audioLoaded|=clip!=nullptr;
    const auto activity=processing?"Analyzing / rendering":processor.hostSync()?(processor.hostPosition()?"DAW synchronized":"DAW timing unavailable"):processor.isPlaying()?"Playback running":audioLoaded?"Audio loaded":"No audio imported";
    g.drawText(activity,1050,59,110,16,juce::Justification::centredLeft);
    g.setColour(juce::Colours::lightgrey);
    g.drawText("KEY",493,11,72,15,juce::Justification::centredLeft); g.drawText("BPM",581,11,70,15,juce::Justification::centredLeft);

    if(activeView!=0)return;
    g.setColour(wingman::panel); g.fillRoundedRectangle(12,124,1176,70,10); g.fillRoundedRectangle(12,200,1176,392,14); g.fillRoundedRectangle(12,600,1176,210,14);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
    g.drawText("INPUT TRACKS (5)",28,207,210,23,juce::Justification::centredLeft);
    g.drawText("EXPORT STEMS",912,207,252,23,juce::Justification::centredLeft);
    g.drawText("IDEAS",28,606,90,32,juce::Justification::centredLeft);

    double duration = 1;
    for (const auto& clip : cachedClips) if (clip) duration = std::max(duration,clip->audio.getNumSamples()/clip->sampleRate);
    g.setColour(juce::Colours::lightgrey); g.setFont(juce::FontOptions(11.0f));
    for (int division = 0; division < 8; ++division)
        g.drawText(juce::String(duration*division/8.0,1) + " s",282+division*598/8,211,65,18,juce::Justification::centredLeft);
    for (int i = 0; i < 5; ++i) {
        const auto n = static_cast<std::size_t>(i); const auto bounds = waveformBounds[n];
        g.setColour(wingman::trackColours[n]); g.setFont(juce::FontOptions(16.0f,juce::Font::bold));
        g.drawText(names[n],28,bounds.getY(),110,25,juce::Justification::centredLeft);
        g.setColour(juce::Colour{0xff101219}); g.fillRoundedRectangle(bounds.toFloat(),5);
        g.setColour(juce::Colours::white.withAlpha(0.07f));
        for (int bar = 1; bar < 8; ++bar) g.drawVerticalLine(bounds.getX()+bar*bounds.getWidth()/8,static_cast<float>(bounds.getY()),static_cast<float>(bounds.getBottom()));
        g.setColour(wingman::trackColours[n]);
        g.strokePath(waveformPaths[n],juce::PathStrokeType(1));
        if (!cachedClips[n]) { g.setFont(juce::FontOptions(13.0f)); g.drawText("Import a WAV or record input",bounds,juce::Justification::centred); }
        else {
            const double x = processor.positionSeconds()/duration;
            if (x <= 1) { g.setColour(juce::Colours::white.withAlpha(0.8f)); g.drawVerticalLine(bounds.getX()+static_cast<int>(x*bounds.getWidth()),static_cast<float>(bounds.getY()),static_cast<float>(bounds.getBottom())); }
        }
    }
    const auto pulse=processor.tempoSnapshot();
    if(pulse.valid){g.setColour(juce::Colours::yellow.withAlpha(0.3f));for(double beat:pulse.beats)if(beat<=duration)for(const auto& bounds:waveformBounds)g.drawVerticalLine(bounds.getX()+static_cast<int>(beat/duration*bounds.getWidth()),static_cast<float>(bounds.getY()),static_cast<float>(bounds.getBottom()));}
    const auto harmony = processor.analysisSnapshot();
    for (const auto& region : harmony.regions) {
        const auto x = 282.0f+static_cast<float>(region.start/duration*598);
        const auto width = static_cast<float>((region.end-region.start)/duration*598);
        g.setColour(wingman::accent.withAlpha(0.25f)); g.fillRect(x,217.0f,width,12.0f);
        g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(10.0f));
        g.drawText(juce::String(wingman::chordName(region.chord)),static_cast<int>(x),216,static_cast<int>(width),13,juce::Justification::centredLeft);
    }
    g.setColour(juce::Colours::lightgrey); g.setFont(juce::FontOptions(12.0f));
    g.drawText("CHORDS | suffix ^inversion:beats | MIDI loops while playing",28,640,800,18,juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xff101219}); g.fillRoundedRectangle(chordBounds.toFloat(),6); g.fillRoundedRectangle(midiBounds.toFloat(),6);
    auto tokens = juce::StringArray::fromTokens(processor.progression()," ,|\t\n",""); tokens.removeEmptyStrings();
    if (!tokens.isEmpty()) {
        double totalBeats = 0;
        for (const auto& token : tokens) if (auto chord = wingman::parseChord(token.toStdString())) totalBeats += chord->beats;
        totalBeats = std::max(0.25,totalBeats);
        double cursor = 0;
        for (int i = 0; i < tokens.size(); ++i) {
            auto chord = wingman::parseChord(tokens[i].toStdString());
            const double durationBeats = chord ? chord->beats : 4;
            const float width = static_cast<float>(durationBeats/totalBeats*chordBounds.getWidth()*timelineZoom.getValue());
            const float left = chordBounds.getX()+static_cast<float>((cursor/totalBeats-timelineScroll.getValue()*(1-1/timelineZoom.getValue()))*chordBounds.getWidth()*timelineZoom.getValue());
            cursor += durationBeats;
            auto r = juce::Rectangle<float>(left,static_cast<float>(chordBounds.getY()),std::max(1.0f,width-2),static_cast<float>(chordBounds.getHeight()));
            g.saveState(); g.reduceClipRegion(chordBounds);
            g.setColour(wingman::trackColours[static_cast<std::size_t>(i%4)].withAlpha(0.18f)); g.fillRect(r);
            g.setColour(wingman::trackColours[static_cast<std::size_t>(i%4)]); g.fillRect(r.removeFromTop(3));
            g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(18.0f)); g.drawText(tokens[i],r,juce::Justification::centred);
            g.restoreState();
            if (processor.selectedPart() < 0 && processor.midiIsGenerated()) if (auto chord = wingman::parseChord(tokens[i].toStdString()))
                for (auto note : chord->notes(processor.key())) {
                    const float x = midiBounds.getX()+i*static_cast<float>(midiBounds.getWidth())/tokens.size();
                    const float y = midiBounds.getBottom()-8-(note-48)*1.6f;
                    g.setColour(wingman::accent); g.fillRoundedRectangle(x+3,y,static_cast<float>(midiBounds.getWidth())/tokens.size()-5,3,1);
                }
        }
    }
    if (processor.selectedPart() >= 0) {
        const auto notes = processor.partNotes(processor.selectedPart());
        const auto settings = processor.partSettings(processor.selectedPart());
        g.saveState(); g.reduceClipRegion(midiBounds);
        if(processor.selectedPart()==1) {
            const int bar=juce::jlimit(0,settings.bars-1,static_cast<int>(midiBar.getValue())-1);
            for(int row=0;row<9;++row)for(int step=0;step<16;++step){
                const double beat=bar*4+step*0.25;
                const bool enabled=std::any_of(notes.begin(),notes.end(),[&](const auto& e){return e.note==settings.drumMap[static_cast<std::size_t>(row)]&&std::abs(e.beat-beat)<0.12;});
                g.setColour(enabled ? wingman::trackColours[3] : juce::Colour{0xff303c43});
                g.fillRect(midiBounds.getX()+step*midiBounds.getWidth()/16+1,midiBounds.getY()+row*midiBounds.getHeight()/9+1,midiBounds.getWidth()/16-2,midiBounds.getHeight()/9-2);
            }
        } else {
            int lowest=127,highest=0;for(const auto& note:notes){lowest=std::min(lowest,note.note);highest=std::max(highest,note.note);}
            const double zoom=timelineZoom.getValue(),scroll=timelineScroll.getValue()*(1-1/zoom);
            for (const auto& note : notes) {
                const float x=midiBounds.getX()+static_cast<float>((note.beat/(settings.bars*4)-scroll)*zoom*midiBounds.getWidth());
                const float width=std::max(2.0f,static_cast<float>(note.duration/(settings.bars*4)*midiBounds.getWidth()*zoom));
                const float y=midiBounds.getBottom()-5-static_cast<float>(note.note-lowest)/std::max(1,highest-lowest)*(midiBounds.getHeight()-10);
                g.setColour(wingman::trackColours[static_cast<std::size_t>(processor.selectedPart()+1)]);g.fillRect(x,y,width,3.0f);
            }
        }
        g.restoreState();
    }
}

void WingmanEditor::mouseDown(const juce::MouseEvent& event) {
    const auto point = event.position/uiScale;
    if(midiBounds.toFloat().contains(point) && processor.selectedPart()==1) {
        const auto settings=processor.partSettings(1);
        const int bar=juce::jlimit(0,settings.bars-1,static_cast<int>(midiBar.getValue())-1);
        showResult(processor.toggleDrumStep(bar,static_cast<int>((point.y-midiBounds.getY())/midiBounds.getHeight()*9),static_cast<int>((point.x-midiBounds.getX())/midiBounds.getWidth()*16)),"Drum step changed"); repaint(); return;
    }
    if(midiBounds.toFloat().contains(point)&&processor.selectedPart()==3)return;
    if(midiBounds.toFloat().contains(point)&&processor.selectedPart()>=0) {
        const int part=processor.selectedPart();const auto notes=processor.partNotes(part);const auto settings=processor.partSettings(part);
        int lowest=127,highest=0;for(const auto& note:notes){lowest=std::min(lowest,note.note);highest=std::max(highest,note.note);}
        const double zoom=timelineZoom.getValue(),scroll=timelineScroll.getValue()*(1-1/zoom);
        int selected=-1;double best=1.0e9;
        for(int i=0;i<static_cast<int>(notes.size());++i){const auto& note=notes[static_cast<std::size_t>(i)];
            const double x=midiBounds.getX()+(note.beat/(settings.bars*4)-scroll)*zoom*midiBounds.getWidth();
            const double y=midiBounds.getBottom()-5-static_cast<double>(note.note-lowest)/std::max(1,highest-lowest)*(midiBounds.getHeight()-10);
            const double distance=std::abs(point.x-x)+std::abs(point.y-y)*2;
            if(distance<best){best=distance;selected=i;}
        }
        if(selected<0)return;
        const auto note=notes[static_cast<std::size_t>(selected)];
        auto* dialog=new juce::AlertWindow("Edit MIDI note","Beat-domain timing; overlapping monophonic notes are rejected",juce::MessageBoxIconType::NoIcon);
        dialog->addTextEditor("pitch",juce::String(note.note),"MIDI pitch (0-127)");
        dialog->addTextEditor("beat",juce::String(note.beat,4),"Start beat");dialog->addTextEditor("duration",juce::String(note.duration,4),"Duration in beats");
        dialog->addTextEditor("velocity",juce::String(note.velocity),"Velocity (1-127)");dialog->addButton("Apply",1);dialog->addButton("Delete",2);dialog->addButton("Cancel",0);
        dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe=juce::Component::SafePointer<WingmanEditor>(this),dialog,note,part,selected](int result)mutable{
            if(!safe||result==0)return;auto changed=note;changed.note=dialog->getTextEditorContents("pitch").getIntValue();changed.beat=dialog->getTextEditorContents("beat").getDoubleValue();
            changed.duration=dialog->getTextEditorContents("duration").getDoubleValue();changed.velocity=dialog->getTextEditorContents("velocity").getIntValue();
            safe->showResult(safe->processor.editPartNote(part,selected,changed,result==2),result==2?"Note deleted":"Note updated");safe->repaint();
        }),true);return;
    }
    for(const auto& lane:waveformBounds)if(lane.toFloat().contains(point)){double duration=1;for(const auto& clip:cachedClips)if(clip)duration=std::max(duration,clip->audio.getNumSamples()/clip->sampleRate);processor.seekSeconds((point.x-lane.getX())/lane.getWidth()*duration);return;}
    if (!chordBounds.toFloat().contains(point)) return;
    auto tokens = juce::StringArray::fromTokens(processor.progression()," ,|\t\n",""); tokens.removeEmptyStrings();
    double total = 0; for (const auto& token : tokens) if (auto chord = wingman::parseChord(token.toStdString())) total += chord->beats;
    const double clicked = ((point.x-chordBounds.getX())/chordBounds.getWidth()/timelineZoom.getValue()
        +timelineScroll.getValue()*(1-1/timelineZoom.getValue()))*total;
    double cursor = 0;
    for (int i = 0; i < tokens.size(); ++i) {
        auto chord = wingman::parseChord(tokens[i].toStdString()); if (!chord) continue;
        if (clicked >= cursor && clicked < cursor+chord->beats) {
            auto* dialog = new juce::AlertWindow("Edit chord","Root, quality, inversion and duration",juce::MessageBoxIconType::NoIcon);
            dialog->addComboBox("root",keyNames,"Root"); dialog->getComboBoxComponent("root")->setSelectedId(chord->root+1);
            dialog->addComboBox("quality",{"Major","Minor","Dominant 7","Major 7","Minor 7","Sus2","Sus4","Diminished","Major 9","Minor 9","Add9","Minor add9","Dominant 9","Dominant 11","Minor 11","Major 13","Minor 13","Dominant 13","7 flat9","7 sharp9","7 sharp11","7 flat13","Major 11","Major 6","Minor 6"},"Quality");
            dialog->getComboBoxComponent("quality")->setSelectedId(static_cast<int>(chord->quality)+1);
            dialog->addComboBox("inversion",{"Root position","First","Second","Third","Fourth","Fifth","Sixth","Seventh"},"Inversion");
            dialog->getComboBoxComponent("inversion")->setSelectedId(chord->inversion+1);
            juce::StringArray bassNames{"Chord bass"};bassNames.addArray(keyNames);dialog->addComboBox("bass",bassNames,"Slash bass");dialog->getComboBoxComponent("bass")->setSelectedId(chord->bass+2);
            dialog->addTextEditor("beats",juce::String(chord->beats),"Duration in beats (0.25-32)");
            dialog->addButton("Apply",1); dialog->addButton("Cancel",0);
            dialog->enterModalState(true,juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<WingmanEditor>(this),dialog,tokens,i](int result) mutable {
                if (!safe || result != 1) return;
                const juce::StringArray suffix{"","m","7","maj7","m7","sus2","sus4","dim","maj9","m9","add9","madd9","9","11","m11","maj13","m13","13","7b9","7#9","7#11","7b13","maj11","6","m6"};
                const int root = dialog->getComboBoxComponent("root")->getSelectedId()-1;
                const int quality = dialog->getComboBoxComponent("quality")->getSelectedId()-1;
                const int bass=dialog->getComboBoxComponent("bass")->getSelectedId()-2;
                tokens.set(i,keyNames[root]+suffix[quality]+(bass>=0?"/"+keyNames[bass]:"")+"^"+juce::String(dialog->getComboBoxComponent("inversion")->getSelectedId()-1)+":"+dialog->getTextEditorContents("beats"));
                const auto text = tokens.joinIntoString(" ");
                const auto outcome = safe->processor.setProgression(text);
                if (outcome.wasOk()) safe->chordInput.setText(text);
                safe->showResult(outcome,"Chord updated");
            }),true);
            break;
        }
        cursor += chord->beats;
    }
}

bool WingmanEditor::isInterestedInFileDrag(const juce::StringArray& files){
    return files.size()==1 && juce::File(files[0]).hasFileExtension("wav;aif;aiff;flac;mp3;m4a");
}
void WingmanEditor::filesDropped(const juce::StringArray& files,int,int){
    if(!isInterestedInFileDrag(files))return;
    if(processor.modelJobStatus().running){splitStatus.setText("Wait for the current operation or Cancel",juce::dontSendNotification);return;}
    splitInput=juce::File(files[0]);splitChoose.setTooltip(splitInput.getFullPathName());
    splitStatus.setText("Ready: "+splitInput.getFileName()+" | Separate replaces four stems; Guitar is preserved",juce::dontSendNotification);
}
void WingmanEditor::startSplit(const juce::File& input){
    const auto result=processor.startModelJob("separate",input);
    splitStatus.setText(result.wasOk()?"Starting separation...":result.getErrorMessage(),juce::dontSendNotification);
}

void WingmanEditor::selectView(int view){
    activeView=juce::jlimit(0,4,view);
    for(auto* child:mainContent)child->setVisible(activeView==0);
    if(guitarPanel){guitarPanel->setVisible(activeView==1);if(activeView==1)guitarPanel->reloadSettings();}
    if(cinematicPanel)cinematicPanel->setVisible(activeView==2);
    if(tempoPanel)tempoPanel->setVisible(activeView==3);
    if(settingsPanel){settingsPanel->setVisible(activeView==4);if(activeView==4)settingsPanel->refresh();}
    for(int i=0;i<5;++i)viewTabs[static_cast<std::size_t>(i)].setToggleState(i==activeView,juce::dontSendNotification);
    repaint();
}
