#pragma once
#include "Transport.h"
#include <atomic>
namespace wingman {
class HostTelemetry {
    std::atomic<unsigned> sequence{0};
    std::array<std::atomic<double>,5> values{};
    std::array<std::atomic<int>,5> flags{};
public:
    void publish(const std::optional<HostFrame>& frame){++sequence;flags[0]=frame.has_value();if(frame){values[0]=frame->bpm;values[1]=frame->ppq;values[2]=frame->seconds;values[3]=frame->loopStart;values[4]=frame->loopEnd;flags[1]=frame->numerator;flags[2]=frame->denominator;flags[3]=frame->playing;flags[4]=frame->looping;}++sequence;}
    std::optional<HostFrame> read()const{for(int attempt=0;attempt<16;++attempt){auto before=sequence.load();if(before&1u)continue;bool valid=flags[0].load();HostFrame f;f.bpm=values[0];f.ppq=values[1];f.seconds=values[2];f.loopStart=values[3];f.loopEnd=values[4];f.numerator=flags[1];f.denominator=flags[2];f.playing=flags[3];f.looping=flags[4];if(before==sequence.load())return valid?std::optional{f}:std::nullopt;}return {};}
};
}
