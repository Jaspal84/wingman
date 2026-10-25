#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <vector>

namespace wingman {
enum class ChordQuality { Major, Minor, Dominant7, Major7, Minor7, Sus2, Sus4, Diminished, Major9, Minor9, Add9, MinorAdd9, Dominant9, Dominant11, Minor11, Major13, Minor13, Dominant13, Flat9, Sharp9, Sharp11, Flat13, Major11, Major6, Minor6 };
struct Chord {
    int root = 0;
    bool minor = false;
    ChordQuality quality = ChordQuality::Major;
    int inversion = 0;
    double beats = 4;
    int bass = -1;
    std::vector<int> notes(int transpose = 0) const {
        const auto q = quality == ChordQuality::Major && minor ? ChordQuality::Minor : quality;
        std::vector<int> intervals;
        switch (q) {
            case ChordQuality::Major: intervals = {0,4,7}; break;
            case ChordQuality::Minor: intervals = {0,3,7}; break;
            case ChordQuality::Dominant7: intervals = {0,4,7,10}; break;
            case ChordQuality::Major7: intervals = {0,4,7,11}; break;
            case ChordQuality::Minor7: intervals = {0,3,7,10}; break;
            case ChordQuality::Sus2: intervals = {0,2,7}; break;
            case ChordQuality::Sus4: intervals = {0,5,7}; break;
            case ChordQuality::Diminished: intervals = {0,3,6}; break;
            case ChordQuality::Major9: intervals = {0,4,7,11,14}; break;
            case ChordQuality::Minor9: intervals = {0,3,7,10,14}; break;
            case ChordQuality::Add9: intervals = {0,4,7,14}; break;
            case ChordQuality::MinorAdd9: intervals = {0,3,7,14}; break;
            case ChordQuality::Dominant9: intervals = {0,4,7,10,14}; break;
            case ChordQuality::Dominant11: intervals = {0,4,7,10,14,17}; break;
            case ChordQuality::Minor11: intervals = {0,3,7,10,14,17}; break;
            case ChordQuality::Major13: intervals = {0,4,7,11,14,21}; break;
            case ChordQuality::Minor13: intervals = {0,3,7,10,14,21}; break;
            case ChordQuality::Dominant13: intervals = {0,4,7,10,14,21}; break;
            case ChordQuality::Flat9: intervals = {0,4,7,10,13}; break;
            case ChordQuality::Sharp9: intervals = {0,4,7,10,15}; break;
            case ChordQuality::Sharp11: intervals = {0,4,7,10,18}; break;
            case ChordQuality::Flat13: intervals = {0,4,7,10,20}; break;
            case ChordQuality::Major11: intervals = {0,4,7,11,14,17}; break;
            case ChordQuality::Major6: intervals = {0,4,7,9}; break;
            case ChordQuality::Minor6: intervals = {0,3,7,9}; break;
        }
        for (auto& note : intervals) note += 48+root+transpose;
        for (int i = 0; i < inversion && i < static_cast<int>(intervals.size()); ++i) {
            const int first = intervals.front()+12; intervals.erase(intervals.begin()); intervals.push_back(first);
        }
        if(bass>=0){
            const int bottom=36+(bass+transpose+120)%12;
            std::erase_if(intervals,[&](int note){return note%12==(bottom%12);});
            intervals.insert(intervals.begin(),bottom);
        }
        return intervals;
    }
};
inline std::optional<Chord> parseChord(const std::string& token) {
    if (token.empty()) return {};
    std::string s = token;
    Chord chord;
    try {
        if (auto p = s.find(':'); p != std::string::npos) {
            std::size_t used = 0; auto duration = s.substr(p+1); chord.beats = std::stod(duration,&used);
            if (used != duration.size() || !std::isfinite(chord.beats) || chord.beats < 0.25 || chord.beats > 32) return {};
            s.resize(p);
        }
        if (auto p = s.find('^'); p != std::string::npos) {
            const auto value = s.substr(p+1);
            if (value.size() != 1 || value[0] < '0' || value[0] > '7') return {};
            chord.inversion = value[0]-'0'; s.resize(p);
        }
    } catch (...) { return {}; }
    if (auto slash=s.find('/');slash!=std::string::npos){
        const auto bottom=s.substr(slash+1);if(bottom.empty()||bottom.size()>2)return {};
        auto at=std::string("C D EF G A B").find(bottom[0]);if(at==std::string::npos||bottom[0]==' ')return {};
        int pc=static_cast<int>(at);if(bottom.size()==2){if(bottom[1]!='#'&&bottom[1]!='b')return {};pc+=bottom[1]=='#'?1:-1;}
        chord.bass=(pc+12)%12;s.resize(slash);
    }
    if (s.empty()) return {};
    const auto index = std::string("C D EF G A B").find(s[0]);
    if (index == std::string::npos || s[0] == ' ') return {};
    int root = static_cast<int>(index); std::size_t p = 1;
    if (p < s.size() && (s[p] == '#' || s[p] == 'b')) root += s[p++] == '#' ? 1 : -1;
    chord.root = (root+12)%12;
    auto suffix = s.substr(p);
    const auto open=suffix.find('('),close=suffix.find(')');
    if((open==std::string::npos)!=(close==std::string::npos))return {};
    if(open!=std::string::npos&&(close!=suffix.size()-1||close<open||suffix.find('(',open+1)!=std::string::npos||suffix.find(')',close+1)!=std::string::npos))return {};
    suffix.erase(std::remove(suffix.begin(),suffix.end(),'('),suffix.end());suffix.erase(std::remove(suffix.begin(),suffix.end(),')'),suffix.end());
    if (suffix.empty()) chord.quality = ChordQuality::Major;
    else if (suffix == "m") chord.quality = ChordQuality::Minor;
    else if (suffix == "7") chord.quality = ChordQuality::Dominant7;
    else if (suffix == "maj7") chord.quality = ChordQuality::Major7;
    else if (suffix == "m7") chord.quality = ChordQuality::Minor7;
    else if (suffix == "sus2") chord.quality = ChordQuality::Sus2;
    else if (suffix == "sus4") chord.quality = ChordQuality::Sus4;
    else if (suffix == "dim") chord.quality = ChordQuality::Diminished;
    else if(suffix=="maj9")chord.quality=ChordQuality::Major9;
    else if(suffix=="m9")chord.quality=ChordQuality::Minor9;
    else if(suffix=="add9")chord.quality=ChordQuality::Add9;
    else if(suffix=="madd9")chord.quality=ChordQuality::MinorAdd9;
    else if(suffix=="9")chord.quality=ChordQuality::Dominant9;
    else if(suffix=="11")chord.quality=ChordQuality::Dominant11;
    else if(suffix=="m11")chord.quality=ChordQuality::Minor11;
    else if(suffix=="maj11")chord.quality=ChordQuality::Major11;
    else if(suffix=="13")chord.quality=ChordQuality::Dominant13;
    else if(suffix=="maj13")chord.quality=ChordQuality::Major13;
    else if(suffix=="m13")chord.quality=ChordQuality::Minor13;
    else if(suffix=="7b9")chord.quality=ChordQuality::Flat9;
    else if(suffix=="7#9")chord.quality=ChordQuality::Sharp9;
    else if(suffix=="7#11")chord.quality=ChordQuality::Sharp11;
    else if(suffix=="7b13")chord.quality=ChordQuality::Flat13;
    else if(suffix=="6")chord.quality=ChordQuality::Major6;
    else if(suffix=="m6")chord.quality=ChordQuality::Minor6;
    else return {};
    chord.minor = chord.quality == ChordQuality::Minor || chord.quality == ChordQuality::Minor7 || chord.quality==ChordQuality::Minor9 || chord.quality==ChordQuality::MinorAdd9 || chord.quality==ChordQuality::Minor11 || chord.quality==ChordQuality::Minor13 || chord.quality==ChordQuality::Minor6;
    if (chord.inversion >= static_cast<int>(chord.notes().size())) return {};
    return chord;
}
inline double progressionBeats(const std::vector<Chord>& chords) { double total = 0; for (const auto& c : chords) total += c.beats; return total; }
inline std::string chordName(const Chord& chord) {
    const std::array<std::string,12> roots{"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
    const std::array<std::string,25> suffix{"","m","7","maj7","m7","sus2","sus4","dim","maj9","m9","add9","madd9","9","11","m11","maj13","m13","13","7b9","7#9","7#11","7b13","maj11","6","m6"};
    return roots[static_cast<std::size_t>(chord.root)]+suffix[static_cast<std::size_t>(chord.quality)]+(chord.bass>=0?"/"+roots[static_cast<std::size_t>(chord.bass)]:"");
}
// Onset-energy autocorrelation. Confidence reflects periodicity, not harmonic analysis.
inline std::optional<double> detectBpm(const float* samples, int count, double sampleRate) {
    if (!samples || !std::isfinite(sampleRate) || sampleRate <= 0 || count < sampleRate * 3) return {};
    const int hop = std::max(1, static_cast<int>(sampleRate / 200.0));
    std::vector<double> onset;
    double previous = 0;
    for (int i = 0; i + hop <= count; i += hop) {
        double energy = 0;
        for (int j = 0; j < hop; ++j) energy += static_cast<double>(samples[i+j]) * samples[i+j];
        energy = std::sqrt(energy / hop);
        onset.push_back(std::max(0.0, energy - previous));
        previous = energy;
    }
    double power = 0;
    for (auto x : onset) power += x * x;
    if (power < 1.0e-8) return {};
    const double rate = sampleRate / hop;
    int bestLag = 0;
    double best = 0;
    for (int lag = static_cast<int>(rate * 60 / 180); lag <= static_cast<int>(rate * 60 / 60); ++lag) {
        double sum = 0;
        for (std::size_t i = static_cast<std::size_t>(lag); i < onset.size(); ++i) sum += onset[i] * onset[i-static_cast<std::size_t>(lag)];
        if (sum > best) { best = sum; bestLag = lag; }
    }
    if (bestLag == 0 || best / power < 0.12) return {};
    return rate * 60 / bestLag;
}
}
