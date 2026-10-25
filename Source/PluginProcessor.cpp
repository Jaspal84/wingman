#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AI/ModelManager/ModelJob.h"
#include <limits>
#include "MIDI/MidiExport.h"
#include "Music/GuitarState.h"
#include "Music/GuitarRenderer.h"
#include "Music/SongGuitar.h"
#include "Files/DragFileStore.h"
namespace {
constexpr double maxClipSeconds = 120.0;
bool validTrack(int i) { return i >= 0 && i < 5; }
}
WingmanProcessor::WingmanProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)) {
    modelJob=std::make_unique<wingman::ModelJob>();
    dragCleanupThread.start([](wingman::CancellationToken){wingman::DragFileStore::cleanupExpired(wingman::DragFileStore::root(),juce::Time::getCurrentTime());});
    setProgression(chordText);
}
WingmanProcessor::~WingmanProcessor() {
    // Stop producers before consumers: model/guitar completion can enqueue tempo work.
    modelJob.reset();cancelGuitar();guitarWorker.join();cancelTempoAnalysis();tempoWorker.join();
    cancelAnalysis();if(analysisThread.joinable())analysisThread.join();
}
void WingmanProcessor::cancelAnalysis(){const juce::ScopedLock control(harmonicControl);pendingHarmony=-1;analysisThread.request_stop();}
juce::String WingmanProcessor::analysisMessage()const{const juce::ScopedLock lock(mutex);return analysisStatus;}
wingman::HarmonicResult WingmanProcessor::analysisSnapshot()const{const juce::ScopedLock lock(mutex);return analysisRevision==audioRevision.load()?analysis:wingman::HarmonicResult{};}
juce::Result WingmanProcessor::startAnalysis(int lane){
    if(!trackSnapshot(lane).clip)return juce::Result::fail("Import or record audio before harmonic analysis");
    const juce::ScopedLock control(harmonicControl);pendingHarmony=lane;
    if(analysisBusy.exchange(true))return juce::Result::ok();
    if(analysisThread.joinable())analysisThread.join();analysisProgress=0;
    analysisThread.start([this](wingman::CancellationToken token){for(;;){int lane;
        {const juce::ScopedLock control(harmonicControl);lane=pendingHarmony;pendingHarmony=-1;if(lane<0||token.stop_requested()){analysisBusy=false;return;}}
        auto clip=trackSnapshot(lane).clip;if(!clip)continue;const auto revision=audioRevision.load();
        {const juce::ScopedLock lock(mutex);analysisStatus="Analyzing imported key and chord timeline...";}analysisProgress=0;
        try{
            // Analyze the more energetic channel: anti-phase stereo must not cancel to silence.
            double energies[2]{};for(int c=0;c<2;++c)for(int i=0;i<clip->audio.getNumSamples();++i){double v=clip->audio.getSample(c,i);energies[c]+=v*v;}
            auto result=wingman::analyseHarmony(clip->audio.getReadPointer(energies[1]>energies[0]?1:0),clip->audio.getNumSamples(),clip->sampleRate,[&](double p){analysisProgress=p;return !token.stop_requested();});
            const juce::ScopedLock lock(mutex);if(token.stop_requested())throw std::runtime_error("Analysis cancelled");if(audioRevision.load()!=revision)throw std::runtime_error("Audio changed; analyzing latest queued import");
            analysis=std::move(result);++analysisGeneration;analysisRevision=revision;analysisProgress=1;
            const juce::StringArray keys{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
            analysisStatus=analysis.regions.empty()?"No reliable harmonic content found":"Detected "+keys[analysis.keyRoot]+(analysis.minor?" minor":" major")+" | key margin "+juce::String(analysis.keyConfidence*100,0)+"% | "+juce::String(static_cast<int>(analysis.regions.size()))+" chord regions. Apply explicitly; ambiguous/modulating music requires correction.";
        }catch(const std::exception& e){const juce::ScopedLock lock(mutex);analysisStatus=e.what();}
    }});return juce::Result::ok();
}
juce::Result WingmanProcessor::applyDetectedKey(){const juce::ScopedLock lock(mutex);auto result=analysisSnapshot();if(result.regions.empty())return juce::Result::fail("Analyze imported audio first");harmonyBaseKey=result.keyRoot;setKey(result.keyRoot);return juce::Result::ok();}
juce::Result WingmanProcessor::applyDetectedChords(){
    const juce::ScopedLock lock(mutex);auto result=analysisSnapshot();if(result.regions.empty())return juce::Result::fail("Analyze imported audio first");
    juce::StringArray tokens;for(const auto& r:result.regions){if(r.end-r.start<.1)continue;double remaining=(r.end-r.start)*bpm/60;while(remaining>.001){double beats=std::min(32.0,remaining);tokens.add(juce::String(wingman::chordName(r.chord))+":"+juce::String(beats,4));remaining-=beats;}if(tokens.size()>128)return juce::Result::fail("Detected timeline exceeds 128 editable chord tokens; use song-follow guitar instead");}
    if(tokens.isEmpty())return juce::Result::fail("No supported chord regions");auto applied=setProgression(tokens.joinIntoString(" "));if(applied.wasOk()){harmonyBaseKey=result.keyRoot;transpose=result.keyRoot;rebuildMidi();}return applied;
}
juce::Result WingmanProcessor::correctDetectedChord(int index,const juce::String& text){const juce::ScopedLock lock(mutex);auto chord=wingman::parseChord(text.toStdString());if(!chord||analysisRevision!=audioRevision.load()||index<0||index>=static_cast<int>(analysis.regions.size()))return juce::Result::fail("Select a current detected region and a valid chord");analysis.regions[static_cast<std::size_t>(index)].chord=*chord;++analysisGeneration;return juce::Result::ok();}
juce::Result WingmanProcessor::exportDetectedMidi(const juce::File& file){
    const auto result=analysisSnapshot();if(result.regions.empty())return juce::Result::fail("Analyze imported audio first");const double rate=tempo();juce::MidiMessageSequence events;
    for(const auto& region:result.regions)for(int pitch:region.chord.notes()){auto on=juce::MidiMessage::noteOn(1,pitch,static_cast<juce::uint8>(96));on.setTimeStamp(region.start*rate/60);events.addEvent(on);auto off=juce::MidiMessage::noteOff(1,pitch);off.setTimeStamp(region.end*rate/60);events.addEvent(off);}
    events.updateMatchedPairs();return wingman::writeMidi(file,events,rate,4,4,result.regions.back().end*rate/60,"Detected song chords");
}
bool WingmanProcessor::isBusesLayoutSupported(const BusesLayout& layout) const {
    const auto out = layout.getMainOutputChannelSet();
    const auto in = layout.getMainInputChannelSet();
    return (out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono())
        && (in.isDisabled() || in == out);
}
void WingmanProcessor::prepareToPlay(double rate, int) {
    const juce::ScopedLock lock(mutex);
    engineRate = rate;
    synth.prepare(rate); hostClock.reset();
    // Reserve once outside the audio callback. Recording is capped at two minutes.
    recordBuffer.setSize(2, static_cast<int>(rate * maxClipSeconds));
    recordSamples = 0; recording = false;recordEnabled=false; panic = true;
}
void WingmanProcessor::releaseResources() {
    const juce::ScopedLock lock(mutex);
    synth.allNotesOff();
    playing = recording = false;recordEnabled=false;trackPlayback.playing=false;
}
void WingmanProcessor::setHostSync(bool enabled) { const juce::ScopedLock lock(mutex);if(wrapperType==wrapperType_Standalone)enabled=false; if(syncEnabled==enabled)return;syncEnabled=enabled;syncRequested=enabled;if(!enabled)bpm=independentBpm.load();clockTempo=bpm;hostClock.reset();panic=true; }
bool WingmanProcessor::hostSync() const { const juce::ScopedLock lock(mutex); return syncEnabled; }
std::optional<wingman::HostFrame> WingmanProcessor::hostPosition() const { const juce::ScopedLock lock(mutex); return hostAvailable?std::optional{observedHost}:std::nullopt; }
void WingmanProcessor::flushOwnedNotes(juce::MidiBuffer& midi,int offset) {
    for(int channel=0;channel<16;++channel)for(int note=0;note<128;++note)if(ownedNotes[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)]){
        midi.addEvent(juce::MidiMessage::noteOff(channel+1,note),offset);ownedNotes[static_cast<std::size_t>(channel)][static_cast<std::size_t>(note)]=false;
    }
}
void WingmanProcessor::chaseMidi(double beat,int offset,juce::MidiBuffer& midi) {
    if(!generated||beat<0||sequenceLength<=0)return;
    const double phase=std::fmod(beat,sequenceLength);
    for(int i=0;i<sequence.getNumEvents();++i){const auto* event=sequence.getEventPointer(i);
        if(event->message.isNoteOn()&&event->noteOffObject&&event->message.getTimeStamp()<phase-1.0e-8&&event->noteOffObject->message.getTimeStamp()>phase){
            midi.addEvent(event->message,offset);ownedNotes[static_cast<std::size_t>(event->message.getChannel()-1)][static_cast<std::size_t>(event->message.getNoteNumber())]=true;
        }
    }
}
void WingmanProcessor::scheduleMidi(double startBeat,int offset,int count,juce::MidiBuffer& midi) {
    if(!generated||sequenceLength<=0||count<=0)return;
    const double endBeat=startBeat+count/engineRate*bpm/60;
    const auto firstLoop=static_cast<juce::int64>(std::floor(startBeat/sequenceLength));
    const auto lastLoop=static_cast<juce::int64>(std::floor(endBeat/sequenceLength));
    int events=0;
    for(auto loop=firstLoop;loop<=lastLoop && events<16384;++loop)for(int i=0;i<sequence.getNumEvents() && events<16384;++i){
        const auto& message=sequence.getEventPointer(i)->message;
        const double beat=loop*sequenceLength+message.getTimeStamp();
        if(beat>=0 && beat>=startBeat && beat<endBeat){
            const int at=offset+juce::jlimit(0,count-1,static_cast<int>(std::llround((beat-startBeat)*60/bpm*engineRate)));
            if(message.isNoteOff()&&!ownedNotes[static_cast<std::size_t>(message.getChannel()-1)][static_cast<std::size_t>(message.getNoteNumber())])continue;
            midi.addEvent(message,at);++events;
            if(message.isNoteOnOrOff())ownedNotes[static_cast<std::size_t>(message.getChannel()-1)][static_cast<std::size_t>(message.getNoteNumber())]=message.isNoteOn();
        }
    }
    if(events>=16384){flushOwnedNotes(midi,offset+count-1);recoverMidi=true;}
}
std::optional<wingman::HostFrame> WingmanProcessor::readHostTiming()const{
    auto* head=getPlayHead();if(!head)return {};auto p=head->getPosition();if(!p)return {};
    wingman::HostFrame frame;frame.bpm=p->getBpm().orFallback(clockTempo.load());
    auto ppq=p->getPpqPosition();auto secondsValue=p->getTimeInSeconds();
    if(!secondsValue)if(auto samples=p->getTimeInSamples())secondsValue=static_cast<double>(*samples)/engineRate;
    frame.seconds=secondsValue.orFallback(ppq?*ppq*60/frame.bpm:trackPlayback.position());
    frame.ppq=ppq.orFallback(frame.seconds*frame.bpm/60);frame.playing=p->getIsPlaying();
    if(auto signature=p->getTimeSignature()){frame.numerator=signature->numerator;frame.denominator=signature->denominator;}
    frame.looping=p->getIsLooping();if(auto loop=p->getLoopPoints()){frame.loopStart=loop->ppqStart;frame.loopEnd=loop->ppqEnd;}
    wingman::HostClock validation;if(!validation.resolve(frame,0,engineRate).valid)return {};return frame;
}
void WingmanProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    if(recordEnabled.load()){
        recordActive=true;if(recordEnabled.load()){
            const int n=std::min(audio.getNumSamples(),recordBuffer.getNumSamples()-recordSamples);const int inputs=std::min(getTotalNumInputChannels(),audio.getNumChannels());
            for(int c=0;c<2;++c)for(int i=0;i<n;++i){const float v=inputs?audio.getSample(std::min(c,inputs-1),i):0;recordBuffer.setSample(c,recordSamples+i,std::isfinite(v)?v:0);}recordSamples+=n;
        }recordActive=false;
    }
    const auto hostFrame=syncRequested.load()?readHostTiming():std::nullopt;
    if(syncRequested.load()){
        const bool hostPlaying=hostFrame&&hostFrame->playing;
        double start=0,end=0;if(hostFrame&&hostFrame->looping){start=hostFrame->seconds+(hostFrame->loopStart-hostFrame->ppq)*60/hostFrame->bpm;end=hostFrame->seconds+(hostFrame->loopEnd-hostFrame->ppq)*60/hostFrame->bpm;}
        trackPlayback.begin(hostFrame?hostFrame->seconds:trackPlayback.position(),hostPlaying,start,end);
    }else trackPlayback.begin();
    const juce::ScopedTryLock lock(mutex);
    if (!lock.isLocked()) { audio.clear();flushOwnedNotes(midi,0);synth.allNotesOff();recoverMidi=true;trackPlayback.render(audio,engineRate,&midi);return; }
    const int count=audio.getNumSamples();
    if(count==0)return;
    wingman::TransportBlock hostBlock;
    if(syncEnabled){
        hostBlock=hostClock.resolve(hostFrame,count,engineRate);hostAvailable=hostBlock.valid;
        if(hostAvailable){observedHost=hostBlock.frame;bpm=hostBlock.frame.bpm;clockTempo=bpm;seconds=hostBlock.frame.seconds;}
        if(playing!=hostBlock.playing||hostBlock.discontinuity)panic=true;
        playing=hostBlock.playing;
    } else hostAvailable=false;
    audio.clear();
    const bool recovered=recoverMidi.exchange(false);
    const bool resetNotes=panic||recovered;
    if(resetNotes){trackPlayback.resetSampleNotes=true;flushOwnedNotes(midi,0);synth.allNotesOff();panic=false;}
    const double loopLength=(!syncEnabled&&internalLoop)?audioLength():0;
    if(!syncEnabled){seconds=trackPlayback.position();playing=trackPlayback.playing.load();}
    if(playing){
        if(syncEnabled){
            if(resetNotes)chaseMidi(hostBlock.frame.ppq,0,midi);
            for(int i=0;i<hostBlock.spanCount;++i){const auto& span=hostBlock.spans[static_cast<std::size_t>(i)];if(span.restart){flushOwnedNotes(midi,span.offset);chaseMidi(span.beat,span.offset,midi);}scheduleMidi(span.beat,span.offset,span.count,midi);}
        } else if(loopLength>0){int offset=0;double at=seconds;int spans=0;while(offset<count&&spans++<128){const int length=std::min(count-offset,std::max(1,static_cast<int>(std::ceil((loopLength-at)*engineRate))));scheduleMidi(at*bpm/60,offset,length,midi);offset+=length;at+=length/engineRate;if(at>=loopLength){at=std::fmod(at,loopLength);if(offset<count){flushOwnedNotes(midi,offset);chaseMidi(at*bpm/60,offset,midi);}}}if(offset<count)flushOwnedNotes(midi,count-1);
        } else {if(resetNotes)chaseMidi(seconds*bpm/60,0,midi);scheduleMidi(seconds*bpm/60,0,count,midi);}
        seconds+=count/engineRate;
        if(loopLength>0)seconds=std::fmod(seconds,loopLength);
    }
    if(previewEnabled&&!sampledGuitar)synth.renderNextBlock(audio,midi,0,count);
    trackPlayback.render(audio,engineRate,&midi);
    for(int ch=0;ch<audio.getNumChannels();++ch)for(int i=0;i<count;++i){const float value=audio.getSample(ch,i);audio.setSample(ch,i,std::isfinite(value)?juce::jlimit(-1.0f,1.0f,value):0);}
}
void WingmanProcessor::processBlockBypassed(juce::AudioBuffer<float>& audio,juce::MidiBuffer& midi){
    flushOwnedNotes(midi,0);synth.allNotesOff();recoverMidi=true;
    for(int ch=getTotalNumInputChannels();ch<audio.getNumChannels();++ch)audio.clear(ch,0,audio.getNumSamples());
}
double WingmanProcessor::audioLength()const{double end=0;for(const auto& track:tracks)if(track.clip)end=std::max(end,track.clip->audio.getNumSamples()/track.clip->sampleRate);return end;}
void WingmanProcessor::seekSeconds(double value){const juce::ScopedLock lock(mutex);if(!syncEnabled&&std::isfinite(value)){seconds=std::clamp(value,0.0,std::max(audioLength(),sequenceLength*60/bpm));trackPlayback.seek(seconds);panic=true;}}
void WingmanProcessor::setLooping(bool enabled){const juce::ScopedLock lock(mutex);internalLoop=enabled;publishPlayback();panic=true;}
bool WingmanProcessor::looping()const{const juce::ScopedLock lock(mutex);return internalLoop;}
void WingmanProcessor::play() { const juce::ScopedLock lock(mutex);if(syncEnabled)return;seconds=trackPlayback.position();const auto end=audioLength();if(end>0&&seconds>=end){seconds=0;panic=true;}trackPlayback.seek(seconds);trackPlayback.playing=true;playing = true; }
void WingmanProcessor::stop() { const juce::ScopedLock lock(mutex);if(syncEnabled)return; playing = false; seconds = 0;trackPlayback.playing=false;trackPlayback.seek(0); panic = true; }
bool WingmanProcessor::isPlaying() const { const juce::ScopedLock lock(mutex); return trackPlayback.playing.load(); }
double WingmanProcessor::positionSeconds() const { const juce::ScopedLock lock(mutex); return trackPlayback.position(); }
void WingmanProcessor::setTempo(double value) { const juce::ScopedLock lock(mutex); if (!syncEnabled&&std::isfinite(value)) { bpm = juce::jlimit(40.0,240.0,value);independentBpm=bpm;clockTempo=bpm; panic = true; seconds = 0;trackPlayback.seek(0); } }
double WingmanProcessor::tempo() const { const juce::ScopedLock lock(mutex); return bpm; }
void WingmanProcessor::setKey(int value) { const juce::ScopedLock lock(mutex); if (transpose != juce::jlimit(0,11,value)) { for (auto& part : parts) part.clear(); guitarNotes.clear(); activePart = -1; } transpose = juce::jlimit(0,11,value); rebuildMidi(); }
int WingmanProcessor::key() const { const juce::ScopedLock lock(mutex); return transpose; }
void WingmanProcessor::setPreview(bool enabled) { const juce::ScopedLock lock(mutex); previewEnabled = enabled;publishPlayback(); panic = true; }
bool WingmanProcessor::preview() const { const juce::ScopedLock lock(mutex); return previewEnabled; }
bool WingmanProcessor::midiIsGenerated() const { const juce::ScopedLock lock(mutex); return generated; }
wingman::Track WingmanProcessor::trackSnapshot(int i) const { const juce::ScopedLock lock(mutex); return validTrack(i) ? tracks[static_cast<std::size_t>(i)] : wingman::Track{}; }
void WingmanProcessor::setTrackMix(int i, float gain, bool mute, bool solo) {
    if (!validTrack(i)) return;
    const juce::ScopedLock lock(mutex);
    auto& t = tracks[static_cast<std::size_t>(i)];
    t.gain = std::isfinite(gain) ? juce::jlimit(0.0f,1.0f,gain) : 0; t.mute = mute; t.solo = solo;publishPlayback();
}
void WingmanProcessor::setTrackPan(int i, float pan) {
    if (!validTrack(i) || !std::isfinite(pan)) return;
    const juce::ScopedLock lock(mutex); tracks[static_cast<std::size_t>(i)].pan = juce::jlimit(-1.0f,1.0f,pan);publishPlayback();
}
juce::String WingmanProcessor::sessionName()const{const juce::ScopedLock lock(mutex);return projectTitle;}
void WingmanProcessor::setSessionName(const juce::String& value){const juce::ScopedLock lock(mutex);projectTitle=value.trim().substring(0,120);}
void WingmanProcessor::setEditorSize(int width, int height) {
    const juce::ScopedLock lock(mutex); editorWidth = juce::jlimit(960,1920,width); editorHeight = juce::jlimit(608,1318,height);
}
std::pair<int,int> WingmanProcessor::editorSize() const { const juce::ScopedLock lock(mutex); return {editorWidth,editorHeight}; }
juce::Result WingmanProcessor::importWav(int i, const juce::File& file) {
    if (!validTrack(i)) return juce::Result::fail("Invalid track");
    if(!file.existsAsFile())return juce::Result::fail("Cannot open audio file");
    juce::AudioFormatManager formats;formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader)return juce::Result::fail("Unsupported or unreadable audio file");
    if (reader->sampleRate < 8000 || reader->sampleRate > 192000 || reader->lengthInSamples <= 0
        || reader->lengthInSamples > reader->sampleRate * maxClipSeconds || reader->numChannels < 1 || reader->numChannels > 2)
        return juce::Result::fail("Audio must be mono/stereo, 8-192 kHz and at most 120 seconds");
    auto clip = std::make_shared<wingman::Clip>();
    clip->sampleRate = reader->sampleRate; clip->name = file.getFileName();
    clip->audio.setSize(2, static_cast<int>(reader->lengthInSamples));
    if (!reader->read(&clip->audio, 0, clip->audio.getNumSamples(), 0, true, true)) return juce::Result::fail("Audio decoding failed");
    for (int ch = 0; ch < 2; ++ch) for (int sample = 0; sample < clip->audio.getNumSamples(); ++sample)
        if (!std::isfinite(clip->audio.getSample(ch,sample))) return juce::Result::fail("Audio contains nonfinite samples");
    clip->buildWaveform();
    {const juce::ScopedLock lock(mutex);
    tracks[static_cast<std::size_t>(i)].clip = std::move(clip);
    if(!syncEnabled){seconds=0;trackPlayback.seek(0);panic=true;}
    publishPlayback();
    ++audioRevision; analysis={};
    }
    startTempoAnalysis(i);startAnalysis(i);
    return juce::Result::ok();
}
juce::String WingmanProcessor::detectTempo(int i) {
    const auto result=startTempoAnalysis(i);return result.wasOk()?"Tempo analysis queued; apply a valid result in Tempo & Analysis":result.getErrorMessage();
}
juce::Result WingmanProcessor::setProgression(const juce::String& text) {
    const auto tokens = juce::StringArray::fromTokens(text, " ,|\t\n", "");
    std::vector<wingman::Chord> parsed;
    for (const auto& token : tokens) {
        if (token.isEmpty()) continue;
        auto chord = wingman::parseChord(token.toStdString());
        if (!chord) return juce::Result::fail("Use valid chords (Am9, Fmaj7, Cadd9, G7(b9), C/E); optional ^inversion:beats");
        parsed.push_back(*chord);
    }
    if (parsed.empty() || parsed.size() > 128) return juce::Result::fail("Enter 1-128 valid chord tokens");
    const juce::ScopedLock lock(mutex);
    if (chordText != text.trim()) { for (auto& part : parts) part.clear(); activePart = -1; }
    harmonyBaseKey=0;
    chords = std::move(parsed); chordText = text.trim(); rebuildMidi();
    return juce::Result::ok();
}
juce::String WingmanProcessor::progression() const { const juce::ScopedLock lock(mutex); return chordText; }
void WingmanProcessor::rebuildMidi() {
    if(activePart==3){sequence=wingman::patternSequence(guitarNotes);sequenceLength=guitarSettings.bars*guitarSettings.numerator*4.0/guitarSettings.denominator;}
    else if (activePart >= 0) { sequence = wingman::patternSequence(parts[static_cast<std::size_t>(activePart)]); sequenceLength = settings[static_cast<std::size_t>(activePart)].bars*4.0; }
    else { sequence = wingman::makeChords(chords, effectiveKey(),voiceLeading); sequenceLength = wingman::progressionBeats(chords); }
    publishPlayback();panic = true; seconds = 0;
}
void WingmanProcessor::selectPart(int part) { const juce::ScopedLock lock(mutex); activePart = juce::jlimit(-1,3,part); generated = activePart < 0 || (activePart==3?!guitarNotes.empty():!parts[static_cast<std::size_t>(activePart)].empty()); rebuildMidi(); }
int WingmanProcessor::selectedPart() const { const juce::ScopedLock lock(mutex); return activePart; }
wingman::PatternSettings WingmanProcessor::partSettings(int part) const { const juce::ScopedLock lock(mutex); if(part==3){wingman::PatternSettings s;s.bars=guitarSettings.bars;s.lowest=40;s.highest=88;return s;}return part >= 0 && part < 3 ? settings[static_cast<std::size_t>(part)] : wingman::PatternSettings{}; }
std::vector<wingman::NoteEvent> WingmanProcessor::partNotes(int part) const { const juce::ScopedLock lock(mutex); return part==3?guitarNotes:part >= 0 && part < 3 ? parts[static_cast<std::size_t>(part)] : std::vector<wingman::NoteEvent>{}; }
juce::Result WingmanProcessor::generatePart(int part,const wingman::PatternSettings& config) {
    if (part < 0 || part > 2) return juce::Result::fail("Invalid part");
    std::vector<wingman::Chord> harmony; int keyCopy; juce::String text;
    { const juce::ScopedLock lock(mutex); harmony = chords; keyCopy = effectiveKey(); text = chordText; }
    try {
        auto notes = wingman::generatePattern(static_cast<wingman::PatternKind>(part),harmony,keyCopy,config);
        const juce::ScopedLock lock(mutex);
        if (text != chordText || keyCopy != effectiveKey()) return juce::Result::fail("Harmony changed during generation; retry");
        settings[static_cast<std::size_t>(part)] = config; parts[static_cast<std::size_t>(part)] = std::move(notes);
        activePart = part; generated = true; rebuildMidi(); return juce::Result::ok();
    } catch (const std::exception& error) { return juce::Result::fail(error.what()); }
}
juce::Result WingmanProcessor::varyPart(int part,int firstBar,int barCount) {
    if(part<0||part>2) return juce::Result::fail("Invalid part");
    const juce::ScopedLock lock(mutex);
    auto& original=parts[static_cast<std::size_t>(part)]; auto config=settings[static_cast<std::size_t>(part)];
    if(original.empty()||firstBar<0||barCount<1||firstBar+barCount>config.bars) return juce::Result::fail("Generate a part and choose a valid phrase first");
    try {
        ++config.variation; auto replacement=wingman::generatePattern(static_cast<wingman::PatternKind>(part),chords,effectiveKey(),config);
        const double begin=firstBar*4.0,end=(firstBar+barCount)*4.0;
        std::erase_if(original,[&](const auto& note){return note.beat>=begin && note.beat<end;});
        for(const auto& note:replacement) if(note.beat>=begin && note.beat<end)original.push_back(note);
        std::stable_sort(original.begin(),original.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
        settings[static_cast<std::size_t>(part)]=config; activePart=part; rebuildMidi(); return juce::Result::ok();
    } catch(const std::exception& error){return juce::Result::fail(error.what());}
}
juce::Result WingmanProcessor::toggleDrumStep(int bar,int instrument,int step) {
    const juce::ScopedLock lock(mutex);
    if(bar<0||bar>=settings[1].bars||instrument<0||instrument>8||step<0||step>15)return juce::Result::fail("Invalid drum step");
    const double beat=bar*4+step*0.25; const int note=settings[1].drumMap[static_cast<std::size_t>(instrument)];
    auto& events=parts[1]; auto found=std::find_if(events.begin(),events.end(),[&](const auto& e){return e.note==note&&std::abs(e.beat-beat)<0.12;});
    if(found!=events.end())events.erase(found); else events.push_back({beat,0.08,note,settings[1].velocity,10});
    std::stable_sort(events.begin(),events.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
    activePart=1; generated=true; rebuildMidi(); return juce::Result::ok();
}
juce::Result WingmanProcessor::editPartNote(int part,int index,const wingman::NoteEvent& note,bool remove) {
    const juce::ScopedLock lock(mutex);
    if (part < 0 || part > 2) return juce::Result::fail("Select a generated part");
    auto& notes = parts[static_cast<std::size_t>(part)];
    if (index < 0 || index >= static_cast<int>(notes.size())) return juce::Result::fail("Invalid note");
    if (remove) notes.erase(notes.begin()+index);
    else {
        if (!std::isfinite(note.beat) || !std::isfinite(note.duration) || note.beat < 0 || note.duration <= 0
            || note.beat+note.duration > settings[static_cast<std::size_t>(part)].bars*4 || note.note < 0 || note.note > 127 || note.velocity < 1 || note.velocity > 127 || note.channel < 1 || note.channel > 16)
            return juce::Result::fail("Invalid MIDI note or timing");
        if (part != 1) for (int i=0;i<static_cast<int>(notes.size());++i) if(i!=index && note.beat < notes[static_cast<std::size_t>(i)].beat+notes[static_cast<std::size_t>(i)].duration
            && note.beat+note.duration > notes[static_cast<std::size_t>(i)].beat) return juce::Result::fail("Overlapping monophonic notes");
        notes[static_cast<std::size_t>(index)] = note;
    }
    std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.beat<b.beat;}); rebuildMidi(); return juce::Result::ok();
}
void WingmanProcessor::generateMidi() { const juce::ScopedLock lock(mutex); generated = true; rebuildMidi(); }
juce::Result WingmanProcessor::beginRecording(int i) {
    const juce::ScopedLock lock(mutex);
    if (!validTrack(i) || recordBuffer.getNumSamples() == 0) return juce::Result::fail("Audio device must be running");
    if (recording) return juce::Result::fail("Stop the current recording first");
    recordSamples = 0; recordTrack = i; recording = true;recordEnabled=true;
    return juce::Result::ok();
}
bool WingmanProcessor::isRecording() const { const juce::ScopedLock lock(mutex); return recording; }
juce::Result WingmanProcessor::finishRecording() {
    const juce::ScopedLock lock(mutex);
    if (!recording) return juce::Result::fail("No recording in progress");
    recording = false;recordEnabled=false;while(recordActive.load())std::this_thread::yield();
    if (!recordSamples) return juce::Result::fail("No input samples captured");
    auto clip = std::make_shared<wingman::Clip>();
    clip->audio.setSize(2, recordSamples);
    for (int ch = 0; ch < 2; ++ch) clip->audio.copyFrom(ch,0,recordBuffer,ch,0,recordSamples);
    clip->sampleRate = engineRate; clip->name = "Recorded input";
    clip->buildWaveform();
    tracks[static_cast<std::size_t>(recordTrack)].clip = std::move(clip);publishPlayback();
    ++audioRevision; analysis={};startTempoAnalysis(recordTrack);startAnalysis(recordTrack);
    return juce::Result::ok();
}
juce::Result WingmanProcessor::exportMidi(const juce::File& file) {
    juce::MidiMessageSequence events;double tempoCopy,length;int numerator=4,denominator=4;juce::String name;
    {const juce::ScopedLock lock(mutex);if(!generated)return juce::Result::fail("Generate MIDI first");events=sequence;tempoCopy=bpm;length=sequenceLength;
     const juce::StringArray labels{"Chords","Bass","Drums","Melody","Guitar"};name="JASWINGMAN "+labels[activePart+1];
     if(activePart==3){numerator=guitarSettings.numerator;denominator=guitarSettings.denominator;}}
    return wingman::writeMidi(file,events,tempoCopy,numerator,denominator,length,name);
}
juce::Result WingmanProcessor::exportWav(const juce::File& file, int track,double outputRate) {
    if(outputRate!=44100&&outputRate!=48000)return juce::Result::fail("Export rate must be 44.1 or 48 kHz");
    std::array<wingman::Track,5> snapshot;
    { const juce::ScopedLock lock(mutex); snapshot = tracks; }
    if (track >= 0) {
        if (!validTrack(track)) return juce::Result::fail("Invalid track");
        for (int i = 0; i < 5; ++i) { auto& t = snapshot[static_cast<std::size_t>(i)]; t.solo = false; t.mute = i != track; }
    }
    double duration = 0; bool solo = false;
    for (const auto& t : snapshot) solo |= t.solo;
    for (const auto& t : snapshot) if (t.clip && !t.mute && (!solo || t.solo)) duration = std::max(duration, t.clip->audio.getNumSamples()/t.clip->sampleRate);
    if (duration <= 0) return juce::Result::fail("No audible audio tracks to export");
    juce::TemporaryFile temporary(file);
    {
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer(wav.createWriterFor(new juce::FileOutputStream(temporary.getFile()),outputRate,2,24,{},0));
        if (!writer) return juce::Result::fail("Cannot write WAV");
        juce::AudioBuffer<float> block(2,1024);
        const auto total = static_cast<juce::int64>(std::ceil(duration * outputRate));
        for (juce::int64 pos = 0; pos < total; pos += 1024) {
            const int n = static_cast<int>(std::min<juce::int64>(1024,total-pos)); block.clear();
            for (const auto& t : snapshot) if (t.clip && !t.mute && (!solo || t.solo))
                for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i)
                    block.addSample(ch,i,wingman::interpolate(*t.clip,ch,(pos+i)*t.clip->sampleRate/outputRate)*t.gain*wingman::balanceGain(t.pan,ch));
            // Saturate PCM mixdown instead of wrapping on overload.
            for (int ch = 0; ch < 2; ++ch) for (int i = 0; i < n; ++i) block.setSample(ch,i,juce::jlimit(-1.0f,1.0f,block.getSample(ch,i)));
            if (!writer->writeFromAudioSampleBuffer(block,0,n)) return juce::Result::fail("WAV write failed");
        }
        if (!writer->flush()) return juce::Result::fail("WAV flush failed");
    }
    return temporary.overwriteTargetFileWithTemporary() ? juce::Result::ok() : juce::Result::fail("Cannot replace destination");
}
void WingmanProcessor::getStateInformation(juce::MemoryBlock& destination) {
    juce::ValueTree state("Wingman");
    const juce::ScopedLock lock(mutex);
    state.setProperty("voiceLeading",voiceLeading,nullptr);
    state.setProperty("projectTitle",projectTitle,nullptr); state.setProperty("inferencePython",inferencePython,nullptr); state.setProperty("inferenceModels",inferenceModels,nullptr);
    state.setProperty("width",editorWidth,nullptr); state.setProperty("height",editorHeight,nullptr);
    state.setProperty("hostSync",syncEnabled,nullptr); state.setProperty("version",2,nullptr); state.setProperty("bpm",independentBpm.load(),nullptr); state.setProperty("key",transpose,nullptr);state.setProperty("harmonyBaseKey",harmonyBaseKey,nullptr);
    state.setProperty("chords",chordText,nullptr); state.setProperty("generated",generated,nullptr); state.setProperty("preview",previewEnabled,nullptr);
    for (const auto& t : tracks) {
        juce::ValueTree node("Track");
        node.setProperty("pan",t.pan,nullptr); node.setProperty("gain",t.gain,nullptr); node.setProperty("mute",t.mute,nullptr); node.setProperty("solo",t.solo,nullptr);
        if (t.clip) {
            node.setProperty("name",t.clip->name,nullptr); node.setProperty("rate",t.clip->sampleRate,nullptr);
            node.setProperty("samples",t.clip->audio.getNumSamples(),nullptr);
            for (int ch = 0; ch < 2; ++ch) {
                juce::MemoryBlock bytes(t.clip->audio.getReadPointer(ch),static_cast<std::size_t>(t.clip->audio.getNumSamples())*sizeof(float));
                node.setProperty(ch == 0 ? "left" : "right",juce::var(bytes),nullptr);
            }
        }
        state.addChild(node,-1,nullptr);
    }
    juce::ValueTree patterns("Parts");
    for (int i=0;i<3;++i) {
        auto node = wingman::saveSettings(settings[static_cast<std::size_t>(i)]);
        for (const auto& note : parts[static_cast<std::size_t>(i)]) {
            juce::ValueTree event("Note"); event.setProperty("beat",note.beat,nullptr); event.setProperty("duration",note.duration,nullptr);
            event.setProperty("note",note.note,nullptr); event.setProperty("velocity",note.velocity,nullptr); event.setProperty("channel",note.channel,nullptr); node.addChild(event,-1,nullptr);
        }
        patterns.addChild(node,-1,nullptr);
    }
    state.setProperty("audioLoop",internalLoop,nullptr);
    state.setProperty("guitarSamplePath",guitarLibraryPath,nullptr);
    state.setProperty("guitarSampleEngine",sampledGuitar,nullptr);
    state.setProperty("guitarVelocityCurve",guitarVelocityCurve,nullptr);
    state.setProperty("guitar",wingman::saveGuitar(guitarSettings,guitarNotes),nullptr);
    juce::MemoryOutputStream patternStream; patterns.writeToStream(patternStream);
    state.setProperty("patterns",juce::var(patternStream.getMemoryBlock()),nullptr); state.setProperty("activePart",activePart,nullptr);
    juce::MemoryOutputStream stream(destination,false); state.writeToStream(stream);
}
void WingmanProcessor::setStateInformation(const void* data, int size) {
    auto state = juce::ValueTree::readFromData(data,static_cast<std::size_t>(std::max(0,size)));
    const int version=state["version"];
    if(!state.hasType("Wingman") || !((version==1&&state.getNumChildren()==4)||(version==2&&state.getNumChildren()==5)))return;
    cancelGuitar();cancelTempoAnalysis();
    std::array<wingman::Track,5> restored;
    for (int i = 0; i < state.getNumChildren(); ++i) {
        auto node = state.getChild(i); auto& t = restored[static_cast<std::size_t>(i)];
        float gain = node["gain"]; if (!std::isfinite(gain)) return;
        const float pan = node.getProperty("pan",0.0); if (!std::isfinite(pan)) return;
        t.pan = juce::jlimit(-1.0f,1.0f,pan); t.gain = juce::jlimit(0.0f,1.0f,gain); t.mute = node["mute"]; t.solo = node["solo"];
        if (node.hasProperty("samples")) {
            const int n = node["samples"]; const double rate = node["rate"];
            if (!std::isfinite(rate) || rate < 8000 || rate > 192000 || n <= 0 || n > rate*maxClipSeconds) return;
            auto clip = std::make_shared<wingman::Clip>(); clip->sampleRate = rate; clip->name = node["name"].toString(); clip->audio.setSize(2,n);
            for (int ch = 0; ch < 2; ++ch) {
                const auto* bytes = node[ch == 0 ? "left" : "right"].getBinaryData();
                if (!bytes || bytes->getSize() != static_cast<std::size_t>(n)*sizeof(float)) return;
                std::memcpy(clip->audio.getWritePointer(ch), bytes->getData(), bytes->getSize());
                for (int s = 0; s < n; ++s) if (!std::isfinite(clip->audio.getSample(ch,s))) return;
            }
            clip->buildWaveform();t.clip = std::move(clip);
        }
    }
    const double restoredBpm = state["bpm"];
    if (!std::isfinite(restoredBpm)) return;
    const juce::ScopedLock lock(mutex);
    if (setProgression(state["chords"].toString()).failed()) return;
    setEditorSize(state.getProperty("width",1200),state.getProperty("height",760));
    projectTitle=state.getProperty("projectTitle","Untitled session").toString().substring(0,120);
    inferencePython=state.getProperty("inferencePython","").toString(); inferenceModels=state.getProperty("inferenceModels","").toString();
    ++audioRevision; analysis={};
    tempoResult={};tempoClip.reset();tempoTrack=-1;tempoMessage="No analyzed tempo available; re-analyse loaded audio";
    tracks = std::move(restored); bpm = juce::jlimit(40.0,240.0,restoredBpm);independentBpm=bpm;clockTempo=bpm; transpose = juce::jlimit(0,11,static_cast<int>(state["key"]));
    harmonyBaseKey=juce::jlimit(0,11,static_cast<int>(state.getProperty("harmonyBaseKey",0)));
    activePart = -1; for (auto& part : parts) part.clear();
    if (const auto* bytes = state["patterns"].getBinaryData()) {
        auto patterns = juce::ValueTree::readFromData(bytes->getData(),bytes->getSize());
        if (patterns.hasType("Parts") && patterns.getNumChildren()==3) try {
            for(int i=0;i<3;++i) {
                auto node=patterns.getChild(i); auto& events=parts[static_cast<std::size_t>(i)];
                settings[static_cast<std::size_t>(i)]=wingman::loadSettings(node);
                if(node.getNumChildren()>8192) throw std::invalid_argument("Too many notes");
                for(auto event : node) {
                    wingman::NoteEvent note{static_cast<double>(event["beat"]),static_cast<double>(event["duration"]),static_cast<int>(event["note"]),static_cast<int>(event["velocity"]),static_cast<int>(event["channel"])};
                    if(!std::isfinite(note.beat)||!std::isfinite(note.duration)||note.beat<0||note.duration<=0||note.beat+note.duration>settings[static_cast<std::size_t>(i)].bars*4
                        ||note.note<0||note.note>127||note.velocity<1||note.velocity>127||note.channel<1||note.channel>16) throw std::invalid_argument("Invalid saved note");
                    events.push_back(note);
                }
            }
            activePart=juce::jlimit(-1,2,static_cast<int>(state.getProperty("activePart",-1)));
        } catch(...) { for(auto& part : parts)part.clear(); activePart=-1; }
    }
    internalLoop=state.getProperty("audioLoop",false);publishPlayback();trackPlayback.playing=false;trackPlayback.seek(0);
    guitarLibraryPath=state.getProperty("guitarSamplePath","").toString();sampledGuitar=state.getProperty("guitarSampleEngine",false);guitarVelocityCurve=std::clamp(static_cast<double>(state.getProperty("guitarVelocityCurve",1.0)),.25,4.0);if(!std::isfinite(guitarVelocityCurve))guitarVelocityCurve=1;guitarLibrary.reset();++libraryRevision;
    try{wingman::loadGuitar(state["guitar"],guitarSettings,guitarNotes);if(static_cast<int>(state.getProperty("activePart",-1))==3&&!guitarNotes.empty())activePart=3;}catch(...){guitarSettings={};guitarNotes.clear();}
    voiceLeading=state.getProperty("voiceLeading",false);
    syncEnabled=state.getProperty("hostSync",false);syncRequested=syncEnabled; hostClock.reset();
    generated = state["generated"]; previewEnabled = state["preview"]; playing = recording = false;recordEnabled=false;trackPlayback.playing=false; rebuildMidi();
}
juce::AudioProcessorEditor* WingmanProcessor::createEditor() { return new WingmanEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new WingmanProcessor; }

