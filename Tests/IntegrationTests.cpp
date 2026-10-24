#include "PluginProcessor.h"
#include "GUI/GeneratorPanel.h"
#include "GUI/ModelPanel.h"
#include <iostream>
#include <stdexcept>
static int checks = 0;
static void check(bool value, const char* message) { ++checks; if (std::getenv("WINGMAN_TRACE_TESTS")) std::cerr << "CHECK " << checks << ": " << message << "\n"; if (!value) throw std::runtime_error(message); }
class MockPlayhead final : public juce::AudioPlayHead {
public:
    juce::Optional<PositionInfo> position;
    juce::Optional<PositionInfo> getPosition() const override {return position;}
};
int main() {
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("wingman-tests", "", false);
    directory.createDirectory();
    try {
        WingmanProcessor p; p.prepareToPlay(48000,512);
        juce::AudioBuffer<float> input(2,4800);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 4800; ++i) input.setSample(ch,i,0.25f);
        const auto source = directory.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(source),48000,2,24,{},0));
            check(writer && writer->writeFromAudioSampleBuffer(input,0,4800),"create WAV fixture");
        }
        check(p.importWav(0,source).wasOk(),"import valid WAV");
        check(p.importWav(1,directory.getChildFile("missing.wav")).failed(),"reject missing file");
        check(p.importWav(9,source).failed(),"reject invalid track");
        auto clip = p.trackSnapshot(0).clip;
        check(clip && clip->audio.getNumSamples() == 4800,"decoded WAV samples");
        juce::AudioBuffer<float> block(2,512); juce::MidiBuffer midi;
        p.play(); block.clear(); p.processBlock(block,midi);
        check(std::abs(block.getSample(0,100)-0.2f)<0.00001f,"actual audio playback and gain");
        p.setTrackMix(0,1,true,false); midi.clear(); p.processBlock(block,midi);
        check(block.getMagnitude(0,512) == 0,"mute silences audio");
        p.setTrackMix(0,0.5f,false,false); p.setTrackMix(1,1,false,true);
        midi.clear(); p.processBlock(block,midi); check(block.getMagnitude(0,512) == 0,"solo excludes other tracks");
        p.setTrackMix(1,1,false,false); p.stop(); p.play(); midi.clear(); p.processBlock(block,midi);
        check(std::abs(block.getSample(1,100)-0.125f)<0.00001f,"unmute playback");
        p.stop(); midi.clear(); p.processBlock(block,midi); check(block.getMagnitude(0,512) == 0,"stop silences and resets");
        check(p.positionSeconds() == 0,"stop rewinds");
        check(p.setProgression("C Am F G").wasOk(),"edit chords");
        check(p.setProgression("C7 nonsense").failed(),"reject invalid progression");
        p.setKey(2); p.setTempo(100); p.generateMidi(); p.setPreview(false); p.play(); midi.clear(); p.processBlock(block,midi);
        int noteOns = 0; bool root = false;
        for (const auto metadata : midi) { auto msg = metadata.getMessage(); if (msg.isNoteOn()) { ++noteOns; root |= msg.getNoteNumber() == 50; } }
        check(noteOns == 3 && root,"sample-timed transposed MIDI chord emission");
        const auto mid = directory.getChildFile("chords.mid"); check(p.exportMidi(mid).wasOk(),"MIDI export");
        juce::MidiFile readMidi; auto in = mid.createInputStream(); check(in && readMidi.readFrom(*in),"standard MIDI round trip");
        check(readMidi.getTimeFormat() == 960 && readMidi.getNumTracks() == 1,"MIDI division and track");
        int onCount = 0, offCount = 0; bool tempoFound = false;
        const auto* sequence = readMidi.getTrack(0);
        for (int i = 0; i < sequence->getNumEvents(); ++i) { auto msg = sequence->getEventPointer(i)->message;
            onCount += msg.isNoteOn(); offCount += msg.isNoteOff(); tempoFound |= msg.isTempoMetaEvent(); }
        check(onCount == 12 && offCount == 12 && tempoFound,"MIDI notes, releases and tempo metadata");
        const auto mixed = directory.getChildFile("mix.wav"); check(p.exportWav(mixed).wasOk(),"WAV mix export");
        juce::WavAudioFormat wav; std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(mixed.createInputStream().release(),true));
        check(reader && reader->lengthInSamples == 4800 && reader->sampleRate == 48000,"exported WAV format and duration");
        juce::AudioBuffer<float> decoded(2,4800); reader->read(&decoded,0,4800,0,true,true);
        check(std::abs(decoded.getSample(0,100)-0.125f)<0.00001f,"exported WAV mix samples");
        check(p.exportWav(directory.getChildFile("stem.wav"),0).wasOk(),"individual stem export");
        juce::MemoryBlock state; p.getStateInformation(state);
        WingmanProcessor restored; restored.prepareToPlay(48000,512); restored.setStateInformation(state.getData(),static_cast<int>(state.getSize()));
        check(restored.tempo() == 100 && restored.key() == 2 && restored.progression() == "C Am F G","restore musical settings");
        check(restored.midiIsGenerated() && !restored.preview(),"restore MIDI generation and preview");
        check(restored.trackSnapshot(0).clip && restored.trackSnapshot(0).gain == 0.5f,"embedded audio and mix restored");
        restored.play(); midi.clear(); restored.processBlock(block,midi);
        check(std::abs(block.getSample(0,100)-0.125f)<0.00001f,"restored session plays audio without source file lookup");
        p.stop(); check(p.beginRecording(2).wasOk(),"record input start");
        block.clear(); for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 512; ++i) block.setSample(ch,i,0.3f);
        midi.clear(); p.processBlock(block,midi); check(p.finishRecording().wasOk(),"record input finish");
        check(std::abs(p.trackSnapshot(2).clip->audio.getSample(0,100)-0.3f)<0.00001f,"record captures actual input");
        check(p.finishRecording().failed(),"double finish rejected");
        const char bad[] = "broken state"; restored.setStateInformation(bad,sizeof(bad)); check(restored.tempo() == 100,"invalid state ignored");
        // Test MIDI loop boundaries with an awkward block size, independent of audio clips.
        WingmanProcessor timing; timing.prepareToPlay(48000,511);
        check(timing.exportMidi(directory.getChildFile("not-generated.mid")).failed(),"MIDI export requires generation");
        timing.setProgression("C"); timing.setTempo(120); timing.generateMidi(); timing.setPreview(false); timing.play();
        juce::AudioBuffer<float> oddBlock(2,511);
        int timingOns = 0, timingOffs = 0;
        for (int base = 0; base < 192000; base += 511) {
            midi.clear(); timing.processBlock(oddBlock,midi);
            for (auto metadata : midi) {
                const auto msg = metadata.getMessage();
                if (msg.isNoteOn()) { ++timingOns; check(std::abs((base+metadata.samplePosition)%96000) <= 1,"MIDI onset sample alignment across loop"); }
                if (msg.isNoteOff()) { ++timingOffs; check(std::abs((base+metadata.samplePosition)%96000-93600) <= 1,"MIDI release sample alignment"); }
            }
        }
        check(timingOns == 9 && timingOffs == 6,"loop emits each note exactly once across blocks");
        timing.stop(); timing.setPreview(true); timing.play(); midi.clear(); oddBlock.clear(); timing.processBlock(oddBlock,midi);
        check(oddBlock.getMagnitude(0,511)>0.001f,"generated chords produce actual synth audio");
        timing.stop(); midi.clear(); timing.processBlock(oddBlock,midi);
        check(oddBlock.getMagnitude(0,511)==0,"stop immediately clears preview voices");
        check(timing.exportWav(directory.getChildFile("empty.wav")).failed(),"empty mix rejected");
        const auto monoSource = directory.getChildFile("mono.wav");
        {
            juce::AudioBuffer<float> mono(1,2400); juce::FloatVectorOperations::fill(mono.getWritePointer(0),0.4f,2400);
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(monoSource),24000,1,24,{},0));
            check(writer && writer->writeFromAudioSampleBuffer(mono,0,2400),"mono sample-rate fixture");
        }
        WingmanProcessor resample; resample.prepareToPlay(48000,512);
        check(resample.importWav(0,monoSource).wasOk(),"mono WAV import"); resample.play(); midi.clear(); resample.processBlock(block,midi);
        check(std::abs(block.getSample(0,100)-0.32f)<0.00001f && std::abs(block.getSample(1,100)-0.32f)<0.00001f,"mono duplication and cross-rate playback");
        resample.setTrackMix(0,0.8f,true,false);
        check(resample.exportWav(directory.getChildFile("muted.wav")).failed(),"muted mix rejected");
        check(resample.exportWav(directory.getChildFile("muted-stem.wav"),0).wasOk(),"individual export ignores mute");
        check(wingman::interpolate(*clip,0,1.0e30)==0,"out-of-range interpolation is silent");
        p.stop(); p.setTrackMix(0,0.5f,false,false); p.setTrackMix(2,0,false,false); p.setPreview(false); p.setTrackPan(0,1); p.play();
        midi.clear(); p.processBlock(block,midi);
        check(block.getSample(0,100)==0 && std::abs(block.getSample(1,100)-0.125f)<0.00001f,"stereo balance routes right");
        p.setEditorSize(1440,912); juce::MemoryBlock extendedState; p.getStateInformation(extendedState);
        restored.setStateInformation(extendedState.getData(),static_cast<int>(extendedState.getSize()));
        check(restored.trackSnapshot(0).pan == 1 && restored.editorSize() == std::pair<int,int>{1440,912},"persist pan and editor dimensions");
        p.setEditorSize(1200,760);
        const auto harmonicSource = directory.getChildFile("harmony.wav");
        {
            juce::AudioBuffer<float> tones(2,96000); tones.clear();
            for (int note : {48,52,55}) for (int i=0;i<96000;++i) for (int ch=0;ch<2;++ch)
                tones.addSample(ch,i,static_cast<float>(0.15*std::sin(juce::MathConstants<double>::twoPi*juce::MidiMessage::getMidiNoteInHertz(note)*i/48000)));
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(harmonicSource),48000,2,24,{},0));
            check(writer && writer->writeFromAudioSampleBuffer(tones,0,96000),"harmonic audio fixture");
        }
        check(p.importWav(3,harmonicSource).wasOk() && p.startAnalysis(3).wasOk(),"background harmonic analysis start");
        for (int wait=0;wait<1000 && p.analysisIsRunning();++wait) juce::Thread::sleep(5);
        const auto harmony = p.analysisSnapshot();
        check(!p.analysisIsRunning() && !harmony.regions.empty() && harmony.regions.front().chord.root==0,"background analysis completes with detected C chord");
        check(p.progression().contains("C:") && p.key()==0,"detected duration progression applied");
        p.cancelAnalysis();
        p.setProgression("C Am F G"); p.setKey(0);
        for(int part=0;part<3;++part) {
            wingman::PatternSettings config; config.bars=8; config.seed=123; config.humanize=0.3;
            check(p.generatePart(part,config).wasOk(),"generate selected MIDI part");
            const auto notes=p.partNotes(part); check(!notes.empty(),"generated part contains actual events");
            auto partFile=directory.getChildFile("part-"+juce::String(part)+".mid");
            check(p.exportMidi(partFile).wasOk(),"export generated part");
            juce::MidiFile decodedMidi; auto partStream=partFile.createInputStream(); check(partStream && decodedMidi.readFrom(*partStream),"read generated part MIDI");
            int attacks=0; for(int i=0;i<decodedMidi.getTrack(0)->getNumEvents();++i) attacks+=decodedMidi.getTrack(0)->getEventPointer(i)->message.isNoteOn();
            check(attacks==static_cast<int>(notes.size()),"exported MIDI has all generated notes");
            juce::MemoryBlock generatedState; p.getStateInformation(generatedState);
            restored.setStateInformation(generatedState.getData(),static_cast<int>(generatedState.getSize()));
            check(restored.selectedPart()==part && restored.partNotes(part)==notes,"generated notes and selection persist");
            check(p.varyPart(part,0,4).wasOk(),"phrase variation");
            auto outside=[](const auto& input){std::vector<wingman::NoteEvent> output;for(const auto& note:input)if(note.beat>=16)output.push_back(note);return output;};
            check(outside(notes)==outside(p.partNotes(part)),"variation preserves untouched bars");
        }
        auto drumCount=p.partNotes(1).size(); check(p.toggleDrumStep(0,0,0).wasOk(),"step sequencer edits drum grid");
        check(p.partNotes(1).size()!=drumCount,"step edit changes events");
        auto invalidNote=p.partNotes(2).front(); invalidNote.velocity=0;
        check(p.editPartNote(2,0,invalidNote,false).failed(),"reject invalid manual MIDI edit");
        auto edited=p.partNotes(2).front(); edited.velocity=80; check(p.editPartNote(2,0,edited,false).wasOk()&&p.partNotes(2).front().velocity==80,"manual MIDI velocity edit updates actual events");
        p.selectPart(-1);
        WingmanProcessor hosted; hosted.prepareToPlay(48000,1024); hosted.setPreview(false); hosted.generateMidi();
        MockPlayhead mock; hosted.setPlayHead(&mock); hosted.setHostSync(true);
        juce::AudioPlayHead::PositionInfo position; position.setBpm(120);position.setPpqPosition(0);position.setTimeInSeconds(0);position.setIsPlaying(true);
        position.setTimeSignature(juce::AudioPlayHead::TimeSignature{4,4});mock.position=position;
        juce::AudioBuffer<float> hostAudio(2,1024); midi.clear();hosted.processBlock(hostAudio,midi);
        int hostedOns=0;for(auto metadata:midi)hostedOns+=metadata.getMessage().isNoteOn();
        check(hostedOns==3&&hosted.isPlaying(),"mock host starts chord output");
        position.setPpqPosition(9);position.setTimeInSeconds(4.5);mock.position=position;midi.clear();hosted.processBlock(hostAudio,midi);
        int chased=0,released=0;for(auto metadata:midi){chased+=metadata.getMessage().isNoteOn();released+=metadata.getMessage().isNoteOff();}
        check(chased==3&&released==3,"host seek releases owned notes and chases held harmony");
        position.setIsPlaying(false);mock.position=position;midi.clear();hosted.processBlock(hostAudio,midi);
        released=0;for(auto metadata:midi)released+=metadata.getMessage().isNoteOff();check(released==3&&!hosted.isPlaying(),"host stop releases every generated note");
        mock.position={};midi.clear();hosted.processBlock(hostAudio,midi);check(!hosted.isPlaying()&&!hosted.hostPosition(),"missing host position is safe");
        position.setIsPlaying(true);position.setPpqPosition(3.99);position.setTimeInSeconds(1.995);position.setIsLooping(true);position.setLoopPoints(juce::AudioPlayHead::LoopPoints{0,4});mock.position=position;
        midi.clear();hosted.processBlock(hostAudio,midi);int loopOns=0;for(auto metadata:midi)if(metadata.getMessage().isNoteOn()){++loopOns;check(metadata.samplePosition==240,"host loop MIDI offset");}
        check(loopOns==3,"host loop emits each onset once");
        for(int ch=0;ch<2;++ch)juce::FloatVectorOperations::fill(hostAudio.getWritePointer(ch),0.37f,1024);
        midi.clear();hosted.processBlockBypassed(hostAudio,midi);check(std::abs(hostAudio.getSample(0,100)-0.37f)<1e-6,"bypass preserves dry input");
        released=0;for(auto metadata:midi)released+=metadata.getMessage().isNoteOff();check(released==3,"bypass releases generated notes");
        hosted.setPlayHead(nullptr);hosted.setHostSync(false);
        WingmanProcessor stress;stress.prepareToPlay(48000,512);stress.importWav(0,source);stress.play();
        std::jthread controls([&]{for(int i=0;i<1000;++i)stress.setTrackMix(0,static_cast<float>(i%100)/100,false,false);});
        for(int blockIndex=0;blockIndex<100;++blockIndex){midi.clear();stress.processBlock(block,midi);check(std::isfinite(block.getSample(0,100))&&block.getMagnitude(0,512)<=1,"concurrent control edits produce bounded finite audio");}
        controls.join();
        check(p.importStemSet({source,source,source,source}).wasOk(),"atomic import of synchronized WAV set (not inference)");
        const auto beforeInvalidStems=p.trackSnapshot(0).clip;
        check(p.importStemSet({source,harmonicSource,source,source}).failed(),"reject mismatched stem lengths");
        check(p.trackSnapshot(0).clip==beforeInvalidStems,"invalid stem set does not partially replace tracks");
        check(p.startModelJob("install",{},false).failed(),"plugin requires explicit model download consent");
