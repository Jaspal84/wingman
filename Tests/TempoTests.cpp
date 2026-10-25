#include "Analysis/TempoAnalysis.h"
#include <iostream>
int main(){int checks=0;for(double bpm:{80.0,90.0,100.0,120.0,140.0})for(bool rhythm:{false,true}){
 const double rate=44100;std::vector<float> audio(static_cast<std::size_t>(rate*12));
 for(int beat=0;beat<static_cast<int>(12*bpm/60);++beat){const int at=static_cast<int>((0.1+beat*60/bpm)*rate);for(int i=0;i<500&&at+i<static_cast<int>(audio.size());++i)audio[static_cast<std::size_t>(at+i)]+=static_cast<float>((rhythm&&beat%2?0.8:1)*std::exp(-i/80.0)*std::sin(i*0.7));if(rhythm){const int hat=at+static_cast<int>(30/bpm*rate);for(int i=0;i<100&&hat+i<static_cast<int>(audio.size());++i)audio[static_cast<std::size_t>(hat+i)]+=static_cast<float>(0.08*std::exp(-i/20.0)*std::sin(i*2.1));}}
 const auto result=wingman::analyseTempo(audio.data(),static_cast<int>(audio.size()),rate);
 if(!result.valid||std::abs(result.bpm-bpm)>1||result.confidence<0.8||result.beats.size()<8){std::cerr<<"Failed "<<bpm<<" got "<<result.bpm<<'\n';return 1;}++checks;std::vector<float> left(audio.size(),0);const auto stereo=wingman::analyseTempo(left.data(),static_cast<int>(left.size()),rate,{},audio.data());if(!stereo.valid||std::abs(stereo.bpm-bpm)>1)return 1;++checks;
 }
 for(double bpm:{80.0,90.0,100.0,120.0,140.0}){wingman::TapTempo taps;std::optional<double> value;for(int i=0;i<8;++i)value=taps.tap(i*60/bpm);if(!value||std::abs(*value-bpm)>0.001)return 1;++checks;if(taps.tap(20).has_value())return 1;}
 std::vector<float> silence(44100*4);if(wingman::analyseTempo(silence.data(),static_cast<int>(silence.size()),44100).valid)return 1;
 bool cancelled=false;try{wingman::analyseTempo(silence.data(),static_cast<int>(silence.size()),44100,[](double){return false;});}catch(const std::runtime_error&){cancelled=true;}
 if(!cancelled)return 1;std::cout<<checks+2<<" tempo tests passed\n";
}
