#pragma once

#include "../audio/AudioTypes.hpp"
#include "../generator/LevelGraph.hpp"
#include "../core/Settings.hpp"
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace imux {

struct ValidationResult {
    bool playable = true;
    std::size_t objects = 0;
    std::size_t warnings = 0;
    std::vector<std::string> messages;
};

struct GenerationRequest {
    audio::AudioAnalysis const* analysis = nullptr;
    core::Settings const* settings = nullptr;
};

struct GenerationResult {
    generator::LevelGraph graph;
    ValidationResult validation;
};

enum class EventType {
    AnalysisStarted,
    AnalysisFinished,
    GenerationStarted,
    GenerationFinished,
    ValidationFinished
};

using EventCallback = std::function<void(EventType)>;

class API {
public:
    static audio::AudioAnalysis analyze(
        std::vector<float> const& mono,
        float sampleRate,
        float sensitivity = 0.55f
    );

    static GenerationResult generate(GenerationRequest const& request);

    static generator::LevelGraph generate(
        audio::AudioAnalysis const& analysis,
        core::Settings const& settings
    );

    static ValidationResult validate(generator::LevelGraph const& graph);

    static void setEventCallback(EventCallback callback);
    static void clearEventCallback();
};

}
