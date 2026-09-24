#include "LevelGenerator.hpp"

#include <algorithm>
#include <cmath>

namespace imux::generator {

LevelGraph LevelGenerator::generate(
    audio::AudioAnalysis const& a,
    core::Settings const& s
) const {
    LevelGraph graph;
    if (a.beats.empty()) return graph;

    const float difficulty = std::clamp(s.difficulty, 0.f, 1.f);
    const float density = std::clamp(s.density, 0.1f, 1.f);
    const float sync = std::clamp(s.syncStrength, 0.f, 1.f);
    const float decoration = std::clamp(s.decoration, 0.f, 1.f);

    // One quarter beat is the base spatial unit. There is no random X/Y
    // placement: the graph is derived directly from the analyzed musical
    // timeline.
    const double beatLength = a.bpm > 1.f
        ? 60.0 / static_cast<double>(a.bpm)
        : 0.5;
    const float unitsPerSecond = 88.f + 42.f * density;
    const float baseUnit = static_cast<float>(beatLength) * unitsPerSecond;

    constexpr float groundY = 105.f;
    constexpr float safeHazardGap = 72.f;
    constexpr float safeOrbGap = 42.f;

    float x = 120.f;
    float lastHazardX = -10000.f;
    std::size_t beatIndex = 0;

    auto add = [&](ObjectType type, float xx, float yy, double time, float importance) {
        if (!std::isfinite(xx) || !std::isfinite(yy) || !std::isfinite(time))
            return;
        graph.objects.push_back({
            type,
            xx,
            yy,
            0.f,
            time,
            std::clamp(importance, 0.f, 1.f)
        });
    };

    for (auto const& beat : a.beats) {
        if (!std::isfinite(beat.time) || beat.time < 0.0)
            continue;

        const double previousTime = beatIndex == 0
            ? beat.time
            : a.beats[beatIndex - 1].time;
        const double dt = std::clamp(
            beat.time - previousTime,
            beatLength / 4.0,
            beatLength * 2.0
        );

        // Preserve exact musical spacing. syncStrength controls how strongly
        // the continuous timing is snapped to the quarter-beat grid.
        const float musicalDistance = static_cast<float>(dt) * unitsPerSecond;
        const float snappedDistance = baseUnit *
            std::max(0.25f, std::round(
                static_cast<float>(dt / (beatLength / 4.0))
            ));
        x += musicalDistance * (1.f - sync) + snappedDistance * sync;

        const float energy = std::clamp(beat.strength, 0.f, 1.f);
        const bool downbeat = beat.type == audio::BeatType::Downbeat;
        const bool accent = downbeat || energy > 0.72f;
        const bool canHazard = x - lastHazardX >= safeHazardGap;

        // Structural accents become gameplay; weaker subdivisions become
        // readable support objects. Every object remains tied to beat.time.
        if (accent && difficulty > 0.18f && canHazard) {
            add(ObjectType::Spike, x, groundY, beat.time, energy);
            lastHazardX = x;
        } else if (energy > 0.48f && difficulty > 0.28f &&
                   x - lastHazardX >= safeOrbGap) {
            add(ObjectType::Orb, x, groundY + 28.f, beat.time, energy);
            lastHazardX = x;
        } else {
            add(ObjectType::Block, x, groundY, beat.time, energy);
        }

        // Subdivisions create denser layouts without making the lethal route
        // denser. These are deterministic and still use the same beat time.
        if (density > 0.42f) {
            const float secondaryX = x + baseUnit * 0.34f;
            const bool safeSecondary = secondaryX - lastHazardX >= safeHazardGap;

            if (energy > 0.58f && safeSecondary) {
                add(ObjectType::Block, secondaryX, groundY, beat.time, energy * 0.72f);
            } else if (energy > 0.34f) {
                add(ObjectType::Block, secondaryX, groundY + 30.f,
                    beat.time, energy * 0.55f);
            }
        }

        // Strong beats get a small rhythm cluster. The player-facing route
        // stays on the ground while the upper objects provide visual and
        // optional jump targets.
        if (density > 0.65f && energy > 0.65f) {
            const float clusterX = x + baseUnit * 0.58f;
            add(ObjectType::Block, clusterX, groundY, beat.time, energy * 0.62f);

            if (difficulty > 0.45f && clusterX - lastHazardX >= safeOrbGap) {
                add(ObjectType::Orb, clusterX, groundY + 42.f,
                    beat.time, energy * 0.50f);
            }
        }

        if (decoration > 0.35f && (downbeat || energy > 0.82f)) {
            const float decoX = x + baseUnit * 0.18f;
            add(ObjectType::Decoration, decoX, groundY + 58.f,
                beat.time, energy * decoration);
        }

        ++beatIndex;
    }

    std::sort(graph.objects.begin(), graph.objects.end(),
        [](GeneratedObject const& lhs, GeneratedObject const& rhs) {
            if (lhs.time != rhs.time) return lhs.time < rhs.time;
            return lhs.x < rhs.x;
        });

    return graph;
}

}
