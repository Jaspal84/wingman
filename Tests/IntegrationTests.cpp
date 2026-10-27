#include "PluginProcessor.h"
#include "Studio/Project.h"
#include "GUI/GeneratorPanel.h"
#include "GUI/ModelPanel.h"
#include "GUI/GuitarStudioPanel.h"
#include "GUI/CinematicPanel.h"
#include "GUI/TempoPanel.h"
#include "GUI/SettingsPanel.h"
#include "GUI/FileDragButton.h"
#include "Files/DragFileStore.h"
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
        const auto cache=directory.getChildFile("drag-cache-test");cache.createDirectory();
        const auto now=juce::Time::getCurrentTime();
        const auto expired=juce::Time(now.toMilliseconds()-wingman::DragFileStore::retentionMilliseconds-60000);
        for(int i=0;i<3;++i){auto old=cache.getChildFile(juce::Uuid().toString());old.createDirectory();old.getChildFile("notes.mid").replaceWithText("retained host export");old.setLastModificationTime(expired);}
        auto recent=cache.getChildFile(juce::Uuid().toString());recent.createDirectory();recent.getChildFile("notes.mid").replaceWithText("pending host import");
        auto unrelated=cache.getChildFile("user-files");unrelated.createDirectory();unrelated.setLastModificationTime(expired);
        check(wingman::DragFileStore::cleanupExpired(cache,now,2)==2,"drag cleanup caps removals per pass");
        check(recent.getChildFile("notes.mid").existsAsFile(),"delayed host import file survives cleanup");
        check(unrelated.isDirectory(),"cleanup preserves unrelated directories");
        check(wingman::DragFileStore::cleanupExpired(cache,now)==1,"remaining expired exports removed on next pass");
        check(wingman::DragFileStore::cleanupExpired(cache,now)==0,"recent exports retained across repeated cleanup");
        {
            wingman::ArrangementPlaybackEngine arrangement;
            std::array<wingman::Track,8> lanes;
            for(auto& lane:lanes){auto clip=std::make_shared<wingman::Clip>();clip->sampleRate=24000;clip->audio.setSize(1,24);for(int n=0;n<24;++n)clip->audio.setSample(0,n,.05f);lane.clip=clip;lane.gain=1;}
            juce::AudioBuffer<float> mix(2,48);
            auto render=[&](bool loop){arrangement.publish(lanes,loop);arrangement.seek(0);arrangement.playing=true;arrangement.begin();mix.clear();arrangement.render(mix,48000);};
            render(false);check(std::abs(mix.getSample(0,10)-.4f)<1e-6,"all eight arrangement lanes mix with mono-to-stereo rate conversion");
            check(std::abs(mix.getSample(1,10)-.4f)<1e-6,"eight-track stereo output");
            check(!arrangement.playing.load(),"eight-track transport stops at final sample");
            lanes[7].solo=true;render(false);check(std::abs(mix.getSample(0,10)-.05f)<1e-6,"eighth lane participates in solo routing");
            lanes[7].pan=1;render(false);check(mix.getSample(0,10)==0&&std::abs(mix.getSample(1,10)-.05f)<1e-6,"eighth lane stereo pan");
            lanes[7].mute=true;render(false);check(mix.getMagnitude(0,48)==0,"muted solo produces silence");
            lanes[7].mute=false;render(true);arrangement.begin();mix.clear();arrangement.render(mix,48000);check(arrangement.playing.load()&&mix.getSample(1,10)>.04f,"eight-track loop crosses boundary");
            lanes[7].solo=false;for(auto& lane:lanes)lane.gain=100;render(false);check(mix.getMagnitude(0,48)<=1,"eight-track mix clipping protection");
        }
        WingmanProcessor p; p.prepareToPlay(48000,512);
        juce::AudioBuffer<float> input(2,4800);
        for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < 4800; ++i) input.setSample(ch,i,0.25f);
        const auto source = directory.getChildFile("source.wav");
        {
            juce::WavAudioFormat format;
            std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(source),48000,2,24,{},0));
            check(writer && writer->writeFromAudioSampleBuffer(input,0,4800),"create WAV fixture");
        }
        {
            wingman::studio::Project project;project.sources[7]=source;project.loop=true;project.tracks[7].pan=.3f;project.tracks[7].gain=.6f;project.tracks[7].solo=true;
            check(wingman::studio::decode(source,project.tracks[7].clip).wasOk(),"studio decoder reads audio fixture");
            const auto bounced=directory.getChildFile("studio-mix.wav");
            check(wingman::studio::renderMix(bounced,project.tracks,[]{return false;}).wasOk(),"studio offline bounce succeeds");
            {juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatReader> reader(wav.createReaderFor(bounced.createInputStream().release(),true));check(reader&&reader->bitsPerSample==24&&reader->numChannels==2&&reader->sampleRate==48000&&reader->lengthInSamples==4800,"studio bounce readable PCM24 stereo with exact duration");juce::AudioBuffer<float> audio(2,4800);check(reader->read(&audio,0,4800,0,true,true)&&std::abs(audio.getSample(0,10)-.105f)<1e-5&&std::abs(audio.getSample(1,10)-.15f)<1e-5,"studio bounce preserves gain, pan and audible samples");}
            juce::MemoryBlock before,after;bounced.loadFileAsData(before);
            check(wingman::studio::renderMix(bounced,project.tracks,[]{return true;}).failed(),"studio bounce cancellation reports failure");bounced.loadFileAsData(after);check(before==after,"cancelled bounce preserves previous destination");
            const auto session=directory.getChildFile("arrangement.jaspro");
            check(wingman::studio::save(session,project).wasOk(),"studio project saves atomically");
            wingman::studio::Project restored;check(wingman::studio::load(session,restored,[]{return false;}).wasOk(),"studio project reopens");
            check(restored.loop&&restored.tracks[7].solo&&std::abs(restored.tracks[7].gain-.6f)<1e-6&&restored.tracks[7].clip->audio.getNumSamples()==4800,"studio project restores eighth lane asset, mix and loop");
            project.sources[7]=directory.getChildFile("absent.wav");check(wingman::studio::save(session,project).wasOk(),"save missing asset reference fixture");
            auto previous=restored.tracks[7].clip;check(wingman::studio::load(session,restored,[]{return false;}).failed()&&restored.tracks[7].clip==previous,"failed missing-file restore preserves current project");
            check(wingman::studio::load(session,restored,[]{return true;}).failed(),"project restore cancellation");
            session.replaceWithText("not XML");check(wingman::studio::load(session,restored,[]{return false;}).failed(),"studio rejects corrupt project");
        }
        check(p.importWav(0,source).wasOk(),"import valid WAV");
        p.prepareTrackDrag(0);juce::File dragAudio;
        for(int wait=0;wait<1000&&p.trackDragFile(0,dragAudio).failed();++wait)juce::Thread::sleep(5);
        check(dragAudio.existsAsFile(),"independently imported audio is draggable without stem separation");
        {juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> decoded(format.createReaderFor(dragAudio.createInputStream().release(),true));
         check(decoded&&decoded->sampleRate==48000&&decoded->numChannels==2&&decoded->bitsPerSample==24&&decoded->lengthInSamples==4800,"background drag WAV is readable aligned stereo PCM24");}

        check(p.importWav(1,directory.getChildFile("missing.wav")).failed(),"reject missing file");
        {const auto aiff=directory.getChildFile("portable-import.aiff");{juce::AiffAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(new juce::FileOutputStream(aiff),48000,2,24,{},0));check(writer&&writer->writeFromAudioSampleBuffer(input,0,4800),"create AIFF import fixture");}WingmanProcessor codecs;check(codecs.importWav(0,aiff).wasOk()&&codecs.trackSnapshot(0).clip->audio.getNumSamples()==4800,"input lanes decode supported non-WAV audio too");}

        check(p.importWav(9,source).failed(),"reject invalid track");
        auto clip = p.trackSnapshot(0).clip;
        check(!clip->peaks.empty()&&std::abs(clip->peaks.front()-0.25f)<0.000001f,"waveform peak cache prepared with clip");
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
        for(double playbackRate:{44100.,48000.}){WingmanProcessor player;player.prepareToPlay(playbackRate,512);player.setPreview(false);
            for(int lane=0;lane<5;++lane)check(player.importWav(lane,source).wasOk(),"playback fixture imports all five lanes");
            for(int lane=0;lane<5;++lane){for(int i=0;i<5;++i)player.setTrackMix(i,.2f,false,i==lane);player.stop();player.play();midi.clear();player.processBlock(block,midi);check(block.getMagnitude(0,512)>.04f,"each solo lane audibly reaches processBlock output at both rates");}
            for(int lane=0;lane<5;++lane)player.setTrackMix(lane,.2f,false,false);player.stop();player.play();midi.clear();player.processBlock(block,midi);check(std::abs(block.getSample(0,100)-.25f)<.0001f,"simultaneous five-lane sum");check(player.positionSeconds()>0,"playback position advances");
            player.seekSeconds(.099);player.setLooping(true);midi.clear();player.processBlock(block,midi);check(block.getMagnitude(0,512)>.24f&&player.positionSeconds()<.02,"audio wraps inside block at lane endpoint");
            player.setLooping(false);player.seekSeconds(.1);player.play();check(player.positionSeconds()==0,"Play restarts from end instead of silently playing beyond audio");player.seekSeconds(.099);midi.clear();player.processBlock(block,midi);check(!player.isPlaying()&&std::abs(player.positionSeconds()-.1)<.00001,"audio-only transport stops at end when looping is off");player.stop();midi.clear();player.processBlock(block,midi);check(block.getMagnitude(0,512)==0,"stopped five-track output silent");}

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
        check(p.importWav(4,source).wasOk(),"fifth guitar lane import");p.setTrackPan(4,0.25f);p.setTrackMix(4,0.6f,true,true);
        juce::MemoryBlock five;p.getStateInformation(five);restored.setStateInformation(five.getData(),static_cast<int>(five.getSize()));
        check(restored.trackSnapshot(4).clip&&restored.trackSnapshot(4).mute&&restored.trackSnapshot(4).solo&&std::abs(restored.trackSnapshot(4).pan-0.25f)<0.001,"five-track state restores guitar audio and mix");
        auto legacy=juce::ValueTree::readFromData(five.getData(),five.getSize());legacy.setProperty("version",1,nullptr);legacy.removeChild(4,nullptr);
        juce::MemoryOutputStream oldStream;legacy.writeToStream(oldStream);restored.setStateInformation(oldStream.getData(),static_cast<int>(oldStream.getDataSize()));
        check(restored.trackSnapshot(0).clip&&!restored.trackSnapshot(4).clip,"legacy four-track session restores with empty guitar lane");p.setTrackMix(4,0.6f,true,false);

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
                tones.addSample(ch,i,static_cast<float>((ch==0?.15:-.15)*std::sin(juce::MathConstants<double>::twoPi*juce::MidiMessage::getMidiNoteInHertz(note)*i/48000)));
            std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(harmonicSource),48000,2,24,{},0));
            check(writer && writer->writeFromAudioSampleBuffer(tones,0,96000),"harmonic audio fixture");
        }
        check(p.importWav(3,harmonicSource).wasOk() && p.startAnalysis(3).wasOk(),"background harmonic analysis start");
        for (int wait=0;wait<1000 && p.analysisIsRunning();++wait) juce::Thread::sleep(5);
        const auto harmony = p.analysisSnapshot();
        check(!p.analysisIsRunning() && !harmony.regions.empty() && harmony.regions.front().chord.root==0,"background analysis completes with detected C chord");
        {const auto dFile=directory.getChildFile("key-d-major.wav");juce::AudioBuffer<float> tones(2,96000);tones.clear();for(int note:{50,54,57})for(int i=0;i<96000;++i)for(int c=0;c<2;++c)tones.addSample(c,i,static_cast<float>(.15*std::sin(juce::MathConstants<double>::twoPi*juce::MidiMessage::getMidiNoteInHertz(note)*i/48000)));{std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(dFile),48000,2,24,{},0));check(writer&&writer->writeFromAudioSampleBuffer(tones,0,96000),"create non-C key fixture");}WingmanProcessor keyProbe;keyProbe.prepareToPlay(48000,512);keyProbe.setProgression("C");check(keyProbe.importWav(0,dFile).wasOk(),"import non-C key fixture");for(int i=0;i<1000&&keyProbe.analysisIsRunning();++i)juce::Thread::sleep(5);check(keyProbe.analysisSnapshot().keyRoot==2&&keyProbe.applyDetectedKey().wasOk(),"detect and explicitly apply D major key");keyProbe.setPreview(false);keyProbe.generateMidi();keyProbe.play();midi.clear();keyProbe.processBlock(block,midi);bool d=false;for(auto e:midi)if(e.getMessage().isNoteOn()&&e.getMessage().getNoteNumber()==50)d=true;check(d,"applying detected key retains manual chord transposition");check(keyProbe.applyDetectedChords().wasOk(),"apply absolute detected D chords");keyProbe.stop();keyProbe.generateMidi();keyProbe.play();midi.clear();keyProbe.processBlock(block,midi);d=false;for(auto e:midi)if(e.getMessage().isNoteOn()&&e.getMessage().getNoteNumber()==50)d=true;check(d,"detected absolute chords are not transposed twice");}
        check(p.exportDetectedMidi(directory.getChildFile("detected-timeline.mid")).wasOk(),"export actual detected time-aligned chord regions");
        check(!p.progression().contains("C:"),"analysis does not silently replace editable progression");check(p.applyDetectedChords().wasOk()&&p.progression().contains("C:")&&p.key()==0,"explicit detected duration progression applied");
        check(p.correctDetectedChord(0,"Cm").wasOk()&&p.analysisSnapshot().regions.front().chord.minor,"manually correct detected chord without changing region time");check(p.correctDetectedChord(0,"C").wasOk(),"restore detected fixture chord");
        check(p.generateGuitarPart(wingman::GuitarSettings{},false,48000,true).wasOk(),"generate guitar from actual analyzed song timeline");for(int wait=0;wait<1000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);check(p.guitarStatus().progress==1&&!p.partNotes(3).empty(),"song-follow guitar publishes playable MIDI notes");for(const auto& note:p.partNotes(3))check((note.beat+note.duration)*60/p.tempo()<=harmony.regions.back().end+.001,"guitar notes end within actual detected song");
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
            auto roundTrip=*decodedMidi.getTrack(0);roundTrip.updateMatchedPairs();
            int verifiedNotes=0;bool tempo=false,signature=false;
            for(int e=0;e<roundTrip.getNumEvents();++e){
                const auto* event=roundTrip.getEventPointer(e);const auto& msg=event->message;
                if(msg.isTempoMetaEvent()){tempo=true;check(std::abs(msg.getTempoSecondsPerQuarterNote()-60.0/p.tempo())<0.000002,"exact MIDI tempo");}
                if(msg.isTimeSignatureMetaEvent())signature=true;
                if(!msg.isNoteOn())continue;
                check(event->noteOffObject!=nullptr,"all MIDI attacks have paired releases");
                const auto found=std::find_if(notes.begin(),notes.end(),[&](const auto& n){
                    return n.note==msg.getNoteNumber() && n.velocity==msg.getVelocity() && n.channel==msg.getChannel()
                        && std::abs(n.beat*960-msg.getTimeStamp())<=1
                        && std::abs((n.beat+n.duration)*960-event->noteOffObject->message.getTimeStamp())<=1;
                });
                check(found!=notes.end(),"SMF preserves pitch channel velocity start and duration");++verifiedNotes;
            }
            check(verifiedNotes==static_cast<int>(notes.size())&&tempo&&signature,"SMF metadata and every editable note preserved");
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
        WingmanProcessor hosted; hosted.prepareToPlay(48000,1024); hosted.setPreview(false);hosted.importWav(0,source);for(int wait=0;wait<1000&&(hosted.analysisIsRunning()||hosted.tempoAnalysisStatus().running);++wait)juce::Thread::sleep(5); hosted.generateMidi();
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
        check(loopOns==3,"host loop emits each onset once");check(hostAudio.getMagnitude(0,240)==0&&hostAudio.getSample(0,241)>.19f,"host loop wraps imported audio at the same sample as MIDI");
        for(int ch=0;ch<2;++ch)juce::FloatVectorOperations::fill(hostAudio.getWritePointer(ch),0.37f,1024);
        midi.clear();hosted.processBlockBypassed(hostAudio,midi);check(std::abs(hostAudio.getSample(0,100)-0.37f)<1e-6,"bypass preserves dry input");
        released=0;for(auto metadata:midi)released+=metadata.getMessage().isNoteOff();check(released==3,"bypass releases generated notes");
        hosted.setPlayHead(nullptr);hosted.setHostSync(false);
        {WingmanProcessor partial;partial.prepareToPlay(48000,512);partial.importWav(0,source);partial.setPreview(false);partial.setTempo(93);MockPlayhead head;partial.setPlayHead(&head);partial.setHostSync(true);
         juce::AudioPlayHead::PositionInfo pos;pos.setBpm(90);pos.setTimeInSeconds(.02);pos.setIsPlaying(true);pos.setTimeSignature(juce::AudioPlayHead::TimeSignature{3,4});head.position=pos;midi.clear();partial.processBlock(block,midi);
         check(partial.isPlaying()&&std::abs(block.getSample(0,100)-.2f)<.0001f,"seconds-only host timing produces imported audio");check(partial.tempo()==90&&partial.hostPosition()->numerator==3,"host BPM and meter followed");
         pos=juce::AudioPlayHead::PositionInfo{};pos.setTimeInSamples(960);pos.setIsPlaying(true);head.position=pos;midi.clear();partial.processBlock(block,midi);check(partial.hostPosition().has_value()&&partial.hostPosition()->numerator==3&&block.getMagnitude(0,512)>.19f,"sample-position-only host timing supported");
         pos.setIsPlaying(false);head.position=pos;midi.clear();partial.processBlock(block,midi);check(!partial.isPlaying()&&block.getMagnitude(0,512)==0,"host stop silences imported audio");partial.setHostSync(false);check(partial.tempo()==93,"independent tempo restored after host sync");partial.play();midi.clear();partial.processBlock(block,midi);check(block.getMagnitude(0,512)>.19f,"independent transport resumes after sync disabled");}
        {WingmanProcessor continuous;continuous.prepareToPlay(48000,512);continuous.importWav(0,source);continuous.setPreview(false);continuous.setLooping(true);continuous.play();std::thread edits([&]{for(int i=0;i<300;++i)continuous.setProgression(i%2?"Cmaj9 Am9 Fmaj7 G7":"Dm7 G7 Cmaj7 Fmaj7");});for(int i=0;i<300;++i){midi.clear();continuous.processBlock(block,midi);check(block.getMagnitude(0,512)>.19f,"immutable mixer stays audible during concurrent session edits");}edits.join();}

        WingmanProcessor stress;stress.prepareToPlay(48000,512);stress.importWav(0,source);stress.play();
        std::thread controls([&]{for(int i=0;i<1000;++i)stress.setTrackMix(0,static_cast<float>(i%100)/100,false,false);});
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
        {juce::String runtimePath,modelPath;bool invoked=false;
         wingman::ModelPanel trimmed({" /tmp/runtime path/python  "," /tmp/models  "},[&](const auto& runtime,const auto& models){runtimePath=runtime;modelPath=models;},[&](const auto&,const auto&,bool){invoked=true;return juce::Result::ok();},[]{},[]{return wingman::ModelJobInfo{};});
         for(auto* child:trimmed.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()=="Verify local model")button->onClick();
         check(invoked&&runtimePath=="/tmp/runtime path/python"&&modelPath=="/tmp/models","model panel trims boundary whitespace while preserving internal path spaces");}

        p.setSessionName("Portable test session"); juce::MemoryBlock namedState;p.getStateInformation(namedState);restored.setStateInformation(namedState.getData(),static_cast<int>(namedState.getSize()));
        check(restored.sessionName()=="Portable test session","project name persists");
        if(const auto* modelsPath=std::getenv("WINGMAN_REAL_MODELS")) {
            const auto* runtime=std::getenv("WINGMAN_REAL_PYTHON");const auto* inputPath=std::getenv("WINGMAN_REAL_INPUT");
            check(runtime&&inputPath,"real inference requires runtime and input environment selectors");
            WingmanProcessor real;real.prepareToPlay(44100,512);real.configureModel(runtime,modelsPath);
            check(real.importWav(4,juce::File(inputPath)).wasOk(),"seed independent Guitar lane before four-stem separation");const auto independentGuitar=real.trackSnapshot(4).clip;
            const auto convertedInput=directory.getChildFile("real-input.aiff");
            { juce::AudioFormatManager formats;formats.registerBasicFormats();
              std::unique_ptr<juce::AudioFormatReader> original(formats.createReaderFor(juce::File(inputPath)));
              check(original!=nullptr,"read real WAV fixture for AIFF conversion test");
              juce::AiffAudioFormat aiff;
              std::unique_ptr<juce::AudioFormatWriter> writer(aiff.createWriterFor(new juce::FileOutputStream(convertedInput),original->sampleRate,original->numChannels,24,{},0));
              check(writer&&writer->writeFromAudioReader(*original,0,original->lengthInSamples),"create real AIFF fixture"); }
            check(real.startModelJob("separate",convertedInput).wasOk(),"decode AIFF and run actual model through embedded plugin worker");
            for(int wait=0;wait<3000&&real.modelJobStatus().running;++wait)juce::Thread::sleep(100);
            check(real.trackSnapshot(4).clip==independentGuitar,"HTDemucs preserves independent Guitar lane");
            const auto done=real.modelJobStatus();
            check(done.outputDirectory.isDirectory(),"validated raw stems retained for delayed host file import");std::cout<<"Real model job: "<<done.message<<"\n";
            check(!done.running&&done.progress==1&&done.message.contains("Four stems imported"),"actual model inference imports four stems");
            juce::WavAudioFormat sourceFormat;std::unique_ptr<juce::AudioFormatReader> realInput(sourceFormat.createReaderFor(juce::File(inputPath).createInputStream().release(),true));
            check(realInput!=nullptr,"real reference WAV is readable");
            const auto expectedFrames=static_cast<int>(std::ceil(realInput->lengthInSamples*44100.0/realInput->sampleRate));
            std::array<std::shared_ptr<const wingman::Clip>,4> imported;
            const juce::StringArray stemNames{"vocals","other","bass","drums"};
            for(int track=0;track<4;++track){
                const auto clip=real.trackSnapshot(track).clip;imported[static_cast<std::size_t>(track)]=clip;
                check(clip&&clip->sampleRate==44100&&clip->audio.getNumSamples()==expectedFrames&&clip->audio.getNumChannels()==2,"actual stem synchronization");
                check(clip->audio.getMagnitude(0,expectedFrames)>1e-8f,"actual stem has nonzero signal");
                const auto exported=directory.getChildFile("actual-"+stemNames[track]+".wav");
                check(real.exportWav(exported,track).wasOk(),"export actual separated lane through plugin mixer");
                std::unique_ptr<juce::AudioFormatReader> checked(sourceFormat.createReaderFor(exported.createInputStream().release(),true));
                check(checked&&checked->sampleRate==48000&&checked->bitsPerSample==24&&checked->numChannels==2&&checked->lengthInSamples==static_cast<juce::int64>(std::ceil(expectedFrames*48000.0/44100)),"validate actual plugin WAV export");
                if(const auto* outputPath=std::getenv("WINGMAN_REAL_EXPORTS")) {
                    const juce::File output(outputPath);check(output.createDirectory().wasOk(),"create requested actual export directory");
                    const auto target=output.getChildFile(stemNames[track]+".wav");
                    check(!target.exists()&&exported.copyFileTo(target),"preserve actual plugin exports without overwriting files");
                }
            }
            auto temporary=juce::File::getSpecialLocation(juce::File::tempDirectory);
            const auto previous=temporary.findChildFiles(juce::File::findDirectories,false,"wingman-job-*");
            check(real.startModelJob("separate",juce::File(inputPath)).wasOk(),"start real model cancellation test");
            for(int wait=0;wait<1500&&real.modelJobStatus().running&&real.modelJobStatus().progress==0;++wait)juce::Thread::sleep(100);
            check(real.modelJobStatus().running&&real.modelJobStatus().progress>0,"cancel only after a genuine inference chunk completed");
            auto runningDirectories=temporary.findChildFiles(juce::File::findDirectories,false,"wingman-job-*");
            const auto cancelStart=juce::Time::getMillisecondCounterHiRes();real.cancelModelJob();
            for(int wait=0;wait<100&&real.modelJobStatus().running;++wait)juce::Thread::sleep(50);
            std::cout<<"Real cancellation latency ms: "<<juce::Time::getMillisecondCounterHiRes()-cancelStart<<"\n";
            check(!real.modelJobStatus().running&&real.modelJobStatus().message.contains("cancelled"),"cancel active model subprocess");
            for(int track=0;track<4;++track)check(real.trackSnapshot(track).clip==imported[static_cast<std::size_t>(track)],"cancel retains previously imported stems");
            for(const auto& path:runningDirectories)if(!previous.contains(path))check(!path.exists(),"cancel removes owned worker temporary files");
        } else std::cout<<"Real-inference integration not requested (WINGMAN_REAL_MODELS unset).\n";
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
        {const auto pulseFile=directory.getChildFile("tempo-auto.wav");juce::AudioBuffer<float> pulse(2,48000*5);pulse.clear();for(int beat=0;beat<9;++beat){const int at=static_cast<int>((0.1+beat*0.6)*48000);for(int i=0;i<400&&at+i<pulse.getNumSamples();++i)for(int ch=0;ch<2;++ch)pulse.setSample(ch,at+i,static_cast<float>(std::exp(-i/60.0)*std::sin(i*0.7)));}
         {juce::WavAudioFormat wav;std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(pulseFile),48000,2,24,{},0));check(writer&&writer->writeFromAudioSampleBuffer(pulse,0,pulse.getNumSamples()),"create automatic tempo fixture");}
         const double before=p.tempo();check(p.importWav(4,pulseFile).wasOk(),"tempo analysis automatically starts on import");
         for(int wait=0;wait<2000&&p.tempoAnalysisStatus().running;++wait)juce::Thread::sleep(5);
         const auto measured=p.tempoSnapshot();check(measured.valid&&std::abs(measured.bpm-100)<1&&p.tempo()==before,"analyzed BPM comes from audio and does not impersonate project default");check(p.applyDetectedTempo().wasOk()&&std::abs(p.tempo()-100)<1,"explicit apply uses measured tempo");}
        wingman::GuitarSettings guitar;check(p.generateGuitarPart(guitar).wasOk(),"start asynchronous guitar MIDI generation");
        for(int wait=0;wait<2000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);
        check(!p.guitarStatus().running&&p.selectedPart()==3&&!p.partNotes(3).empty(),"guitar MIDI generated and selected");
        check(p.generateGuitarPart(guitar,true,44100).wasOk(),"start background guitar audio render");
        for(int wait=0;wait<3000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);
        check(p.guitarStatus().progress==1&&p.trackSnapshot(4).clip&&p.trackSnapshot(4).clip->sampleRate==44100&&p.trackSnapshot(4).clip->audio.getMagnitude(0,p.trackSnapshot(4).clip->audio.getNumSamples())>0.0001,"rendered guitar is real audible 44.1k audio");
        const auto renderedClip=p.trackSnapshot(4).clip;const auto guitarWav=directory.getChildFile("guitar-44100.wav");check(p.exportWav(guitarWav,4,44100).wasOk(),"export guitar WAV at selected rate");
        {juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> r(format.createReaderFor(guitarWav.createInputStream().release(),true));check(r&&r->sampleRate==44100&&r->numChannels==2&&r->bitsPerSample==24,"rendered guitar export is stereo PCM24 at 44.1k");}
        guitar.bars=32;check(p.generateGuitarPart(guitar,true).wasOk(),"start cancellable guitar render");p.cancelGuitar();
        for(int wait=0;wait<1000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);
        check(!p.guitarStatus().running&&p.trackSnapshot(4).clip==renderedClip,"cancelled guitar rendering preserves existing audio");guitar.bars=4;
        const auto guitarNotes=p.partNotes(3);const auto guitarMidi=directory.getChildFile("guitar.mid");check(p.exportMidi(guitarMidi).wasOk(),"export guitar SMF");
        {juce::MidiFile file;auto stream=guitarMidi.createInputStream();check(stream&&file.readFrom(*stream)&&file.getTimeFormat()==960,"guitar MIDI file readable at 960 PPQ");auto seq=*file.getTrack(0);seq.updateMatchedPairs();int attacks=0;for(int e=0;e<seq.getNumEvents();++e){const auto* event=seq.getEventPointer(e);if(event->message.isNoteOn()){++attacks;check(event->noteOffObject!=nullptr&&event->message.getChannel()<=6,"guitar string channels and matched releases");}}check(attacks==static_cast<int>(guitarNotes.size()),"all guitar notes exported");}
        juce::MemoryBlock guitarState;p.getStateInformation(guitarState);restored.setStateInformation(guitarState.getData(),static_cast<int>(guitarState.getSize()));
        check(restored.selectedPart()==3&&restored.partNotes(3)==guitarNotes&&restored.trackSnapshot(4).clip&&restored.trackSnapshot(4).clip->audio.getNumSamples()==p.trackSnapshot(4).clip->audio.getNumSamples(),"guitar generated-note and rendered-audio state round trip");
        const auto sfzFixture=directory.getChildFile("guitar-fixture.sfz");
        check(sfzFixture.replaceWithText("<region> sample="+source.getFullPathName()+" lokey=40 hikey=88 pitch_keycenter=60 ampeg_release=0.1"),"write licensed-test-only synthetic sample mapping");
        check(p.loadGuitarLibrary(sfzFixture).wasOk(),"start async SFZ loading");
        for(int wait=0;wait<2000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);
        check(p.guitarEngineLabel().startsWith("Sampled Guitar"),"sampled indicator only after verified asset load");
        check(p.generateGuitarPart(guitar,true,48000).wasOk(),"render generated MIDI through loaded samples");
        for(int wait=0;wait<2000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);
        check(p.guitarStatus().progress==1&&p.trackSnapshot(4).clip->name=="Sampled guitar","sampled WAV publication");
        check(p.exportWav(directory.getChildFile("sampled-guitar.wav"),4,48000).wasOk(),"export sampled stereo PCM24 WAV");
        {juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatReader> r(format.createReaderFor(directory.getChildFile("sampled-guitar.wav").createInputStream().release(),true));check(r&&r->bitsPerSample==24&&r->numChannels==2&&r->sampleRate==48000,"sampled WAV readable, stereo, 24 bit at 48k");}
        juce::MemoryBlock sampledState;p.getStateInformation(sampledState);restored.setStateInformation(sampledState.getData(),static_cast<int>(sampledState.getSize()));
        check(restored.guitarEngineLabel().contains("Load and verify")&&restored.guitarSampleFile()==sfzFixture,"sample library state retained without decoding on host state callback");
        for(double liveRate:{44100.,48000.}){WingmanProcessor live;live.prepareToPlay(liveRate,512);check(live.loadGuitarLibrary(sfzFixture).wasOk(),"start live sample library load");for(int wait=0;wait<2000&&live.guitarStatus().running;++wait)juce::Thread::sleep(5);juce::MidiBuffer inputMidi;inputMidi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(100)),100);inputMidi.addEvent(juce::MidiMessage::noteOff(1,60),300);block.clear();live.processBlock(block,inputMidi);check(block.getMagnitude(0,100)==0&&block.getMagnitude(100,200)>.1f,"live sampled notes begin at MIDI sample offset at both rates");check(std::abs(block.getSample(0,250)-block.getSample(1,250))<.00001f,"live stereo sample channels preserved");for(int i=0;i<30;++i){inputMidi.clear();live.processBlock(block,inputMidi);}check(block.getMagnitude(0,512)==0,"live note release terminates without stuck voices");}
        {WingmanProcessor bankProbe;bankProbe.prepareToPlay(48000,512);for(int instrument:{0,1}){check(bankProbe.loadGuitarLibrary(sfzFixture,instrument).wasOk(),"load separate steel/nylon user library bank");for(int wait=0;wait<2000&&bankProbe.guitarStatus().running;++wait)juce::Thread::sleep(5);}bankProbe.selectGuitarInstrument(wingman::GuitarType::Nylon);check(bankProbe.guitarLibraryReport().contains("1 samples")&&bankProbe.guitarEngineLabel().startsWith("Sampled Guitar Ready"),"verified bank reports actual sample count");auto custom=bankProbe.guitarConfig();custom.capo=2;custom.maxFret=19;custom.tuning={38,45,50,55,59,64};custom.strumSpeed=.8;check(bankProbe.generateGuitarPart(custom).wasOk(),"generate alternate-tuning capo arrangement");for(int wait=0;wait<1000&&bankProbe.guitarStatus().running;++wait)juce::Thread::sleep(5);check(bankProbe.guitarStatus().progress==1,"custom guitar settings generated");bankProbe.setGuitarLayers(.5,.25,.4);juce::MemoryBlock state;bankProbe.getStateInformation(state);WingmanProcessor bankRestore;bankRestore.setStateInformation(state.getData(),static_cast<int>(state.getSize()));check(bankRestore.guitarConfig().fretNoise==.25&&bankRestore.guitarConfig().capo==2&&bankRestore.guitarConfig().maxFret==19&&bankRestore.guitarConfig().tuning==custom.tuning,"capo/fret/tuning restored");check(bankRestore.guitarSampleFile()==sfzFixture&&bankRestore.guitarEngineLabel().contains("Missing"),"library bank paths restore without unverified sample playback");bankProbe.selectGuitarInstrument(wingman::GuitarType::Steel);check(bankProbe.unloadGuitarLibrary().wasOk(),"unload a user library bank");bankProbe.selectGuitarInstrument(wingman::GuitarType::Nylon);check(bankProbe.guitarEngineLabel().startsWith("Sampled Guitar Ready"),"unloading steel preserves the verified nylon bank");}
        p.useSampledGuitar(false);

        check(p.setProgression("Dm(add9) Bbmaj7 Fmaj9 C/E | Am9 G7(b9) Cadd9 Fmaj7").wasOk(),"advanced cinematic progression accepts extensions altered dominants and slash bass");
        p.setVoiceLeading(true);p.generateMidi();const auto extendedMidi=directory.getChildFile("cinematic.mid");check(p.exportMidi(extendedMidi).wasOk(),"advanced voice-led chord MIDI exports");
        {juce::MidiFile file;auto stream=extendedMidi.createInputStream();check(stream&&file.readFrom(*stream),"advanced chord SMF readable");auto seq=*file.getTrack(0);seq.updateMatchedPairs();int notes=0;for(int e=0;e<seq.getNumEvents();++e){auto* event=seq.getEventPointer(e);if(event->message.isNoteOn()){++notes;check(event->noteOffObject&&event->message.getNoteNumber()>=36&&event->message.getNoteNumber()<=88,"voice-led extended chord notes have valid releases and ranges");}}check(notes>32,"extended chords retain additional tones");}
        // Instantiate and lay out the real editor, then rasterise it to exercise GUI paint paths.
        std::unique_ptr<juce::AudioProcessorEditor> editor(p.createEditor());
        check(editor->getWidth() == 1200 && editor->getHeight() == 760,"editor preserves saved size and lays out controls");
        for(const auto size:std::array<std::pair<int,int>,4>{{{960,660},{1200,760},{1920,1318},{960,1216}}}){
            editor->setSize(size.first,size.second);
            int dragControls=0;bool import=false,separate=false,progress=false,fallback=false;
            for(auto* child:editor->getChildren()){
                if(dynamic_cast<wingman::FileDragButton*>(child)){
                    ++dragControls;check(child->isVisible() && editor->getLocalBounds().contains(editor->getLocalArea(child,child->getLocalBounds())),"MIDI/stem drag controls visible at every editor size");
                }
                if(auto* button=dynamic_cast<juce::TextButton*>(child)){
                    import|=button->getButtonText()=="Import Audio";
                    separate|=button->getButtonText()=="Separate Stems";
                    fallback|=button->getButtonText()=="Export MIDI";
                }
                progress|=dynamic_cast<juce::ProgressBar*>(child)!=nullptr;
            }
            check(dragControls==6&&import&&separate&&progress&&fallback,"graphical splitter and MIDI fallback remain present");
        }
        editor->setSize(1200,840);
        auto image = editor->createComponentSnapshot(editor->getLocalBounds());
        check(image.isValid(),"editor paint renders");
        if (const auto* path = std::getenv("WINGMAN_SCREENSHOT")) { juce::PNGImageFormat png; juce::FileOutputStream out{juce::File{path}};
            check(out.openedOk() && out.setPosition(0) && out.truncate().wasOk() && png.writeImageToStream(image,out),"write current editor screenshot"); }
        for(int attempt=0;attempt<4;++attempt){auto closing=std::make_unique<WingmanProcessor>();check(closing->importWav(4,source).wasOk(),"start tempo producer for lifecycle test");check(closing->generateGuitarPart(wingman::GuitarSettings{},true).wasOk(),"start render producer for lifecycle test");const auto closingAt=juce::Time::getMillisecondCounterHiRes();closing.reset();check(juce::Time::getMillisecondCounterHiRes()-closingAt<5000,"active background producers cancel and join promptly before destruction");}
        auto activate=[&](const juce::String& name){for(auto* child:editor->getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()==name&&button->onClick){button->onClick();return true;}return false;};
        auto press=[](juce::Component& parent,const juce::String& name){for(auto* child:parent.getChildren())if(auto* button=dynamic_cast<juce::TextButton*>(child))if(button->getButtonText()==name&&button->onClick){button->onClick();return true;}return false;};
        for(const auto& name:juce::StringArray{"Guitar Studio","Chords & Ideas","Tempo & Analysis","Settings"}){
            check(activate(name),"main functional tab activation");check(editor->createComponentSnapshot(editor->getLocalBounds()).isValid(),"new workspace tab renders");
            for(auto* child:editor->getChildren())if(child->isVisible()&&(dynamic_cast<wingman::GuitarStudioPanel*>(child)||dynamic_cast<wingman::CinematicPanel*>(child)||dynamic_cast<wingman::TempoPanel*>(child)||dynamic_cast<wingman::SettingsPanel*>(child))){
                for(auto* control:child->getChildren())if(control->isVisible())check(child->getLocalBounds().contains(child->getLocalArea(control,control->getLocalBounds())),"panel controls stay within view bounds");

                if(auto* guitarUi=dynamic_cast<wingman::GuitarStudioPanel*>(child)){
                    check(press(*guitarUi,"Generate MIDI"),"guitar Generate button wired to engine");for(int wait=0;wait<2000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);check(p.selectedPart()==3&&!p.partNotes(3).empty(),"Guitar Studio UI generates actual notes");
                    check(press(*guitarUi,"Render WAV"),"guitar Render button wired to engine");for(int wait=0;wait<2000&&p.guitarStatus().running;++wait)juce::Thread::sleep(5);check(p.guitarStatus().progress==1&&p.trackSnapshot(4).clip->sampleRate==48000,"Guitar Studio UI renders actual 48k audio");
                }
                if(auto* ideasUi=dynamic_cast<wingman::CinematicPanel*>(child)){check(press(*ideasUi,"Generate idea")&&press(*ideasUi,"Apply to Project"),"cinematic ideas GUI generates and applies real chords");check(p.midiIsGenerated(),"cinematic GUI enables exported chord MIDI");}
                juce::Timer::callPendingTimersSynchronously();
                if(const auto* path=std::getenv("WINGMAN_SCREENSHOT")){juce::File target=juce::File(path).getSiblingFile(name.retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ")+".png");juce::FileOutputStream out(target);juce::PNGImageFormat png;check(out.openedOk()&&out.setPosition(0)&&out.truncate().wasOk()&&png.writeImageToStream(editor->createComponentSnapshot(editor->getLocalBounds()),out),"save actual workspace screenshot");}
            }
        }
        check(activate("Stems & Tracks"),"return to preserved mixer workspace");
        std::cout << checks << " integration checks passed\n";
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; directory.deleteRecursively(); return 1; }
    directory.deleteRecursively(); return 0;
}