void WingmanProcessor::configureModel(const juce::String& python,const juce::String& models){const juce::ScopedLock lock(mutex);inferencePython=python;inferenceModels=models;}
std::pair<juce::String,juce::String> WingmanProcessor::modelConfig()const{const juce::ScopedLock lock(mutex);return {inferencePython,inferenceModels};}
wingman::ModelJobInfo WingmanProcessor::modelJobStatus()const{return modelJob->snapshot();}
void WingmanProcessor::cancelModelJob(){modelJob->cancel();}
juce::Result WingmanProcessor::startModelJob(const juce::String& action,const juce::File& input,bool consent){
    if(action=="install"&&!consent)return juce::Result::fail("Explicit model download/license consent is required");
    const auto config=modelConfig();
    if(config.first.isEmpty()||config.second.isEmpty())return juce::Result::fail("Configure Python and model directory first");
    return modelJob->start(juce::File(config.first),juce::File(config.second),action,input,[this](const juce::File& output){
        return importStemSet({output.getChildFile("vocals.wav"),output.getChildFile("other.wav"),output.getChildFile("bass.wav"),output.getChildFile("drums.wav")});
    });
}
juce::Result WingmanProcessor::importStemSet(const std::array<juce::File,4>& files){
    std::array<std::shared_ptr<const wingman::Clip>,4> loaded;
    for(int i=0;i<4;++i){
        juce::WavAudioFormat format;auto input=files[static_cast<std::size_t>(i)].createInputStream();
        if(!input)return juce::Result::fail("A separated stem is missing");
        std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(input.release(),true));
        if(!reader||reader->numChannels!=2||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<=0||reader->lengthInSamples>reader->sampleRate*120)return juce::Result::fail("Invalid separated stem format/length");
        if(i && (reader->sampleRate!=loaded[0]->sampleRate||reader->lengthInSamples!=loaded[0]->audio.getNumSamples()))return juce::Result::fail("Separated stems are not synchronized");
        auto clip=std::make_shared<wingman::Clip>();clip->sampleRate=reader->sampleRate;clip->name=files[static_cast<std::size_t>(i)].getFileName();clip->audio.setSize(2,static_cast<int>(reader->lengthInSamples));
        if(!reader->read(&clip->audio,0,clip->audio.getNumSamples(),0,true,true))return juce::Result::fail("Cannot decode separated stem");
        for(int ch=0;ch<2;++ch)for(int s=0;s<clip->audio.getNumSamples();++s)if(!std::isfinite(clip->audio.getSample(ch,s)))return juce::Result::fail("Nonfinite separated stem");
        clip->buildWaveform();loaded[static_cast<std::size_t>(i)]=std::move(clip);
    }
    const juce::ScopedLock lock(mutex);for(int i=0;i<4;++i)tracks[static_cast<std::size_t>(i)].clip.swap(loaded[static_cast<std::size_t>(i)]);
    publishPlayback();
    seconds=0;panic=true;++audioRevision;analysis={};startTempoAnalysis(3);startAnalysis(1);trackPlayback.seek(0);return juce::Result::ok();
}

