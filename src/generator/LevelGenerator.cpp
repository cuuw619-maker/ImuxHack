#include "LevelGenerator.hpp"
#include <cmath>
namespace imux::generator {
LevelGraph LevelGenerator::generate(audio::AudioAnalysis const& a, core::Settings const& s) const {
    LevelGraph graph;
    float x = 0.f;
    const float spacing = 35.f - s.density * 12.f;
    for (auto const& beat : a.beats) {
        float strength = beat.strength;
        float y = 105.f + std::sin(static_cast<float>(beat.time * 2.1)) * 28.f * s.movement;
        auto add = [&](ObjectType type, float importance, float yy) {
            graph.objects.push_back({type, x, yy, 0.f, beat.time, importance});
        };
        if (beat.type == audio::BeatType::Downbeat || strength > 0.8f)
            add(ObjectType::Spike, strength, y);
        else if (strength > 0.45f)
            add(ObjectType::Orb, strength, y + 25.f * s.difficulty);
        else
            add(ObjectType::Block, strength, y - 20.f);
        if (s.decoration > 0.1f && strength > 0.65f)
            add(ObjectType::Decoration, strength * s.decoration, y + 45.f);
        x += spacing + (1.f - strength) * 8.f;
    }
    return graph;
}
}
