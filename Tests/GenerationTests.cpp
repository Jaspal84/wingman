#include "Generation/Patterns.h"
#include <iostream>
#include <stdexcept>
int count = 0;
void check(bool ok,const char* text) {++count;if(!ok)throw std::runtime_error(text);}
int main() {
    using namespace wingman;
    try {
        std::vector<Chord> chords{*parseChord("C"),*parseChord("Am"),*parseChord("F"),*parseChord("G")};
        for (int style=0;style<8;++style) for(int bars : {4,8,16,32}) for (auto kind : {PatternKind::Bass,PatternKind::Drums,PatternKind::Melody}) {
            PatternSettings settings; settings.style=static_cast<Style>(style); settings.bars=bars; settings.humanize=0.5; settings.swing=0.3;
            auto notes=generatePattern(kind,chords,0,settings);
            check(!notes.empty(),"nonempty pattern"); check(notes==generatePattern(kind,chords,0,settings),"seed reproducibility");
            for (std::size_t i=0;i<notes.size();++i) {
                const auto& n=notes[i];
                check(n.note>=0&&n.note<=127&&n.velocity>=1&&n.velocity<=127&&n.beat>=0&&n.duration>0&&n.beat+n.duration<=bars*4+1e-6,"note bounds and timing");
                if (kind!=PatternKind::Drums) {
                    check(inScale(n.note,settings.keyRoot,settings.minor),"scale constraints");
                    if (i+1<notes.size()) check(n.beat+n.duration<=notes[i+1].beat+1e-6,"monophonic collisions");
                    if (kind==PatternKind::Melody) check(n.note>=settings.lowest&&n.note<=settings.highest,"melody range");
                } else check(n.channel==10,"GM drum channel");
            }
            settings.variation=1; check(notes!=generatePattern(kind,chords,0,settings),"variation changes pattern");
        }
        std::cout<<count<<" music checks passed\n";
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}
}
