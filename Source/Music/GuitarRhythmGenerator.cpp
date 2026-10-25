#include "GuitarRhythmGenerator.h"
#include <functional>
namespace wingman {
std::array<int,6> guitarVoicing(const Chord& chord,int transpose){return guitarVoicing(chord,transpose,GuitarSettings{});}
std::array<int,6> guitarVoicing(const Chord& chord,int transpose,const GuitarSettings& settings,const std::array<int,6>* previous){
    validateGuitarSettings(settings);
    unsigned target=0;for(auto note:chord.notes(transpose))target|=1u<<(note%12);
    const int bass=((chord.bass>=0?chord.bass:chord.root)+transpose+120)%12;
    std::array<int,6> best{-1,-1,-1,-1,-1,-1},candidate=best;double bestScore=1e9;
    for(int position=0;position<=settings.maxFret-settings.capo;++position){
        std::function<void(int,unsigned,int,int,int)> visit=[&](int string,unsigned mask,int count,int first,int sum){
            if(string==6){
                if(count<3 || !(mask&(1u<<bass)))return;
                int missing=0;for(int pc=0;pc<12;++pc)if((target&(1u<<pc))&&!(mask&(1u<<pc)))++missing;
                double movement=0,open=0;for(std::size_t i=0;i<6;++i){if(candidate[i]==0)++open;if(previous&&candidate[i]>=0&&(*previous)[i]>=0)movement+=std::abs(candidate[i]-(*previous)[i]);}
                const double score=missing*100.0+(first!=bass?18:0)+(6-count)*3+sum*0.35+position*0.2+movement*.6-(settings.preferOpen?open*1.5:0);
                if(score<bestScore){bestScore=score;best=candidate;}return;
            }
            candidate[static_cast<std::size_t>(string)]=-1;visit(string+1,mask,count,first,sum);
            if(!(settings.stringMask&(1<<string)))return;
            for(int fret=0;fret<=settings.maxFret-settings.capo;++fret){
                if(fret && (fret<position || fret>position+3))continue;
                const int pc=(settings.tuning[static_cast<std::size_t>(string)]+settings.capo+fret)%12;
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
    if(s.bars<1||s.bars>128||s.variation<0||s.variation>3||
       (s.numerator!=3&&s.numerator!=4&&s.numerator!=6)||(s.denominator!=4&&s.denominator!=8))throw std::invalid_argument("Invalid guitar length, variation or meter");
    int selected=0;for(int i=0;i<6;++i){selected+=(s.stringMask>>i)&1;if(s.tuning[static_cast<std::size_t>(i)]<28||s.tuning[static_cast<std::size_t>(i)]>76||(i&&s.tuning[static_cast<std::size_t>(i)]<=s.tuning[static_cast<std::size_t>(i-1)]))throw std::invalid_argument("Tuning must ascend within MIDI 28-76");}
    if(s.capo<0||s.capo>12||s.maxFret<s.capo||s.maxFret>24||s.stringMask<0||s.stringMask>63||selected<3||s.pickDirection<0||s.pickDirection>2||s.articulationOverride<0||s.articulationOverride>43||(s.articulationOverride>0&&s.articulationOverride<24))throw std::invalid_argument("Invalid capo, fret, strings, direction or articulation");
    for(double value:{s.energy,s.density,s.swing,s.strumSpeed,s.palmMute})if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("Guitar parameters must be 0-1");
    if(static_cast<int>(s.technique)<0||static_cast<int>(s.technique)>7||static_cast<int>(s.style)<0||static_cast<int>(s.style)>7||static_cast<int>(s.type)<0||static_cast<int>(s.type)>4)throw std::invalid_argument("Invalid guitar selection");
}
std::vector<NoteEvent> generateGuitar(const std::vector<Chord>& harmony,int transpose,const GuitarSettings& s,const std::function<bool()>& next,const std::array<int,6>* previous){
    if(harmony.empty())throw std::invalid_argument("A guitar chord progression is required");
    validateGuitarSettings(s);
    const double barBeats=s.numerator*4.0/s.denominator,total=s.bars*barBeats,cycle=progressionBeats(harmony);
    std::mt19937 random(s.seed+static_cast<unsigned>(s.variation)*2246822519u);
    auto unit=[&]{return random()/static_cast<double>(std::mt19937::max());};
    std::vector<std::array<int,6>> shapes;for(const auto& chord:harmony){shapes.push_back(guitarVoicing(chord,transpose,s,previous));previous=&shapes.back();}
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
        if(!stroke || (step && unit()>std::min(1.0,s.density+.1+s.energy*.2)))continue;
        if(s.humanize&&s.technique==GuitarTechnique::Strumming&&unit()<.06)stroke=-stroke;
        if(s.technique==GuitarTechnique::Downstroke)stroke=1;
        if(s.technique==GuitarTechnique::Upstroke)stroke=-1;
        if(s.pickDirection==1)stroke=1;else if(s.pickDirection==2)stroke=-1;
        const double at=bar*barBeats+step*0.5+(step%2?s.swing*0.16:0);
        double chordAt=std::fmod(at,cycle);const Chord* chord=&harmony.back();
        for(const auto& item:harmony){if(chordAt<item.beats){chord=&item;break;}chordAt-=item.beats;}
        const auto frets=shapes[static_cast<std::size_t>(chord-harmony.data())];
        std::vector<int> strings;for(int string=0;string<6;++string)if(frets[static_cast<std::size_t>(string)]>=0)strings.push_back(string);
        if(stroke<0)std::reverse(strings.begin(),strings.end());
        if(stroke<0&&strings.size()>4)strings.resize(4);
        if(picked){const auto string=strings[static_cast<std::size_t>(step)%strings.size()];const int bassString=*std::min_element(strings.begin(),strings.end());strings={string};if(s.technique==GuitarTechnique::Hybrid&&step%2==0&&string!=bassString)strings.insert(strings.begin(),bassString);}
        int order=0;const double interval=(.045-.035*s.strumSpeed)*(.9-.25*s.energy);
        const bool muted=s.technique==GuitarTechnique::PalmMute||(!picked&&unit()<s.palmMute*(1-s.energy)*.25);
        for(int string:strings){
            const double jitter=s.humanize?(unit()-0.5)*std::min(.02,interval*.5):0;
            const double beat=std::clamp(at+order++*interval+jitter,bar*barBeats,total-0.01);
            const bool accent=step==0||step==6||(s.energy>.7&&unit()<s.energy*.15);
            const int velocity=std::clamp(static_cast<int>(43+s.energy*55+(accent?12:0)+(s.technique==GuitarTechnique::Accents?(accent?14:-12):0)+(stroke<0?-15:0)+(string<2&&stroke>0?6:0)+(s.humanize?(unit()-0.5)*10:0)),1,127);
            const double length=muted?.3-.24*s.palmMute:s.technique==GuitarTechnique::Accents?0.45:0.78;
            notes.push_back({beat,std::min(length,total-beat),s.tuning[static_cast<std::size_t>(string)]+s.capo+frets[static_cast<std::size_t>(string)],velocity,string+1,s.articulationOverride?s.articulationOverride:muted?29:s.technique==GuitarTechnique::Accents?31:picked?24+static_cast<int>(s.technique):stroke>0?25:26});
        }
    }
    std::stable_sort(notes.begin(),notes.end(),[](const auto& a,const auto& b){return a.beat<b.beat;});
    // A string has one fretted pitch at a time; overlap is preserved across strings.
    for(std::size_t i=0;i<notes.size();++i)for(std::size_t j=i+1;j<notes.size();++j)if(notes[i].channel==notes[j].channel){
        notes[i].duration=std::min(notes[i].duration,std::max(0.001,notes[j].beat-notes[i].beat-0.001));break;
    }
    return notes;
}
}
