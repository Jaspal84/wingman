#pragma once
#include "PluginProcessor.h"
#include "GUI/Theme.h"

class WingmanEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit WingmanEditor(WingmanProcessor&);
    ~WingmanEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    WingmanProcessor& processor;
    wingman::Theme theme;
    juce::TextButton record{"Record audio"}, playButton{"Play"}, stopButton{"Stop"};
    juce::ComboBox keyBox, recordTarget, ideas;
    juce::Slider bpm;
    juce::Label status;
    juce::TextEditor chordInput;
    juce::TextButton apply{"Apply chords"}, generate{"Generate MIDI"}, exportMix{"Export WAV mix"}, exportMidi{"Export MIDI"};
    juce::ToggleButton preview{"Audition chords"};
    std::array<juce::TextButton,4> imports, solos, mutes, stemExports;
    std::array<juce::Slider,4> volumes;
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
