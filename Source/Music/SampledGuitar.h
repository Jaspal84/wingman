#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "GuitarRenderer.h"
#include <map>
#include <regex>
namespace wingman {
// Deliberately strict SFZ subset: unsupported opcodes fail verification.
struct GuitarZone {
    std::shared_ptr<const juce::AudioBuffer<float>> audio;
    double rate=0,release=.25,volume=1,tune=0;
    int low=0,high=127,root=60,loVel=1,hiVel=127,loChan=1,hiChan=16,seqLength=1,seqPosition=1;
    int articulation=-1;
    bool onRelease=false;
};
struct SampledGuitar {
    std::vector<GuitarZone> zones;
    std::size_t bytes=0;
    static SampledGuitar load(const juce::File& sfz,const std::function<bool(double)>& progress={}) {
        if(!sfz.existsAsFile()||sfz.getSize()>1024*1024)throw std::runtime_error("Missing SFZ or SFZ exceeds 1 MiB");
        auto source=sfz.loadFileAsString().toStdString();
        source=std::regex_replace(source,std::regex(R"(//[^\r\n]*)"),"");
        const std::regex token(R"((<[^>]+>)|([A-Za-z0-9_]+)\s*=\s*([^<]*?)(?=\s+[A-Za-z0-9_]+\s*=|\s*<|$))");
        std::map<std::string,std::string> global,group,region;
        SampledGuitar result;juce::AudioFormatManager formats;formats.registerBasicFormats();
        std::map<juce::String,std::pair<std::shared_ptr<const juce::AudioBuffer<float>>,double>> cache;
        bool active=false;std::string section;
        auto flush=[&]{if(!active)return;
            if(result.zones.size()>=2048)throw std::runtime_error("SFZ exceeds 2048 zones");
            auto values=global;for(const auto& p:group)values[p.first]=p.second;for(const auto& p:region)values[p.first]=p.second;
            GuitarZone z;juce::String sample;
            auto number=[](const std::string& s){std::size_t used=0;double v=std::stod(s,&used);if(used!=s.size()||!std::isfinite(v))throw std::runtime_error("Invalid numeric SFZ opcode");return v;};
            if(values.count("key")){double key=number(values["key"]);if(key<0||key>127||key!=std::floor(key))throw std::runtime_error("Invalid SFZ key");z.low=z.high=z.root=static_cast<int>(key);}
            for(const auto& [k,v]:values){
                if(k=="sample"){sample=juce::String(v).unquoted();continue;}
                if(k=="trigger"){if(v!="attack"&&v!="release")throw std::runtime_error("Unsupported SFZ trigger");z.onRelease=v=="release";continue;}
                if(k=="loop_mode"){if(v!="no_loop")throw std::runtime_error("Only no_loop supported");continue;}
                double n=number(v);
                if(k!="ampeg_release"&&k!="volume"&&k!="tune"&&(n!=std::floor(n)||n < -100000||n>100000))throw std::runtime_error("SFZ mapping values must be bounded integers");
                if(k=="key")continue;
                else if(k=="sw_last")z.articulation=static_cast<int>(n);
                else if(k=="lokey")z.low=static_cast<int>(n);else if(k=="hikey")z.high=static_cast<int>(n);
                else if(k=="pitch_keycenter")z.root=static_cast<int>(n);else if(k=="lovel")z.loVel=static_cast<int>(n);else if(k=="hivel")z.hiVel=static_cast<int>(n);
                else if(k=="lochan")z.loChan=static_cast<int>(n);else if(k=="hichan")z.hiChan=static_cast<int>(n);
                else if(k=="seq_length")z.seqLength=static_cast<int>(n);else if(k=="seq_position")z.seqPosition=static_cast<int>(n);
                else if(k=="ampeg_release")z.release=n;else if(k=="volume")z.volume=std::pow(10.0,n/20);else if(k=="tune")z.tune=n;
                else throw std::runtime_error("Unsupported SFZ opcode: "+k);
            }
            if(z.articulation < -1 || z.articulation>127 ||sample.isEmpty()||z.low<0||z.high>127||z.low>z.high||z.root<0||z.root>127||z.loVel<1||z.hiVel>127||z.loVel>z.hiVel||z.loChan<1||z.hiChan>16||z.loChan>z.hiChan||z.seqLength<1||z.seqLength>128||z.seqPosition<1||z.seqPosition>z.seqLength||z.release<.001||z.release>10||z.volume>16||std::abs(z.tune)>1200)throw std::runtime_error("Invalid SFZ zone");
            auto file=sfz.getSiblingFile(sample);auto name=file.getFullPathName();
            if(!cache.count(name)){
                std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
                if(!reader||reader->numChannels<1||reader->numChannels>2||!std::isfinite(reader->sampleRate)||reader->sampleRate<8000||reader->sampleRate>192000||reader->lengthInSamples<2||reader->lengthInSamples>reader->sampleRate*120)throw std::runtime_error("Missing, corrupt or unsupported sample: "+sample.toStdString());
                auto size=static_cast<std::size_t>(reader->lengthInSamples)*reader->numChannels*sizeof(float);
                if(size>256*1024*1024-result.bytes)throw std::runtime_error("Sample library exceeds 256 MiB decoded budget; split into smaller SFZ instruments");
                auto data=std::make_shared<juce::AudioBuffer<float>>(static_cast<int>(reader->numChannels),static_cast<int>(reader->lengthInSamples));
                if(!reader->read(data.get(),0,data->getNumSamples(),0,true,true))throw std::runtime_error("Sample decode failed");
                for(int c=0;c<data->getNumChannels();++c)for(int i=0;i<data->getNumSamples();++i)if(!std::isfinite(data->getSample(c,i)))throw std::runtime_error("Nonfinite sample audio");
                result.bytes+=size;cache[name]={data,reader->sampleRate};
            }
            z.audio=cache[name].first;z.rate=cache[name].second;result.zones.push_back(z);
            if(progress&&!progress(std::min(.99,result.zones.size()/2048.0)))throw std::runtime_error("Sample loading cancelled");
        };
        std::size_t consumed=0;
        for(std::sregex_iterator it(source.begin(),source.end(),token),end;it!=end;++it){
            if(juce::String(source.substr(consumed,static_cast<std::size_t>(it->position())-consumed)).trim().isNotEmpty())throw std::runtime_error("Malformed SFZ text");
            consumed=static_cast<std::size_t>(it->position()+it->length());
            if((*it)[1].matched){flush();section=(*it)[1];active=section=="<region>";region.clear();if(section=="<global>"){global.clear();group.clear();}else if(section=="<group>")group.clear();else if(!active)throw std::runtime_error("Unsupported SFZ section: "+section);}
            else{auto key=(*it)[2].str();auto value=juce::String((*it)[3].str()).trim().toStdString();if(section=="<global>")global[key]=value;else if(section=="<group>")group[key]=value;else if(active)region[key]=value;else throw std::runtime_error("SFZ opcode outside section");}
        }if(juce::String(source.substr(consumed)).trim().isNotEmpty())throw std::runtime_error("Malformed SFZ text");flush();if(result.zones.empty())throw std::runtime_error("SFZ has no sample regions");if(progress&&!progress(1))throw std::runtime_error("Sample loading cancelled");return result;
    }
    int articulationFor(const NoteEvent& note,int fallback,int velocity)const{
        if(note.articulation>0)for(const auto& z:zones)if(!z.onRelease&&z.articulation==note.articulation&&note.note>=z.low&&note.note<=z.high&&velocity>=z.loVel&&velocity<=z.hiVel&&note.channel>=z.loChan&&note.channel<=z.hiChan)return note.articulation;
        return fallback;
    }
    GuitarAudio render(const std::vector<NoteEvent>& notes,const GuitarSettings& settings,double bpm,double rate,double velocityCurve=1,const std::function<bool(double)>& next={})const {
        validateGuitarSettings(settings);if((rate!=44100&&rate!=48000)||!std::isfinite(bpm)||bpm<40||bpm>240||notes.empty()||!std::isfinite(velocityCurve)||velocityCurve<.25||velocityCurve>4)throw std::runtime_error("Invalid sampled render settings");
        double tail=0;for(const auto& z:zones)tail=std::max(tail,z.release);
        const double duration=settings.bars*settings.numerator*4.0/settings.denominator*60/bpm;
        if(duration+tail>120)throw std::runtime_error("Sampled render exceeds 120 seconds including release");
        GuitarAudio output;output.sampleRate=rate;for(auto& c:output.channels)c.assign(static_cast<std::size_t>(std::ceil((duration+tail)*rate)),0);
        std::array<int,16> counters{};bool heard=false;
        for(std::size_t n=0;n<notes.size();++n){if(next&&!next(n/static_cast<double>(notes.size())))throw std::runtime_error("Sample render cancelled");const auto& note=notes[n];
            if(note.channel<1||note.channel>16||note.note<0||note.note>127||note.velocity<1||note.velocity>127||!std::isfinite(note.beat)||!std::isfinite(note.duration)||note.beat<0||note.duration<=0||note.beat+note.duration>duration*bpm/60+.001)throw std::runtime_error("Invalid sample MIDI note");
            int velocity=std::clamp(static_cast<int>(std::round(127*std::pow(note.velocity/127.0,velocityCurve))),1,127);int round=counters[static_cast<std::size_t>(note.channel-1)]++;const int articulation=articulationFor(note,24+static_cast<int>(settings.technique),velocity);bool mapped=false;
            for(const auto& z:zones){if((z.articulation>=0&&z.articulation!=articulation)||note.note<z.low||note.note>z.high||velocity<z.loVel||velocity>z.hiVel||note.channel<z.loChan||note.channel>z.hiChan||round%z.seqLength+1!=z.seqPosition)continue;
                mapped|=!z.onRelease;double start=std::round((note.beat+(z.onRelease?note.duration:0))*60/bpm*rate);double hold=note.duration*60/bpm;double step=z.rate/rate*std::pow(2.0,(note.note-z.root+z.tune/100)/12.0);
                for(std::size_t i=0;static_cast<std::size_t>(start)+i<output.channels[0].size();++i){if(i%8192==0&&next&&!next(n/static_cast<double>(notes.size())))throw std::runtime_error("Sample render cancelled");double pos=i*step;if(pos>=z.audio->getNumSamples()-1)break;double t=i/rate;double envelope=std::min(1.0,t/.002);if(z.onRelease){if(t>z.release)break;envelope*=std::max(0.0,1-t/z.release);}else if(t>hold){if(t>hold+z.release)break;envelope*=std::max(0.0,1-(t-hold)/z.release);}
                    for(int c=0;c<2;++c){int ch=std::min(c,z.audio->getNumChannels()-1),at=static_cast<int>(pos);double v=z.audio->getSample(ch,at)*(1-(pos-at))+z.audio->getSample(ch,at+1)*(pos-at);output.channels[static_cast<std::size_t>(c)][static_cast<std::size_t>(start)+i]+=static_cast<float>(v*envelope*z.volume*velocity/127.0);}
                }
            }if(!mapped)throw std::runtime_error("No attack sample mapping for generated note/velocity/string; choose a complete library");heard=true;
        }
        float peak=0;for(const auto& c:output.channels)for(float v:c){if(!std::isfinite(v))throw std::runtime_error("Sample render overflow");peak=std::max(peak,std::abs(v));}if(!heard||peak<1e-9f)throw std::runtime_error("Sample library rendered silence");if(peak>.95f)for(auto& c:output.channels)for(float& v:c)v*=.95f/peak;return output;
    }
};
}
