#pragma once
#include "AudioTypes.hpp"
#include <vector>
namespace imux::audio {
class BeatAnalyzer {
public:
    AudioAnalysis analyze(std::vector<float> const& mono, float sampleRate, float sensitivity = 0.55f) const;
};
}
