#include "ImuxAPI.hpp"
#include "../audio/BeatAnalyzer.hpp"
#include "../generator/LevelGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <utility>

namespace imux {
namespace {
std::mutex g_callbackMutex;
EventCallback g_callback;

void emit(EventType event) {
    EventCallback callback;
    {
        std::lock_guard lock(g_callbackMutex);
        callback = g_callback;
    }
    if (callback) callback(event);
    ImuxGenerationEvent(event).send();
}
}

audio::AudioAnalysis API::analyze(
    std::vector<float> const& mono,
    float rate,
    float sensitivity
) {
    emit(EventType::AnalysisStarted);
    auto result = audio::BeatAnalyzer{}.analyze(mono, rate, sensitivity);
    emit(EventType::AnalysisFinished);
    return result;
}

GenerationResult API::generate(GenerationRequest const& request) {
    GenerationResult result;
    if (!request.analysis || !request.settings) {
        result.validation.playable = false;
        result.validation.warnings = 1;
        result.validation.messages.push_back("GenerationRequest requires analysis and settings.");
        return result;
    }

    emit(EventType::GenerationStarted);
    result.graph = generator::LevelGenerator{}.generate(*request.analysis, *request.settings);
    emit(EventType::GenerationFinished);

    result.validation = validate(result.graph);
    emit(EventType::ValidationFinished);
    return result;
}

generator::LevelGraph API::generate(
    audio::AudioAnalysis const& analysis,
    core::Settings const& settings
) {
    auto result = generate(GenerationRequest{&analysis, &settings});
    return std::move(result.graph);
}

ValidationResult API::validate(generator::LevelGraph const& graph) {
    ValidationResult r;
    r.objects = graph.objects.size();

    double lastTime = -1.0;
    float lastX = -std::numeric_limits<float>::infinity();

    for (std::size_t i = 0; i < graph.objects.size(); ++i) {
        auto const& o = graph.objects[i];

        if (!std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.time)) {
            r.playable = false;
            ++r.warnings;
            r.messages.push_back("Non-finite generated object.");
            continue;
        }

        if (o.x < lastX) {
            r.playable = false;
            ++r.warnings;
            r.messages.push_back("Object order is not monotonic.");
        }

        if (o.time + 1e-6 < lastTime) {
            r.playable = false;
            ++r.warnings;
            r.messages.push_back("Event timing is not monotonic.");
        }

        if (i > 0 && o.x - lastX < 8.f) {
            ++r.warnings;
            r.messages.push_back("Objects are too close horizontally.");
        }

        // Keep lethal objects on the supported ground route and leave enough
        // horizontal room for a conservative normal Geometry Dash jump.
        if (o.type == generator::ObjectType::Spike) {
            for (std::size_t j = i; j-- > 0;) {
                auto const& previous = graph.objects[j];
                if (previous.type != generator::ObjectType::Spike)
                    continue;

                const float gap = o.x - previous.x;
                if (gap < 70.f) {
                    r.playable = false;
                    ++r.warnings;
                    r.messages.push_back("Spike spacing is below the safe jump interval.");
                }
                break;
            }

            if (o.y < 95.f || o.y > 115.f) {
                r.playable = false;
                ++r.warnings;
                r.messages.push_back("Spike is outside the supported ground route.");
            }
        }

        lastX = o.x;
        lastTime = o.time;
    }

    return r;
}

void API::setEventCallback(EventCallback callback) {
    std::lock_guard lock(g_callbackMutex);
    g_callback = std::move(callback);
}

void API::clearEventCallback() {
    std::lock_guard lock(g_callbackMutex);
    g_callback = nullptr;
}
}
