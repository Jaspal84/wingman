#include "Music/GuitarRhythmGenerator.h"

#include <iostream>
#include <stdexcept>
int main(){int checks=0;auto check=[&](bool condition){++checks;if(!condition)throw std::runtime_error("Guitar assertion "+std::to_string(checks));};
try{
 std::vector<wingman::Chord> chords;for(auto token:{"C","Am","F","G7"})chords.push_back(*wingman::parseChord(token));
 for(const auto& chord:chords){auto voice=wingman::guitarVoicing(chord);int low=20,high=0,strings=0;for(std::size_t string=0;string<6;++string){int fret=voice[string];if(fret<0)continue;++strings;check(fret<=12);if(fret){low=std::min(low,fret);high=std::max(high,fret);}auto allowed=chord.notes();check(std::any_of(allowed.begin(),allowed.end(),[&](int n){return n%12==(wingman::guitarTuning[string]+fret)%12;}));}check(strings>=3&&high-low<=3);}
 wingman::GuitarSettings config;auto notes=wingman::generateGuitar(chords,0,config);check(notes==wingman::generateGuitar(chords,0,config));check(!notes.empty());
 bool delayed=false;for(std::size_t i=0;i<notes.size();++i){const auto& n=notes[i];check(n.beat>=0&&n.duration>0&&n.beat+n.duration<=16.000001);check(n.note>=40&&n.note<=76&&n.velocity>=1&&n.velocity<=127&&n.channel>=1&&n.channel<=6);if(i&&n.beat>notes[i-1].beat&&n.beat-notes[i-1].beat<0.05)delayed=true;}check(delayed);
 for(int variation=1;variation<4;++variation){config.variation=variation;check(wingman::generateGuitar(chords,0,config)!=notes);}
 for(int technique=0;technique<8;++technique){config.technique=static_cast<wingman::GuitarTechnique>(technique);check(!wingman::generateGuitar(chords,0,config).empty());}
 std::cout<<checks<<" guitar generation/render checks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
