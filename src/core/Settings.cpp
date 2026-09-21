#include "Settings.hpp"
#include <algorithm>
#include <limits>
using namespace geode::prelude;

namespace imux::core {
Settings& settings() {
    static Settings s;
    return s;
}

void load() {
    auto& s = settings();
    auto* mod = Mod::get();
    if (!mod) return;

    auto readFloat = [&](char const* key, float fallback) {
        if (!mod->hasSetting(key)) {
            log::warn("ImuxHack: setting '{}' is unavailable, using default {}", key, fallback);
            return fallback;
        }
        auto value = mod->getSettingValue<double>(key);
        if (!std::isfinite(value)) return fallback;
        return static_cast<float>(value);
    };

    s.difficulty = readFloat("difficulty", 0.50f);
    s.density = readFloat("density", 0.65f);
    s.syncStrength = readFloat("sync-strength", 0.90f);
    s.movement = readFloat("movement", 0.55f);
    s.decoration = readFloat("decoration", 0.45f);
    s.beatSensitivity = readFloat("beat-sensitivity", 0.55f);

    if (mod->hasSetting("seed"))
        s.seed = static_cast<std::uint32_t>(mod->getSettingValue<int64_t>("seed"));
    if (mod->hasSetting("show-beat-markers"))
        s.showBeatMarkers = mod->getSettingValue<bool>("show-beat-markers");
    if (mod->hasSetting("show-energy"))
        s.showEnergy = mod->getSettingValue<bool>("show-energy");
    if (mod->hasSetting("auto-gamemode"))
        s.autoGamemode = mod->getSettingValue<bool>("auto-gamemode");
    if (mod->hasSetting("auto-speed"))
        s.autoSpeed = mod->getSettingValue<bool>("auto-speed");

    s.difficulty = std::clamp(s.difficulty, 0.f, 1.f);
    s.density = std::clamp(s.density, 0.1f, 1.f);
    s.syncStrength = std::clamp(s.syncStrength, 0.f, 1.f);
    s.movement = std::clamp(s.movement, 0.f, 1.f);
    s.decoration = std::clamp(s.decoration, 0.f, 1.f);
    s.beatSensitivity = std::clamp(s.beatSensitivity, 0.1f, 1.f);
}

void save() {
    auto* mod = Mod::get();
    if (!mod) return;

    auto& s = settings();
    if (mod->hasSetting("difficulty")) mod->setSettingValue("difficulty", static_cast<double>(s.difficulty));
    if (mod->hasSetting("density")) mod->setSettingValue("density", static_cast<double>(s.density));
    if (mod->hasSetting("sync-strength")) mod->setSettingValue("sync-strength", static_cast<double>(s.syncStrength));
    if (mod->hasSetting("movement")) mod->setSettingValue("movement", static_cast<double>(s.movement));
    if (mod->hasSetting("decoration")) mod->setSettingValue("decoration", static_cast<double>(s.decoration));
    if (mod->hasSetting("beat-sensitivity")) mod->setSettingValue("beat-sensitivity", static_cast<double>(s.beatSensitivity));
    if (mod->hasSetting("seed")) mod->setSettingValue("seed", static_cast<int64_t>(s.seed));
    if (mod->hasSetting("show-beat-markers")) mod->setSettingValue("show-beat-markers", s.showBeatMarkers);
    if (mod->hasSetting("show-energy")) mod->setSettingValue("show-energy", s.showEnergy);
    if (mod->hasSetting("auto-gamemode")) mod->setSettingValue("auto-gamemode", s.autoGamemode);
    if (mod->hasSetting("auto-speed")) mod->setSettingValue("auto-speed", s.autoSpeed);
}
}
