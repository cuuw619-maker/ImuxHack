#pragma once
#include <Geode/Geode.hpp>
#include <cstdint>

namespace imux::core {
struct Settings {
    float difficulty = 0.50f;
    float density = 0.65f;
    float syncStrength = 0.90f;
    float movement = 0.55f;
    float decoration = 0.45f;
    float beatSensitivity = 0.55f;
    std::uint32_t seed = 1337;
    bool showBeatMarkers = true;
    bool showEnergy = true;
    bool autoGamemode = true;
    bool autoSpeed = true;
};
Settings& settings();
void load();
void save();
}
