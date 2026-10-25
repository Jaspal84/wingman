#include "SampledGuitar.h"
#include "Audio/SampleVoices.h"
#include "MIDI/PatternSequence.h"
namespace wingman {
GuitarAudio SampledGuitar::render(const std::vector<NoteEvent>& notes,const GuitarSettings& settings,double bpm,double rate,double curve,const std::function<bool(double)>& next)const{
    validateGuitarSettings(settings);
    if((rate!=44100&&rate!=48000)||!std::isfinite(bpm)||bpm<40||bpm>240||notes.empty()||!std::isfinite(curve)||curve<.25||curve>4)throw std::invalid_argument("Invalid sampled render settings");
    const double duration=settings.bars*settings.numerator*4.0/settings.denominator*60/bpm;
    double tail=0;for(const auto& z:zones)tail=std::max(tail,z.release);
    if(duration+tail>120)throw std::runtime_error("Render exceeds 120 seconds including release");
    for(const auto& n:notes)if(n.note<0||n.note>127||n.channel<1||n.channel>16||n.velocity<1||n.velocity>127||!std::isfinite(n.beat)||!std::isfinite(n.duration)||n.beat<0||n.duration<=0||n.beat+n.duration>duration*bpm/60+.001)throw std::invalid_argument("Invalid sample MIDI note");
    GuitarAudio output;output.sampleRate=rate;const int frames=static_cast<int>(std::ceil((duration+tail)*rate));for(auto& channel:output.channels)channel.assign(static_cast<std::size_t>(frames),0);
    auto sequence=patternSequence(notes);SampleVoices voices;voices.bind(this,1);juce::AudioBuffer<float> block(2,512);juce::MidiBuffer events;events.ensureSize(65536);int event=0;
    float peak=0;
    for(int first=0;first<frames;first+=512){if(next&&!next(first/static_cast<double>(frames)))throw std::runtime_error("Sample render cancelled");int count=std::min(512,frames-first);block.setSize(2,count,false,false,true);block.clear();events.clear();
        while(event<sequence.getNumEvents()){const auto& message=sequence.getEventPointer(event)->message;auto at=static_cast<int>(std::llround(message.getTimeStamp()*60/bpm*rate));if(at>=first+count)break;events.addEvent(message,std::max(0,at-first));++event;}
        voices.render(block,events,rate,24+static_cast<int>(settings.technique),curve,settings.pickNoise,settings.fretNoise,settings.resonance);if(voices.unmapped)throw std::runtime_error("No attack sample mapping for generated note, velocity or string");
        for(int c=0;c<2;++c)for(int i=0;i<count;++i){float value=block.getSample(c,i);if(!std::isfinite(value))throw std::runtime_error("Sample render overflow");output.channels[static_cast<std::size_t>(c)][static_cast<std::size_t>(first+i)]=value;peak=std::max(peak,std::abs(value));}
    }
    if(peak<1e-9f)throw std::runtime_error("Sample library rendered silence");if(peak>.95f)for(auto& channel:output.channels)for(float& value:channel)value*=.95f/peak;if(next&&!next(1))throw std::runtime_error("Sample render cancelled");return output;
}
}
