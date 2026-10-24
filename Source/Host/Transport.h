#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <optional>
namespace wingman {
struct HostFrame {
    double bpm=120,ppq=0,seconds=0;
    int numerator=4,denominator=4;
    bool playing=false,looping=false;
    double loopStart=0,loopEnd=0;
};
struct BeatSpan { double beat=0; int offset=0,count=0; bool restart=false; };
struct TransportBlock {
    bool valid=false,playing=false,discontinuity=false;
    HostFrame frame;
    std::array<BeatSpan,64> spans{};
    int spanCount=0;
};
class HostClock {
    bool hadPosition=false,wasPlaying=false;
    double expected=0;
public:
    void reset(){hadPosition=wasPlaying=false;expected=0;}
    TransportBlock resolve(std::optional<HostFrame> input,int samples,double rate){
        TransportBlock block;
        if(!input||!std::isfinite(rate)||rate<=0||samples<0){block.discontinuity=wasPlaying;reset();return block;}
        auto frame=*input;
        if(!std::isfinite(frame.bpm)||frame.bpm<20||frame.bpm>400||!std::isfinite(frame.ppq)||std::abs(frame.ppq)>1.0e9
            ||!std::isfinite(frame.seconds)||std::abs(frame.seconds)>1.0e9||frame.numerator<1||frame.numerator>32
            ||(frame.denominator!=1&&frame.denominator!=2&&frame.denominator!=4&&frame.denominator!=8&&frame.denominator!=16)){
            block.discontinuity=wasPlaying;reset();return block;
        }
        frame.looping=frame.looping&&std::isfinite(frame.loopStart)&&std::isfinite(frame.loopEnd)&&frame.loopEnd-frame.loopStart>=0.01;
        block.valid=true;block.playing=frame.playing;block.frame=frame;
        block.discontinuity=hadPosition&&(wasPlaying!=frame.playing||(frame.playing&&std::abs(frame.ppq-expected)>1.0e-5));
        double beat=frame.ppq;int offset=0;
        const double beatsPerSample=frame.bpm/(60*rate);
        while(offset<samples&&block.spanCount<static_cast<int>(block.spans.size())){
            bool restart=false;
            if(frame.looping&&beat>=frame.loopEnd-1e-10){beat=frame.loopStart+std::fmod(std::max(0.0,beat-frame.loopEnd),frame.loopEnd-frame.loopStart);restart=true;}
            int count=samples-offset;
            if(frame.looping&&beat<frame.loopEnd) count=std::min(count,std::max(1,static_cast<int>(std::ceil((frame.loopEnd-beat)/beatsPerSample-1e-8))));
            block.spans[static_cast<std::size_t>(block.spanCount++)]={beat,offset,count,restart};
            beat+=count*beatsPerSample;offset+=count;
        }
        if(offset<samples){block.playing=false;block.discontinuity=true;block.spanCount=0;} // Malformed ultra-short loop/buffer: silence safely.
        expected=beat;
        if(frame.looping&&expected>=frame.loopEnd-1e-10)expected=frame.loopStart+std::fmod(std::max(0.0,expected-frame.loopEnd),frame.loopEnd-frame.loopStart);
        hadPosition=true;wasPlaying=frame.playing;return block;
    }
};
}
