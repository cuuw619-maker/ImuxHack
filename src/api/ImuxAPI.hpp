#pragma once
#include "../audio/AudioTypes.hpp"
#include "../generator/LevelGraph.hpp"
#include "../core/Settings.hpp"
#include <vector>

namespace imux {
struct ValidationResult {
    bool playable = true;
    std::size_t objects = 0;
    std::size_t warnings = 0;
    std::vector<std::string> messages;
};

class API {
public:
    static audio::AudioAnalysis analyze(std::vector<float> const& mono, float sampleRate, float sensitivity = 0.55f);
    static generator::LevelGraph generate(audio::AudioAnalysis const& analysis, core::Settings const& settings);
    static ValidationResult validate(generator::LevelGraph const& graph);
};
}
