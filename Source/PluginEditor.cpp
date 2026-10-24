#include "PluginEditor.h"
namespace {
const std::array<juce::String,4> names{"Vocals", "Other", "Bass", "Drums"};
const juce::StringArray keyNames{"C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"};
}
WingmanEditor::WingmanEditor(WingmanProcessor& p) : AudioProcessorEditor(p), processor(p) {
    setLookAndFeel(&theme);
    const auto size = processor.editorSize();
    setResizable(true,false); setResizeLimits(960,608,1920,1216);
    setSize(size.first,size.second);
    addAndMakeVisible(models);
    models.onClick = [] {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,"Model Manager",
            "HTDemucs 4-stem model: not configured.\nAI separation is unavailable until verified weights and the pinned Python worker are installed.\n\nNo model is downloaded automatically. See Models/README.md for installation, license and checksums.","Close");
    };
    for (auto* component : std::initializer_list<juce::Component*>{&record,&playButton,&stopButton,&keyBox,&recordTarget,&bpm,&status,&chordInput,&apply,&generate,&exportMix,&exportMidi,&preview,&ideas}) addAndMakeVisible(component);
    keyBox.addItemList(keyNames,1);
    keyBox.setTooltip("Transpose the written chord progression by this many semitones from C");
    keyBox.onChange = [this] { processor.setKey(keyBox.getSelectedId()-1); };
    recordTarget.addItemList({"Vocals", "Other", "Bass", "Drums"},1); recordTarget.setSelectedId(1);
    bpm.setRange(40,240,0.1); bpm.setSliderStyle(juce::Slider::IncDecButtons);
    bpm.setTextBoxStyle(juce::Slider::TextBoxLeft,false,65,28);
    bpm.onValueChange = [this] { processor.setTempo(bpm.getValue()); };
    record.setColour(juce::TextButton::buttonOnColourId,juce::Colour{0xffa43848});
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
    for (int i = 0; i < 4; ++i) {
        const auto n = static_cast<std::size_t>(i);
        for (auto* c : std::initializer_list<juce::Component*>{&imports[n],&solos[n],&mutes[n],&volumes[n],&pans[n],&stemExports[n]}) addAndMakeVisible(c);
        imports[n].setButtonText("Import WAV"); imports[n].onClick = [this,i] { importTrack(i); };
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
        stemExports[n].setButtonText("Export " + names[n]); stemExports[n].onClick = [this,i] { chooseExport(false,i); };
    }
    chordInput.setMultiLine(false); chordInput.setText(processor.progression());
    chordInput.setTooltip("1-16 major/minor chords: C Am F G. Each chord lasts four beats.");
    auto applyChords = [this] { showResult(processor.setProgression(chordInput.getText()),"Progression updated; one bar per chord"); repaint(); };
    apply.onClick = applyChords; chordInput.onReturnKey = applyChords;
    generate.onClick = [this] {
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
    refresh(); startTimerHz(20);
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
    chooser = std::make_unique<juce::FileChooser>("Import WAV into " + names[static_cast<std::size_t>(i)],juce::File{},"*.wav");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safe = juce::Component::SafePointer<WingmanEditor>(this),i](const juce::FileChooser& fc) {
            if (!safe || fc.getResult() == juce::File{}) return;
            auto result = safe->processor.importWav(i,fc.getResult());
            safe->showResult(result,"Imported " + fc.getResult().getFileName());
            if (result.wasOk()) safe->status.setText(safe->processor.detectTempo(i),juce::dontSendNotification);
            safe->refresh();
        });
}
void WingmanEditor::chooseExport(bool midi, int track) {
    const auto extension = midi ? ".mid" : ".wav";
    chooser = std::make_unique<juce::FileChooser>(midi ? "Export standard MIDI file" : "Export WAV audio",
        juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("Wingman" + juce::String(extension)), "*" + juce::String(extension));
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
        [safe = juce::Component::SafePointer<WingmanEditor>(this),midi,track,extension](const juce::FileChooser& fc) {
            if (!safe || fc.getResult() == juce::File{}) return;
            const auto file = fc.getResult().withFileExtension(extension);
            safe->showResult(midi ? safe->processor.exportMidi(file) : safe->processor.exportWav(file,track),"Exported " + file.getFileName());
        });
}
void WingmanEditor::refresh() {
    keyBox.setSelectedId(processor.key()+1,juce::dontSendNotification);
    bpm.setValue(processor.tempo(),juce::dontSendNotification);
    preview.setToggleState(processor.preview(),juce::dontSendNotification);
    record.setButtonText(processor.isRecording() ? "Finish recording" : "Record audio");
    record.setToggleState(processor.isRecording(),juce::dontSendNotification);
    playButton.setToggleState(processor.isPlaying(),juce::dontSendNotification);
    bool changed = false;
    for (int i = 0; i < 4; ++i) {
        const auto n = static_cast<std::size_t>(i); const auto t = processor.trackSnapshot(i);
        pans[n].setValue(t.pan,juce::dontSendNotification);
        volumes[n].setValue(t.gain,juce::dontSendNotification); mutes[n].setToggleState(t.mute,juce::dontSendNotification);
        solos[n].setToggleState(t.solo,juce::dontSendNotification);
        if (cachedClips[n] != t.clip) { cachedClips[n] = t.clip; changed = true; }
    }
    if (changed) for (int i = 0; i < 4; ++i) rebuildWaveform(i);
    repaint();
}
void WingmanEditor::timerCallback() {
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
        for (int s = begin; s < end; ++s) peak = std::max(peak,std::abs(samples[s]));
        const float height = std::min(1.0f,peak)*bounds.getHeight()*0.42f;
        path.startNewSubPath(bounds.getX()+x,bounds.getCentreY()-height);
        path.lineTo(bounds.getX()+x,bounds.getCentreY()+std::max(0.7f,height));
    }
}
void WingmanEditor::resized() {
    uiScale = std::min(getWidth()/1200.0f,getHeight()/760.0f);
    processor.setEditorSize(getWidth(),getHeight());
    models.setBounds(942,24,222,34);
    record.setBounds(206,24,142,34); recordTarget.setBounds(354,24,118,34);
    keyBox.setBounds(493,27,72,30); bpm.setBounds(581,27,120,30);
    playButton.setBounds(719,24,68,34); stopButton.setBounds(795,24,68,34);
    for (int i = 0; i < 4; ++i) {
        const auto n = static_cast<std::size_t>(i); const std::array<int,4> row{0,3,2,1}; const int y = 131+row[n]*84;
        imports[n].setBounds(28,y+32,100,24); solos[n].setBounds(142,y+14,32,28);
        mutes[n].setBounds(180,y+14,32,28); pans[n].setBounds(142,y+47,125,18); volumes[n].setBounds(218,y+4,47,47);
        waveformBounds[n] = {282,y+4,598,59}; stemExports[n].setBounds(912,y+19,252,31); rebuildWaveform(i);
    }
    ideas.setBounds(124,499,240,32); chordInput.setBounds(28,552,594,34); apply.setBounds(638,552,126,34);
    chordBounds = {28,602,852,92}; midiBounds = {914,557,248,78};
    generate.setBounds(912,644,252,32); preview.setBounds(912,680,160,28);
    exportMidi.setBounds(1044,499,120,32); exportMix.setBounds(912,455,252,30);
    status.setBounds(24,720,1152,28);
    for (auto* child : getChildren()) child->setTransform(juce::AffineTransform::scale(uiScale));
}
void WingmanEditor::paint(juce::Graphics& g) {
    g.fillAll(wingman::background);
    g.addTransform(juce::AffineTransform::scale(uiScale));
    g.setColour(wingman::panel); g.fillRoundedRectangle(12,12,1176,65,12);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(21.0f,juce::Font::bold)); g.drawText("WINGMAN PRO",28,26,174,29,juce::Justification::centredLeft);
    g.setFont(juce::FontOptions(11.0f)); g.setColour(juce::Colours::lightgrey);
    g.drawText("KEY",493,11,72,15,juce::Justification::centredLeft); g.drawText("BPM",581,11,70,15,juce::Justification::centredLeft);

    g.setColour(wingman::panel); g.fillRoundedRectangle(12,92,1176,399,14); g.fillRoundedRectangle(12,493,1176,216,14);
    g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(15.0f,juce::Font::bold));
    g.drawText("INPUT TRACKS",28,101,210,23,juce::Justification::centredLeft);
    g.drawText("EXPORT STEMS",912,101,252,23,juce::Justification::centredLeft);
    g.drawText("IDEAS",28,499,90,32,juce::Justification::centredLeft);
    g.drawText("GENERATED MIDI",912,536,240,20,juce::Justification::centredLeft);
    double duration = 1;
    for (const auto& clip : cachedClips) if (clip) duration = std::max(duration,clip->audio.getNumSamples()/clip->sampleRate);
    g.setColour(juce::Colours::lightgrey); g.setFont(juce::FontOptions(11.0f));
    for (int division = 0; division < 8; ++division)
        g.drawText(juce::String(duration*division/8.0,1) + " s",282+division*598/8,104,65,18,juce::Justification::centredLeft);
    for (int i = 0; i < 4; ++i) {
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
    g.setColour(juce::Colours::lightgrey); g.setFont(juce::FontOptions(12.0f));
    g.drawText("CHORD PROGRESSION | 4 beats per chord | MIDI loops while playing",28,532,800,18,juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xff101219}); g.fillRoundedRectangle(chordBounds.toFloat(),6); g.fillRoundedRectangle(midiBounds.toFloat(),6);
    auto tokens = juce::StringArray::fromTokens(processor.progression()," ,|\t\n",""); tokens.removeEmptyStrings();
    if (!tokens.isEmpty()) {
        const float width = static_cast<float>(chordBounds.getWidth())/tokens.size();
        for (int i = 0; i < tokens.size(); ++i) {
            auto r = juce::Rectangle<float>(chordBounds.getX()+i*width,static_cast<float>(chordBounds.getY()),width-2,static_cast<float>(chordBounds.getHeight()));
            g.setColour(wingman::trackColours[static_cast<std::size_t>(i%4)].withAlpha(0.18f)); g.fillRect(r);
            g.setColour(wingman::trackColours[static_cast<std::size_t>(i%4)]); g.fillRect(r.removeFromTop(3));
            g.setColour(juce::Colours::white); g.setFont(juce::FontOptions(18.0f)); g.drawText(tokens[i],r,juce::Justification::centred);
            if (processor.midiIsGenerated()) if (auto chord = wingman::parseChord(tokens[i].toStdString()))
                for (auto note : chord->notes(processor.key())) {
                    const float x = midiBounds.getX()+i*static_cast<float>(midiBounds.getWidth())/tokens.size();
                    const float y = midiBounds.getBottom()-8-(note-48)*1.6f;
                    g.setColour(wingman::accent); g.fillRoundedRectangle(x+3,y,static_cast<float>(midiBounds.getWidth())/tokens.size()-5,3,1);
                }
        }
    }
}