void WingmanProcessor::prepareTrackDrag(int track){if(validTrack(track))audioDragCache.prepare(track,trackSnapshot(track).clip);}
juce::Result WingmanProcessor::trackDragFile(int track,juce::File& file){
    if(!validTrack(track))return juce::Result::fail("Invalid audio lane");
    prepareTrackDrag(track);return audioDragCache.get(track,trackSnapshot(track).clip,file);
}

wingman::GuitarSettings WingmanProcessor::guitarConfig()const{const juce::ScopedLock lock(mutex);return guitarSettings;}
wingman::ModelJobInfo WingmanProcessor::guitarStatus()const{const juce::ScopedLock lock(mutex);return {guitarBusy.load(),guitarProgress.load(),guitarMessage,{}};}
void WingmanProcessor::cancelGuitar(){guitarWorker.request_stop();}
double WingmanProcessor::guitarSampleVelocity()const{const juce::ScopedLock lock(mutex);return guitarVelocityCurve;}
juce::File WingmanProcessor::guitarSampleFile()const{const juce::ScopedLock lock(mutex);return !juce::File::isAbsolutePath(guitarLibraryPath)?juce::File{}:juce::File(guitarLibraryPath);}
void WingmanProcessor::useSampledGuitar(bool enabled,double curve){const juce::ScopedLock lock(mutex);sampledGuitar=enabled;guitarVelocityCurve=std::isfinite(curve)?std::clamp(curve,.25,4.0):1;publishPlayback();}
juce::String WingmanProcessor::guitarEngineLabel()const{const juce::ScopedLock lock(mutex);return sampledGuitar?(guitarLibrary?"Sampled Guitar | verified SFZ"+juce::String(trackPlayback.unmappedSamples.load()>0?" | unmapped live notes":""):"Sampled selected | Load and verify SFZ first"):"Basic Synthesis";}
juce::Result WingmanProcessor::loadGuitarLibrary(const juce::File& file){
    if(guitarBusy.exchange(true))return juce::Result::fail("Wait for current guitar job");guitarProgress=0;
    {const juce::ScopedLock lock(mutex);guitarMessage="Loading and verifying SFZ samples...";}
    guitarWorker.start([this,file](wingman::CancellationToken token){try{
        auto library=std::make_shared<wingman::SampledGuitar>(wingman::SampledGuitar::load(file,[&](double p){guitarProgress=p;return !token.stop_requested();}));
        const juce::ScopedLock lock(mutex);if(token.stop_requested())throw std::runtime_error("Sample loading cancelled");guitarLibrary=std::move(library);++libraryRevision;guitarLibraryPath=file.getFullPathName();sampledGuitar=true;publishPlayback();guitarProgress=1;guitarMessage="SFZ verified | "+juce::String(static_cast<double>(guitarLibrary->bytes)/1048576,1)+" MiB | user must hold sample rights";
    }catch(const std::exception& e){const juce::ScopedLock lock(mutex);guitarMessage=e.what();}guitarBusy=false;});return juce::Result::ok();
}
juce::Result WingmanProcessor::generateGuitarPart(const wingman::GuitarSettings& config,bool renderAudio,double sampleRate,bool followSong){
    const auto song=followSong?analysisSnapshot():wingman::HarmonicResult{};const auto songRevision=audioRevision.load();const auto songGeneration=analysisGeneration.load();if(followSong&&song.regions.empty())return juce::Result::fail("Analyze imported audio before Generate Guitar from Song");
    if(guitarBusy.exchange(true))return juce::Result::fail("Guitar generation is already running");
    std::vector<wingman::Chord> harmony;int keyCopy;double tempoCopy;juce::String text;std::shared_ptr<const wingman::Clip> originalGuitar;
    {const juce::ScopedLock lock(mutex);tempoCopy=bpm;originalGuitar=tracks[4].clip;harmony=chords;keyCopy=effectiveKey();text=chordText;guitarMessage="Generating playable guitar MIDI...";}guitarProgress=0;
    std::shared_ptr<const wingman::SampledGuitar> library;bool sampled;double curve;{const juce::ScopedLock lock(mutex);library=guitarLibrary;sampled=sampledGuitar;curve=guitarVelocityCurve;}
    guitarWorker.start([this,song,songRevision,songGeneration,followSong,library,sampled,curve,config=wingman::GuitarSettings(config),harmony,keyCopy,text,renderAudio,sampleRate,tempoCopy,originalGuitar](wingman::CancellationToken token)mutable{
        try{auto keep=[&]{return !token.stop_requested();};auto notes=followSong?wingman::guitarFromSong(song,config,tempoCopy,keep):wingman::generateGuitar(harmony,keyCopy,config,keep);
            std::shared_ptr<const wingman::Clip> rendered;
            if(renderAudio){
                {const juce::ScopedLock lock(mutex);guitarMessage=sampled?"Rendering sampled guitar...":"Rendering basic plucked-string synthesis...";}
                if(sampled&&!library)throw std::runtime_error("Load and verify an SFZ library, or explicitly select Basic Synthesis");
                auto progress=[&](double p){guitarProgress=0.1+p*0.85;return !token.stop_requested();};
                const auto audio=sampled?library->render(notes,config,tempoCopy,sampleRate,curve,progress):wingman::renderGuitar(notes,config,tempoCopy,sampleRate,progress);
                auto clip=std::make_shared<wingman::Clip>();clip->sampleRate=sampleRate;clip->name=sampled?"Sampled guitar":"Basic synthesized guitar";clip->audio.setSize(2,static_cast<int>(audio.channels[0].size()));
                for(int ch=0;ch<2;++ch)clip->audio.copyFrom(ch,0,audio.channels[static_cast<std::size_t>(ch)].data(),clip->audio.getNumSamples());clip->buildWaveform();rendered=clip;
            }
            if(token.stop_requested())throw std::runtime_error("Guitar generation cancelled");
            const juce::ScopedLock lock(mutex);if(token.stop_requested())throw std::runtime_error("Guitar generation cancelled");if(followSong&&(songRevision!=audioRevision.load()||songGeneration!=analysisGeneration.load()))throw std::runtime_error("Imported song changed; analyze and generate again");if(!followSong&&(text!=chordText||keyCopy!=effectiveKey()))throw std::runtime_error("Harmony changed; regenerate guitar");
            if(renderAudio){if(tracks[4].clip!=originalGuitar)throw std::runtime_error("Guitar lane changed; render again");if(bpm!=tempoCopy)throw std::runtime_error("Tempo changed; render guitar again");tracks[4].clip.swap(rendered);publishPlayback();++audioRevision;startTempoAnalysis(4);}
            guitarNotes=std::move(notes);guitarSettings=config;activePart=3;generated=true;rebuildMidi();guitarProgress=1;guitarMessage=renderAudio?(sampled?"Sampled guitar WAV rendered":"Guitar WAV rendered with basic synthesis (not sampled realism)"):"Guitar MIDI generated; audio requires Render WAV";
        }catch(const std::exception& ex){const juce::ScopedLock lock(mutex);guitarMessage=ex.what();}guitarBusy=false;
    });return juce::Result::ok();
}

