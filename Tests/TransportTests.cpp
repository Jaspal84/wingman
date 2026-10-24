#include "Host/Transport.h"
#include "Threading/BackgroundThread.h"
#include <iostream>
#include <stdexcept>
int checks=0;void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
int main(){using namespace wingman;try{
    HostClock clock;HostFrame frame;frame.playing=true;
    auto block=clock.resolve(frame,480,48000);check(block.valid&&block.playing&&block.spanCount==1,"host start");
    frame.ppq=0.02;frame.seconds=0.01;block=clock.resolve(frame,480,48000);check(!block.discontinuity,"continuous host");
    frame.ppq=8;frame.seconds=4;block=clock.resolve(frame,480,48000);check(block.discontinuity,"seek discontinuity");
    frame.ppq=8.02;frame.bpm=90;block=clock.resolve(frame,480,48000);check(!block.discontinuity,"tempo change at continuous position");
    frame.playing=false;block=clock.resolve(frame,480,48000);check(!block.playing&&block.discontinuity,"host stop");
    check(!clock.resolve(std::nullopt,480,48000).valid,"missing position");
    frame.bpm=120;frame.ppq=3.99;frame.looping=true;frame.loopStart=0;frame.loopEnd=4;frame.playing=true;
    block=clock.resolve(frame,1024,48000);check(block.spanCount==2&&block.spans[1].restart,"split loop inside buffer");
    check(block.spans[0].count==240&&block.spans[1].offset==240,"sample-accurate loop boundary");
    frame.ppq=0.0326666666667;block=clock.resolve(frame,480,48000);check(!block.discontinuity,"expected wrapped host position");
    frame.bpm=std::nan("");check(!clock.resolve(frame,480,48000).valid,"nonfinite tempo rejected");
    frame.bpm=120;frame.ppq=-1;frame.seconds=-0.5;check(clock.resolve(frame,480,48000).valid,"negative preroll supported");
    frame.denominator=3;check(!clock.resolve(frame,480,48000).valid,"invalid meter rejected");
    std::atomic<bool> started{false},cancelled{false};
    {BackgroundThread worker;worker.start([&](CancellationToken token){started=true;while(!token.stop_requested())std::this_thread::yield();cancelled=true;});
     while(!started)std::this_thread::yield();worker.request_stop();worker.join();check(cancelled,"portable worker cooperative cancellation");}
    std::cout<<checks<<" transport checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
