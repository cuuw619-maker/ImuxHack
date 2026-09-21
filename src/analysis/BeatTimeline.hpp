#pragma once

#include "../audio/AudioTypes.hpp"
#include <vector>

namespace imux::analysis {

struct BeatPosition {
    double time = 0.0;
    float strength = 0.0f;
};

class BeatTimeline {
public:
    explicit BeatTimeline(audio::AudioAnalysis analysis);
    std::vector<BeatPosition> build() const;

private:
    audio::AudioAnalysis m_analysis;
};

}
