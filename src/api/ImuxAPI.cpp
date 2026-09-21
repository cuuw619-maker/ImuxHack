#include "ImuxAPI.hpp"
#include "../audio/BeatAnalyzer.hpp"
#include "../generator/LevelGenerator.hpp"
#include <cmath>
#include <algorithm>
namespace imux {
audio::AudioAnalysis API::analyze(std::vector<float> const& mono, float rate, float sensitivity) {
    return audio::BeatAnalyzer{}.analyze(mono, rate, sensitivity);
}
generator::LevelGraph API::generate(audio::AudioAnalysis const& analysis, core::Settings const& settings) {
    return generator::LevelGenerator{}.generate(analysis, settings);
}
ValidationResult API::validate(generator::LevelGraph const& graph) {
    ValidationResult r;
    r.objects = graph.objects.size();
    double lastTime = -100.0;
    float lastX = -10000.f;
    for (auto const& o : graph.objects) {
        if (!std::isfinite(o.x) || !std::isfinite(o.y)) {
            r.playable = false;
            ++r.warnings;
            r.messages.push_back("Non-finite object coordinate.");
        }
        if (o.x < lastX) {
            r.playable = false;
            ++r.warnings;
            r.messages.push_back("Object order is not monotonic.");
        }
        if (o.time < lastTime) {
            ++r.warnings;
            r.messages.push_back("Generated event timing is not monotonic.");
        }
        lastX = o.x;
        lastTime = o.time;
    }
    return r;
}
}
