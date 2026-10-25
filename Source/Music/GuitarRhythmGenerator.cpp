#include "GuitarRhythmGenerator.h"
#include <functional>
namespace wingman {
std::array<int,6> guitarVoicing(const Chord& chord,int transpose){
    unsigned target=0;for(auto note:chord.notes(transpose))target|=1u<<(note%12);
    const int bass=(chord.root+transpose+120)%12;
    std::array<int,6> best{-1,-1,-1,-1,-1,-1},candidate=best;double bestScore=1e9;
    for(int position=0;position<=9;++position){
        std::function<void(int,unsigned,int,int,int)> visit=[&](int string,unsigned mask,int count,int first,int sum){
            if(string==6){
                if(count<3 || !(mask&(1u<<bass)))return;
                int missing=0;for(int pc=0;pc<12;++pc)if((target&(1u<<pc))&&!(mask&(1u<<pc)))++missing;
                const double score=missing*100.0+(first!=bass?18:0)+(6-count)*3+sum*0.35+position*0.2;
                if(score<bestScore){bestScore=score;best=candidate;}return;
            }
            candidate[static_cast<std::size_t>(string)]=-1;visit(string+1,mask,count,first,sum);
            for(int fret=0;fret<=12;++fret){
                if(fret && (fret<position || fret>position+3))continue;
                const int pc=(guitarTuning[static_cast<std::size_t>(string)]+fret)%12;
                if(!(target&(1u<<pc)))continue;
                candidate[static_cast<std::size_t>(string)]=fret;
                visit(string+1,mask|(1u<<pc),count+1,count?first:pc,sum+fret);
            }
        };visit(0,0,0,-1,0);
    }
    if(bestScore==1e9)throw std::invalid_argument("No playable guitar voicing");
    return best;
}
void validateGuitarSettings(const GuitarSettings& s){
    if(s.bars<1||s.bars>32||s.variation<0||s.variation>3||
       (s.numerator!=3&&s.numerator!=4&&s.numerator!=6)||(s.denominator!=4&&s.denominator!=8))throw std::invalid_argument("Invalid guitar length, variation or meter");
    for(double value:{s.energy,s.density,s.swing})if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Guitar parameters must be 0-1");
    if(static_cast<int>(s.technique)<0||static_cast<int>(s.technique)>7||static_cast<int>(s.style)<0||static_cast<int>(s.style)>7||static_cast<int>(s.type)<0||static_cast<int>(s.type)>4)throw std::invalid_argument("Invalid guitar selection");
}
std::vector<NoteEvent> generateGuitar(const std::vector<Chord>& harmony,int transpose,const GuitarSettings& s,const std::function<bool()>& next){
    if(harmony.empty())throw std::invalid_argument("A guitar chord progression is required");
    validateGuitarSettings(s);
    const double barBeats=s.numerator*4.0/s.denominator,total=s.bars*barBeats,cycle=progressionBeats(harmony);
    std::mt19937 random(s.seed+static_cast<unsigned>(s.variation)*2246822519u);
    auto unit=[&]{return random()/static_cast<double>(std::mt19937::max());};
    std::vector<NoteEvent> notes;
    const std::array<int,8> base{1,0,1,-1,0,-1,1,-1};
    for(int bar=0;bar<s.bars;++bar)for(int step=0;step<static_cast<int>(barBeats*2);++step){
        if(next&&!next())throw std::runtime_error("Guitar generation cancelled");
        int stroke=base[static_cast<std::size_t>(step%8)];
        if(s.variation==1&&step==4)stroke=1;
        if(s.variation==2&&step==1)stroke=-1;
        if(s.variation==3&&step==2)stroke=0;
        if(s.style==GuitarStyle::PunjabiPop&&step==1)stroke=-1;
        if(s.style==GuitarStyle::Ambient&&step%4!=0)stroke=0;
        if(s.style==GuitarStyle::Rock&&step%2==0)stroke=1;
        if(s.style==GuitarStyle::Bollywood&&step==4)stroke=1;
        if(s.style==GuitarStyle::Folk&&step==0)stroke=1;
        if(s.style==GuitarStyle::Jazz&&step==1)stroke=-1;
        if(s.style==GuitarStyle::Indie&&step==7)stroke=0;
        const bool picked=s.technique==GuitarTechnique::Fingerpicking||s.technique==GuitarTechnique::Arpeggio||s.technique==GuitarTechnique::Hybrid;
        if(picked)stroke=step%2?-1:1;
        if(!stroke || (step && unit()>s.density+0.25))continue;
        if(s.technique==GuitarTechnique::Downstroke)stroke=1;
        if(s.technique==GuitarTechnique::Upstroke)stroke=-1;
        const double at=bar*barBeats+step*0.5+(step%2?s.swing*0.16:0);
        double chordAt=std::fmod(at,cycle);const Chord* chord=&harmony.back();
        for(const auto& item:harmony){if(chordAt<item.beats){chord=&item;break;}chordAt-=item.beats;}
        const auto frets=guitarVoicing(*chord,transpose);
        std::vector<int> strings;for(int string=0;string<6;++string)if(frets[static_cast<std::size_t>(string)]>=0)strings.push_back(string);
        if(stroke<0)std::reverse(strings.begin(),strings.end());
        if(stroke<0&&strings.size()>4)strings.resize(4);
        if(picked){const auto string=strings[static_cast<std::size_t>(step)%strings.size()];strings={string};}
        int order=0;
        for(int string:strings){
            const double jitter=s.humanize?(unit()-0.5)*0.02:0;
            const double beat=std::clamp(at+order++*0.018+jitter,bar*barBeats,total-0.01);
            const bool accent=step==0||step==6;
            const int velocity=std::clamp(static_cast<int>(43+s.energy*55+(accent?12:0)+(stroke<0?-15:0)+(string<2&&stroke>0?6:0)+(s.humanize?(unit()-0.5)*10:0)),1,127);
            const double length=s.technique==GuitarTechnique::PalmMute?0.15:s.technique==GuitarTechnique::Accents?0.45:0.78;
            notes.push_back({beat,std::min(length,total-beat),guitarTuning[static_cast<std::size_t>(string)]+frets[static_cast<std::size_t>(string)],velocity,string+1});
        }
    }
    std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
    // Each string owns a channel; terminate repeated notes before the next attack.
    for(std::size_t i=0;i<notes.size();++i)for(std::size_t j=i+1;j<notes.size();++j)if(notes[i].channel==notes[j].channel&&notes[i].note==notes[j].note){
        notes[i].duration=std::min(notes[i].duration,std::max(0.001,notes[j].beat-notes[i].beat-0.001));break;
    }
    return notes;
}
}
