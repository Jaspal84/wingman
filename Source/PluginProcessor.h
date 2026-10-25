#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "Audio/Track.h"
#include "Music/GuitarRhythmGenerator.h"
#include "Audio/AudioDragCache.h"
#include "AI/ModelManager/ModelTypes.h"
namespace wingman { class ModelJob; }
#include "Audio/PreviewSynth.h"
#include "Host/Transport.h"
#include "MIDI/ChordSequence.h"
#include "MIDI/PatternSequence.h"
#include "Generation/State.h"
#include "AI/ChordDetection/HarmonicAnalysis.h"
#include "Threading/BackgroundThread.h"
#include <atomic>

class WingmanProcessor final : public juce::AudioProcessor {
public:
    WingmanProcessor();
    ~WingmanProcessor() override;
    void prepareToPlay(double, int) override;
    void releaseResources() override;
    using juce::AudioProcessor::processBlock;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlockBypassed(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void setHostSync(bool);
    bool hostSync() const;
    std::optional<wingman::HostFrame> hostPosition() const;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "JASWINGMAN PRO"; }
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
    juce::Result importStemSet(const std::array<juce::File,4>&);
    void configureModel(const juce::String&,const juce::String&);
    std::pair<juce::String,juce::String> modelConfig() const;
    juce::Result startModelJob(const juce::String&,const juce::File& input={},bool consent=false);
    void cancelModelJob();
    wingman::ModelJobInfo modelJobStatus() const;
    juce::Result exportWav(const juce::File&, int track = -1,double outputRate=48000);
    void prepareTrackDrag(int);
    juce::Result trackDragFile(int,juce::File&);
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
    juce::String sessionName() const;
    void setSessionName(const juce::String&);
    std::pair<int,int> editorSize() const;
    juce::Result beginRecording(int);
    juce::Result finishRecording();
    bool isRecording() const;
    juce::String detectTempo(int);
    bool midiIsGenerated() const;
    juce::Result generatePart(int, const wingman::PatternSettings&);
    juce::Result varyPart(int,int,int);
    juce::Result toggleDrumStep(int,int,int);
    void selectPart(int);
    int selectedPart() const;
    wingman::PatternSettings partSettings(int) const;
    std::vector<wingman::NoteEvent> partNotes(int) const;
    juce::Result editPartNote(int, int, const wingman::NoteEvent&, bool remove);

    juce::Result generateGuitarPart(const wingman::GuitarSettings&,bool renderAudio=false,double sampleRate=48000);
    wingman::GuitarSettings guitarConfig() const;
    wingman::ModelJobInfo guitarStatus() const;
    void cancelGuitar();
    juce::Result startAnalysis(int);
    void cancelAnalysis();
    bool analysisIsRunning() const { return analysisBusy.load(); }
    double analysisPercent() const { return analysisProgress.load(); }
    juce::String analysisMessage() const;
    wingman::HarmonicResult analysisSnapshot() const;
private:
    mutable juce::CriticalSection mutex;
    std::array<wingman::Track,5> tracks;
    wingman::AudioDragCache audioDragCache;
    double engineRate = 44100, bpm = 120, seconds = 0;
    int transpose = 0, editorWidth = 1200, editorHeight = 760;
    bool playing = false, recording = false, previewEnabled = true, generated = false, panic = false;
    std::vector<wingman::Chord> chords;
    juce::String projectTitle="Untitled session";
    juce::String chordText = "C Am F G";
    juce::MidiMessageSequence sequence;
    double sequenceLength = 16;
    int activePart = -1;
    std::array<wingman::PatternSettings,3> settings;
    std::array<std::vector<wingman::NoteEvent>,3> parts;
    juce::AudioBuffer<float> recordBuffer;
    int recordSamples = 0, recordTrack = 0;
    wingman::PreviewSynth synth;
    wingman::HostClock hostClock;
    bool syncEnabled=false, hostAvailable=false;
    wingman::HostFrame observedHost;
    std::atomic<bool> recoverMidi{false};
    std::array<std::array<bool,128>,16> ownedNotes{};
    void flushOwnedNotes(juce::MidiBuffer&,int);
    void scheduleMidi(double,int,int,juce::MidiBuffer&);
    void chaseMidi(double,int,juce::MidiBuffer&);
    std::atomic<bool> analysisBusy{false};
    std::atomic<double> analysisProgress{0};
    juce::String analysisStatus;
    wingman::HarmonicResult analysis;
    wingman::GuitarSettings guitarSettings;
    std::vector<wingman::NoteEvent> guitarNotes;
    wingman::BackgroundThread guitarWorker;
    std::atomic<bool> guitarBusy{false};
    std::atomic<double> guitarProgress{0};
    juce::String guitarMessage="Generate a guitar MIDI pattern";
    wingman::BackgroundThread analysisThread;
    wingman::BackgroundThread dragCleanupThread;
    std::unique_ptr<wingman::ModelJob> modelJob;
    juce::String inferencePython,inferenceModels;
    std::atomic<std::uint64_t> audioRevision{0};
    void rebuildMidi();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WingmanProcessor)
};