wingman::TempoResult WingmanProcessor::tempoSnapshot()const{const juce::ScopedLock lock(mutex);if(tempoTrack<0||tracks[static_cast<std::size_t>(tempoTrack)].clip!=tempoClip)return {};return tempoResult;}
wingman::ModelJobInfo WingmanProcessor::tempoAnalysisStatus()const{const juce::ScopedLock lock(mutex);return {tempoBusy.load(),tempoProgress.load(),tempoMessage,{}};}
int WingmanProcessor::tempoSourceTrack()const{const juce::ScopedLock lock(mutex);return tempoTrack;}
void WingmanProcessor::cancelTempoAnalysis(){const juce::ScopedLock lock(tempoControlMutex);pendingTempo=-1;tempoWorker.request_stop();}
juce::Result WingmanProcessor::applyDetectedTempo(){
    const auto result=tempoSnapshot();if(!result.valid)return juce::Result::fail("Analyze a rhythmic audio clip first");
    if(hostSync())return juce::Result::fail("Disable DAW Sync before changing project tempo");setTempo(result.bpm);return juce::Result::ok();
}
juce::Result WingmanProcessor::startTempoAnalysis(int track){
    if(!validTrack(track)||!trackSnapshot(track).clip)return juce::Result::fail("Import or record audio in the selected lane first");
    const juce::ScopedLock control(tempoControlMutex);pendingTempo=track;
    if(tempoBusy.load())return juce::Result::ok();tempoBusy=true;
    tempoWorker.start([this](wingman::CancellationToken token){
        for(;;){
            int lane;
            {const juce::ScopedLock lock(tempoControlMutex);if(token.stop_requested()){pendingTempo=-1;tempoBusy=false;break;}lane=pendingTempo;pendingTempo=-1;if(lane<0){tempoBusy=false;break;}}
            const auto clip=trackSnapshot(lane).clip;if(!clip)continue;
            {const juce::ScopedLock lock(mutex);tempoResult={};tempoClip=clip;tempoTrack=lane;tempoMessage="Analyzing onsets and pulse...";}tempoProgress=0;
            try{auto result=wingman::analyseTempo(clip->audio.getReadPointer(0),clip->audio.getNumSamples(),clip->sampleRate,[&](double p){tempoProgress=p;return !token.stop_requested();},clip->audio.getReadPointer(1));
                const juce::ScopedLock lock(mutex);if(token.stop_requested())throw std::runtime_error("Tempo analysis cancelled");
                if(tracks[static_cast<std::size_t>(lane)].clip==clip){tempoResult=std::move(result);tempoProgress=1;tempoMessage=tempoResult.valid?"Tempo measured from audio. Apply explicitly; half/double ambiguity remains.":"No reliable pulse found; use rhythmic audio longer than three seconds";}
            }catch(const std::exception& ex){const juce::ScopedLock lock(mutex);tempoResult={};tempoMessage=ex.what();}
        }
    });return juce::Result::ok();
}

void WingmanProcessor::setVoiceLeading(bool enabled){const juce::ScopedLock lock(mutex);voiceLeading=enabled;rebuildMidi();}
bool WingmanProcessor::voiceLeadingEnabled()const{const juce::ScopedLock lock(mutex);return voiceLeading;}
