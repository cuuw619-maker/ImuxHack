#include "BeatTimeline.hpp"

#include <algorithm>

namespace imux::analysis {

BeatTimeline::BeatTimeline(audio::AudioAnalysis analysis)
  : m_analysis(std::move(analysis)) {}

std::vector<BeatPosition> BeatTimeline::build() const {
    std::vector<BeatPosition> result;
    result.reserve(m_analysis.beats.size());

    for (auto const& beat : m_analysis.beats) {
        result.push_back({
            beat.time,
            std::clamp(beat.strength, 0.0f, 1.0f)
        });
    }

    return result;
}

}
