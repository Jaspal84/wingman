#pragma once
#include "PluginProcessor.h"
#include "GUI/Theme.h"

class WingmanEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit WingmanEditor(WingmanProcessor&);
    ~WingmanEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
private:
    WingmanProcessor& processor;
    wingman::Theme theme;
    juce::TextButton record{"Record audio"}, playButton{"Play"}, stopButton{"Stop"};
    juce::ComboBox keyBox, recordTarget, ideas, partChoice;
    juce::TextButton dragMidi{"Drag"};
    juce::Slider bpm, timelineZoom, timelineScroll, midiBar;
    juce::Label status;
    juce::TextEditor chordInput, projectTitle;
    juce::TextButton apply{"Apply chords"}, generate{"Generate MIDI"}, exportMix{"Export WAV mix"}, exportMidi{"Export MIDI"};
    juce::ToggleButton sync{"DAW"};
    juce::ToggleButton preview{"Audition MIDI"};
    std::array<juce::TextButton,4> imports, solos, mutes, stemExports;
    std::array<juce::Slider,4> volumes, pans;
    juce::TextButton detect{"Detect chords/key"}, cancel{"Cancel analysis"};
    bool analysisWasRunning = false;
    juce::TextButton models{"Model Manager"};
    float uiScale = 1;
    std::array<juce::Rectangle<int>,4> waveformBounds;
    juce::Rectangle<int> chordBounds, midiBounds;
    std::array<std::shared_ptr<const wingman::Clip>,4> cachedClips;
    std::array<juce::Path,4> waveformPaths;
    std::unique_ptr<juce::FileChooser> chooser;
    void timerCallback() override;
    void refresh();
    void updateMix(int);
    void importTrack(int);
    void chooseExport(bool midi, int track = -1);
    void showResult(const juce::Result&, const juce::String&);
    void rebuildWaveform(int);
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WingmanEditor)
};
