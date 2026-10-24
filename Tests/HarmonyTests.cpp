#include "AI/ChordDetection/HarmonicAnalysis.h"
#include <iostream>
#include <stdexcept>
int count = 0;
void check(bool ok, const char* text) { ++count; if (!ok) throw std::runtime_error(text); }
std::vector<float> render(const wingman::Chord& chord, double cents=0, double seconds=2) {
    std::vector<float> audio(static_cast<std::size_t>(seconds*22050));
    for (int note : chord.notes()) for (std::size_t i=0;i<audio.size();++i) {
        const double f = 440*std::pow(2.0,(note-69+cents/100)/12);
        audio[i] += static_cast<float>(0.15*std::sin(2*3.141592653589793*f*i/22050));
    }
    return audio;
}
int main() {
    using namespace wingman;
    try {
        check(parseChord("Cmaj7^1:2")->notes() == std::vector<int>({52,55,59,60}),"seventh inversion");
        check(parseChord("Cmaj7^1:2")->beats==2,"chord duration");
        for (auto bad : {"C^3", "C:0", "C:nan", "C^1:2x", "C^x", "C:33"}) check(!parseChord(bad),"invalid extended chord");
        int correct=0,total=0;
        for (int root=0;root<12;++root) for(int quality=0;quality<8;++quality) {
            Chord chord; chord.root=root; chord.quality=static_cast<ChordQuality>(quality);
            auto audio=render(chord,15);
            const auto result=analyseHarmony(audio.data(),static_cast<int>(audio.size()),22050);
            if (!result.regions.empty()) {
                auto longest=*std::max_element(result.regions.begin(),result.regions.end(),[](const auto& a,const auto& b){return a.end-a.start<b.end-b.start;});
                const bool match = longest.chord.root==root && longest.chord.quality==chord.quality;
                if (!match) std::cout << "Mismatch " << chordName(chord) << " -> " << chordName(longest.chord) << "\n";
                correct += match;
            }
            ++total;
            check(std::abs(result.tuningCents-15)<6,"tuning estimate");
        }
        std::cout<<"Synthetic held-chord accuracy: "<<correct<<"/"<<total<<"\n";
        check(correct>=90,"held-chord accuracy threshold");
        int keyCorrect=0,keyTotal=0;
        for (int root=0;root<12;++root) for(int mode=0;mode<2;++mode) {
            std::vector<float> song;
            for (int degree : {0,5,7,0}) {
                Chord c; c.root=(root+degree)%12; c.quality=mode ? ChordQuality::Minor : ChordQuality::Major;
                auto audio=render(c,0,degree==0 ? 3 : 2); song.insert(song.end(),audio.begin(),audio.end());
            }
            auto result=analyseHarmony(song.data(),static_cast<int>(song.size()),22050);
            keyCorrect += result.keyRoot==root && result.minor==(mode==1); ++keyTotal;
        }
        std::cout<<"Synthetic cadence key accuracy: "<<keyCorrect<<"/"<<keyTotal<<"\n";
        check(keyCorrect>=22,"cadence key accuracy");
        std::vector<float> silence(44100); check(analyseHarmony(silence.data(),44100,22050).regions.empty(),"silence has no harmony");
        bool cancelled=false; try { analyseHarmony(silence.data(),44100,22050,[](double){return false;}); } catch(const std::runtime_error&) {cancelled=true;}
        check(cancelled,"analysis cancellation");
        std::cout<<count<<" harmony checks passed\n";
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<"\n";return 1;}
}
