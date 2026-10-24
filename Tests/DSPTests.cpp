#include "DSP/Music.h"
#include <iostream>
#include <cstdlib>
static int checks = 0;
static void check(bool ok, const char* text) { ++checks; if (!ok) { std::cerr << "FAIL: " << text << '\n'; std::exit(1); } }
int main() {
    using namespace wingman;
    check(parseChord("C")->notes() == std::array<int,3>{48,52,55},"C major voicing");
    check(parseChord("Am")->notes() == std::array<int,3>{57,60,64},"A minor voicing");
    check(parseChord("Bb")->root == 10,"flat root");
    check(parseChord("F#m")->root == 6 && parseChord("F#m")->minor,"sharp minor");
    check(parseChord("Cb")->root == 11,"enharmonic root");
    for (auto text : {"", "H", "Cmaj7", "Cmgarbage", " ", "c", "C##"}) check(!parseChord(text),"invalid chord rejected");
    check(parseChord("C")->notes(8) == std::array<int,3>{56,60,63},"key transposition");
    const double rate = 44100;
    std::vector<float> samples(static_cast<std::size_t>(rate*12));
    for (int beat = 0; beat < 24; ++beat) for (int i = 0; i < 1000; ++i)
        samples[static_cast<std::size_t>(beat*22050+i)] = static_cast<float>(std::exp(-i/100.0));
    const auto bpm = detectBpm(samples.data(),static_cast<int>(samples.size()),rate);
    check(bpm && std::abs(*bpm-120)<1,"120 BPM pulse detection");
    std::fill(samples.begin(),samples.end(),0);
    check(!detectBpm(samples.data(),static_cast<int>(samples.size()),rate),"silence has no tempo");
    check(!detectBpm(samples.data(),100,rate),"short audio has no tempo");
    std::cout << checks << " DSP checks passed\n";
}
