#pragma once
#include "GuitarRhythmGenerator.h"
#include <functional>
namespace wingman {
struct GuitarAudio {double sampleRate=48000;std::array<std::vector<float>,2> channels;};
// Original Karplus–Strong plucked-string synthesis. Basic preview, not sampled realism.
GuitarAudio renderGuitar(const std::vector<NoteEvent>&,const GuitarSettings&,double bpm,double sampleRate,
                        const std::function<bool(double)>& continueRendering={});
}
