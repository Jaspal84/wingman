#include "Audio/SampleVoices.h"
#include <cstdlib>
#include <new>
#include <iostream>
static thread_local bool audit=false;
static thread_local std::size_t allocations=0;
void* operator new(std::size_t bytes){if(audit)++allocations;if(void* p=std::malloc(bytes?bytes:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p)noexcept{std::free(p);}void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
int main(){int checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("Realtime check "+std::to_string(checks));};try{
 wingman::SampledGuitar library;auto audio=std::make_shared<juce::AudioBuffer<float>>(2,4800);for(int i=0;i<4800;++i){audio->setSample(0,i,.01f);audio->setSample(1,i,.005f);}wingman::GuitarZone z;z.audio=audio;z.rate=48000;z.release=.02;library.zones.push_back(z);
 wingman::SampleVoices voices;voices.bind(&library,1);juce::MidiBuffer midi;midi.ensureSize(65536);for(int i=0;i<200;++i)midi.addEvent(juce::MidiMessage::noteOn(i%6+1,60,static_cast<juce::uint8>(70+i%40)),0);juce::AudioBuffer<float> block(2,512);block.clear();audit=true;voices.render(block,midi,48000,24,1);audit=false;check(allocations==0);check(voices.activeVoices()==128);check(block.getMagnitude(0,512)>0);float previous=block.getSample(0,511);block.clear();audit=true;voices.render(block,midi,48000,24,1);audit=false;check(allocations==0&&std::abs(block.getSample(0,0)-previous)<.05f);for(int i=0;i<512;++i)check(std::isfinite(block.getSample(0,i)));
 midi.clear();for(int c=1;c<=6;++c)midi.addEvent(juce::MidiMessage::noteOff(c,60),0);for(int i=0;i<30;++i){block.clear();audit=true;voices.render(block,midi,48000,24,1);audit=false;midi.clear();}check(allocations==0&&voices.activeVoices()==0);
 voices.bind(nullptr,2);block.clear();voices.render(block,midi,48000,24,1);check(block.getMagnitude(0,512)==0);
 std::cout<<checks<<" polyphony/voice-steal/C++ allocation checks passed\n";return 0;
 }catch(const std::exception& e){audit=false;std::cerr<<e.what()<<'\n';return 1;}}
