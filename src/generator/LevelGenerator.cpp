#include "LevelGenerator.hpp"
#include <algorithm>
#include <cmath>
#include <random>

namespace imux::generator {

LevelGraph LevelGenerator::generate(audio::AudioAnalysis const& a, core::Settings const& s) const {
    LevelGraph graph;
    if (a.beats.empty()) return graph;

    std::mt19937 rng(s.seed);
    std::uniform_real_distribution<float> jitter(-1.f, 1.f);

    const float difficulty = std::clamp(s.difficulty, 0.f, 1.f);
    const float density = std::clamp(s.density, 0.1f, 1.f);
    const float sync = std::clamp(s.syncStrength, 0.f, 1.f);
    const float movement = std::clamp(s.movement, 0.f, 1.f);

    double previousTime = -1.0;
    float x = 120.f;
    float laneY = 105.f;
    std::size_t index = 0;

    auto add = [&](ObjectType type, float xx, float yy, double time, float importance) {
        if (!std::isfinite(xx) || !std::isfinite(yy) || !std::isfinite(time)) return;
        graph.objects.push_back({type, xx, yy, 0.f, time, std::clamp(importance, 0.f, 1.f)});
    };

    for (auto const& beat : a.beats) {
        if (!std::isfinite(beat.time) || beat.time < 0.0) continue;

        const double dt = previousTime < 0.0 ? 0.5 : beat.time - previousTime;
        previousTime = beat.time;

        // Convert musical time into horizontal distance. Strong sync keeps
        // consecutive events closer to a stable beat grid instead of using
        // arbitrary sine-wave placement.
        const float beatDistance = static_cast<float>(
            std::clamp(dt, 0.08, 1.5) * (38.f + 22.f * density)
        );
        x += beatDistance * (0.65f + 0.35f * sync);

        const float energy = std::clamp(beat.strength, 0.f, 1.f);
        const float movementAmount =
            (0.15f + movement * 0.85f) * (0.35f + energy * 0.65f);

        if (index > 0) {
            const int direction = (index % 2 == 0) ? 1 : -1;
            laneY += direction * movementAmount * (8.f + 16.f * difficulty);
            laneY = std::clamp(laneY, 75.f, 135.f);
        }

        // Downbeats and strong onsets form the primary structural anchors.
        const bool structural = beat.type == audio::BeatType::Downbeat || energy > 0.78f;

        if (structural) {
            add(ObjectType::Spike, x, laneY, beat.time, energy);
        } else if (energy > 0.52f && difficulty > 0.22f) {
            add(ObjectType::Orb, x, laneY + 22.f + difficulty * 8.f, beat.time, energy);
        } else {
            add(ObjectType::Block, x, laneY - 18.f, beat.time, energy);
        }

        // Add a secondary object only for dense, strong events. This creates
        // rhythm subdivisions without placing something on every sample.
        if (density > 0.72f && energy > 0.68f && (index % 2 == 0)) {
            const float offset = 22.f + jitter(rng) * 5.f;
            add(ObjectType::Block, x + offset, laneY - 18.f, beat.time, energy * 0.7f);
        }

        if (s.decoration > 0.25f && energy > 0.82f && index % 4 == 0) {
            add(ObjectType::Decoration, x, laneY + 42.f, beat.time, energy * s.decoration);
        }

        ++index;
    }

    return graph;
}

}