#ifdef WINGMAN_TEST_PYTHON
        p.configureModel(WINGMAN_TEST_PYTHON,directory.getChildFile("no-model-installed").getFullPathName());
        check(p.startModelJob("check").wasOk(),"embedded model worker subprocess starts");
        for(int wait=0;wait<1000&&p.modelJobStatus().running;++wait)juce::Thread::sleep(5);
        check(!p.modelJobStatus().running&&p.modelJobStatus().message.contains("No such file"),"missing model reported through subprocess status protocol");
        p.cancelModelJob();
#endif
        bool uiGenerated=false;
        wingman::GeneratorPanel generatorPanel(wingman::PatternSettings{},[&](const auto& config){uiGenerated=true;return p.generatePart(0,config);},[&](int first,int length){return p.varyPart(0,first,length);});
        for(auto* child:generatorPanel.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Generate / regenerate")button->onClick();
        check(uiGenerated&&!p.partNotes(0).empty(),"generator parameter-panel button invokes actual engine");
        check(generatorPanel.createComponentSnapshot(generatorPanel.getLocalBounds()).isValid(),"generator parameter panel renders");
        wingman::ModelPanel modelPanel(p.modelConfig(),[&](const auto& runtime,const auto& models){p.configureModel(runtime,models);},
            [&](const auto& action,const auto& input,bool consent){return p.startModelJob(action,input,consent);},[&]{p.cancelModelJob();},[&]{return p.modelJobStatus();});
        check(modelPanel.createComponentSnapshot(modelPanel.getLocalBounds()).isValid(),"model manager renders");
        p.setSessionName("Portable test session"); juce::MemoryBlock namedState;p.getStateInformation(namedState);restored.setStateInformation(namedState.getData(),static_cast<int>(namedState.getSize()));
        check(restored.sessionName()=="Portable test session","project name persists");
        // Optional reproducible visual fixture, never loaded by the application itself.
        if (std::getenv("WINGMAN_SCREENSHOT")) {
            for (int lane = 0; lane < 4; ++lane) {
                juce::AudioBuffer<float> visual(2,384000);
                for (int i = 0; i < visual.getNumSamples(); ++i) {
                    const double t = i/48000.0;
                    const float sample = static_cast<float>(0.65*std::sin(t*(lane+1)*710.0)
                        * (0.2+0.8*std::exp(-std::fmod(t,0.5)*(3+lane*2))));
                    for (int ch = 0; ch < 2; ++ch) visual.setSample(ch,i,sample);
                }
                auto visualFile = directory.getChildFile("visual-"+juce::String(lane)+".wav");
                { std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(visualFile),48000,2,24,{},0));
                  check(writer && writer->writeFromAudioSampleBuffer(visual,0,visual.getNumSamples()),"visual fixture WAV"); }
                check(p.importWav(lane,visualFile).wasOk(),"visual fixture import");
            }
        }
        // Instantiate and lay out the real editor, then rasterise it to exercise GUI paint paths.
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        check(editor->getWidth() == 1200 && editor->getHeight() == 760,"editor created and laid out");
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        check(image.isValid(),"editor paint renders");
        if (const auto* path = std::getenv("WINGMAN_SCREENSHOT")) { juce::PNGImageFormat png; juce::FileOutputStream out{juce::File{path}};
            check(out.openedOk() && out.setPosition(0) && out.truncate().wasOk() && png.writeImageToStream(image,out),"write current editor screenshot"); }
        std::cout << checks << " integration checks passed\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; directory.deleteRecursively(); return 1; }
    directory.deleteRecursively(); return 0;
}
