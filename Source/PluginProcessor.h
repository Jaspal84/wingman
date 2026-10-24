#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Audio/Track.h"
#include "MIDI/ChordSequence.h"

class WingmanProcessor final : public juce::AudioProcessor {
public:
    WingmanProcessor();
    ~WingmanProcessor() override = default;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Wingman Pro"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    double getTailLengthSeconds() const override { return 0.02; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::Result importWav(int track, const juce::File&);
    juce::Result exportWav(const juce::File&, int track = -1);
    juce::Result exportMidi(const juce::File&);
    juce::Result setProgression(const juce::String&);
    juce::String progression() const;
    void setTempo(double);
    double tempo() const;
    void setKey(int);
    int key() const;
    void generateMidi();
    void setPreview(bool);
    bool preview() const;
    void play();
    void stop();
    bool isPlaying() const;
    double positionSeconds() const;
    wingman::Track trackSnapshot(int) const;
    void setTrackMix(int, float, bool, bool);
    void setTrackPan(int, float);
    void setEditorSize(int, int);
    std::pair<int,int> editorSize() const;
    juce::Result beginRecording(int);
    juce::Result finishRecording();
    bool isRecording() const;
    juce::String detectTempo(int);
    bool midiIsGenerated() const;
private:
    mutable juce::CriticalSection mutex;
    std::array<wingman::Track,4> tracks;
    double engineRate = 44100, bpm = 120, seconds = 0;
    int transpose = 0, editorWidth = 1200, editorHeight = 760;
    bool playing = false, recording = false, previewEnabled = true, generated = false, panic = false;
    std::vector<wingman::Chord> chords;
    juce::String chordText = "C Am F G";
    juce::MidiMessageSequence sequence;
    juce::AudioBuffer<float> recordBuffer;
    int recordSamples = 0, recordTrack = 0;
    juce::Synthesiser synth;
    void rebuildMidi();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WingmanProcessor)
};
