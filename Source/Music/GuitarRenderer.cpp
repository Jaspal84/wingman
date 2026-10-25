#include "GuitarRenderer.h"
namespace wingman {
GuitarAudio renderGuitar(const std::vector<NoteEvent>& notes,const GuitarSettings& settings,double bpm,double rate,const std::function<bool(double)>& next){
    validateGuitarSettings(settings);
    if((rate!=44100&&rate!=48000)||!std::isfinite(bpm)||bpm<40||bpm>240||notes.empty())throw std::invalid_argument("Render requires notes, 40-240 BPM, 44.1/48 kHz");
    const double seconds=settings.bars*settings.numerator*4.0/settings.denominator*60.0/bpm;
    if(seconds+0.4>120)throw std::invalid_argument("Rendered guitar must fit the 120-second lane limit");
    GuitarAudio audio;audio.sampleRate=rate;const auto count=static_cast<std::size_t>(std::ceil((seconds+0.4)*rate));
    for(auto& channel:audio.channels)channel.assign(count,0);
    std::mt19937 random(settings.seed);auto noise=[&]{return random()/static_cast<double>(std::mt19937::max())*2-1;};
    for(std::size_t n=0;n<notes.size();++n){
        if(next&&!next(n/static_cast<double>(notes.size())))throw std::runtime_error("Guitar render cancelled");
        const auto& note=notes[n];
        if(note.channel<1||note.channel>6||note.note<40||note.note>88||note.velocity<1||note.velocity>127||!std::isfinite(note.beat)||!std::isfinite(note.duration)||note.beat<0||note.duration<=0||note.beat+note.duration>seconds*bpm/60+0.001)throw std::invalid_argument("Invalid guitar MIDI note");
        const double frequency=440*std::pow(2.0,(note.note-69)/12.0);
        const auto delay=static_cast<std::size_t>(std::max(2.0,std::round(rate/frequency)));
        std::vector<double> string(delay);for(auto& sample:string)sample=noise();
        const double duration=note.duration*60/bpm,pan=std::clamp((note.channel-1)/5.0,0.0,1.0);
        const auto start=static_cast<std::size_t>(note.beat*60/bpm*rate);
        const auto length=static_cast<std::size_t>((duration+0.4)*rate);
        const double decay=settings.type==GuitarType::Nylon?0.988:0.996;
        for(std::size_t i=0;i<length&&start+i<count;++i){
            const auto at=i%delay;double value=string[at];string[at]=decay*(string[at]+string[(at+1)%delay])*0.5;
            const double time=i/rate,envelope=std::min(1.0,time/0.002)*(time>duration?std::exp(-(time-duration)*16):1);
            if(settings.type==GuitarType::DistortedElectric)value=std::tanh(value*3)*0.65;
            if(settings.type==GuitarType::TwelveString)value+=0.14*std::sin(time*frequency*12.566370614359)*std::exp(-time*6);
            if(settings.type==GuitarType::CleanElectric)value*=0.8;
            value*=envelope*note.velocity/127.0*0.18;
            audio.channels[0][start+i]+=static_cast<float>(value*(0.8-pan*0.3));
            audio.channels[1][start+i]+=static_cast<float>(value*(0.5+pan*0.3));
        }
    }
    float peak=0;for(const auto& channel:audio.channels)for(float value:channel){if(!std::isfinite(value))throw std::runtime_error("Nonfinite guitar audio");peak=std::max(peak,std::abs(value));}
    if(peak>0.95f)for(auto& channel:audio.channels)for(float& value:channel)value*=0.95f/peak;
    if(next&&!next(1))throw std::runtime_error("Guitar render cancelled");
    return audio;
}
}
