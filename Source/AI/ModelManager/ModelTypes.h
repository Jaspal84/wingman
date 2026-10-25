#pragma once
#include <juce_core/juce_core.h>
namespace wingman {
struct ModelJobInfo { bool running=false; double progress=0; juce::String message="No model operation has run"; juce::File outputDirectory; };
}
