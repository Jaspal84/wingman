#include "Music/CinematicIdeas.h"
#include "Music/GuitarRhythmGenerator.h"
#include <iostream>
#include <stdexcept>
int main(){int checks=0;auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("Advanced chord check "+std::to_string(checks));};try{
 for(auto token:{"Am9","Fmaj7","Cadd9","Gsus4","Dm(add9)","Bbmaj7","Fmaj9","C/E","G7(b9)","A7(#9)","D7(#11)","E7(b13)","Cm11","C13","Cmaj13","Cm13"}){auto c=wingman::parseChord(token);check(c.has_value());check(wingman::parseChord(wingman::chordName(*c)).has_value());auto voice=wingman::guitarVoicing(*c);int strings=0;for(int fret:voice)strings+=fret>=0;check(strings>=3);}
 auto slash=*wingman::parseChord("C/E");check(slash.notes().front()%12==4);auto voice=wingman::guitarVoicing(slash);for(int string=0;string<6;++string)if(voice[static_cast<std::size_t>(string)]>=0){check((wingman::guitarTuning[static_cast<std::size_t>(string)]+voice[static_cast<std::size_t>(string)])%12==4);break;}
 for(auto invalid:{"C/","C/H","C(add9","Cadd9)","C^7","Qmaj9","G7(nope)","Cmaj9:0","C/E/H"})check(!wingman::parseChord(invalid));
 for(int style=0;style<8;++style)for(int seed=0;seed<8;++seed){auto chords=wingman::cinematicIdeas(style,8,static_cast<unsigned>(seed));check(chords.size()==8);for(const auto& chord:chords){check(wingman::parseChord(wingman::chordName(chord)).has_value());check(chord.notes().size()>=3);}}
 std::cout<<checks<<" advanced chord tests passed\n";
 }catch(const std::exception& ex){std::cerr<<ex.what()<<'\n';return 1;}}
