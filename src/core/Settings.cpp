#include "Settings.hpp"
using namespace geode::prelude;
namespace imux::core {
Settings& settings() { static Settings s; return s; }
void load() {
    auto& s = settings();
    auto* mod = Mod::get();
    s.difficulty = mod->getSettingValue<float>("difficulty");
    s.density = mod->getSettingValue<float>("density");
    s.syncStrength = mod->getSettingValue<float>("sync-strength");
    s.movement = mod->getSettingValue<float>("movement");
    s.decoration = mod->getSettingValue<float>("decoration");
    s.beatSensitivity = mod->getSettingValue<float>("beat-sensitivity");
    s.seed = static_cast<std::uint32_t>(mod->getSettingValue<int64_t>("seed"));
    s.showBeatMarkers = mod->getSettingValue<bool>("show-beat-markers");
    s.showEnergy = mod->getSettingValue<bool>("show-energy");
    s.autoGamemode = mod->getSettingValue<bool>("auto-gamemode");
    s.autoSpeed = mod->getSettingValue<bool>("auto-speed");
}
void save() {
    auto& s = settings();
    auto* mod = Mod::get();
    mod->setSettingValue("difficulty", s.difficulty);
    mod->setSettingValue("density", s.density);
    mod->setSettingValue("sync-strength", s.syncStrength);
    mod->setSettingValue("movement", s.movement);
    mod->setSettingValue("decoration", s.decoration);
    mod->setSettingValue("beat-sensitivity", s.beatSensitivity);
    mod->setSettingValue("seed", static_cast<int64_t>(s.seed));
    mod->setSettingValue("show-beat-markers", s.showBeatMarkers);
    mod->setSettingValue("show-energy", s.showEnergy);
    mod->setSettingValue("auto-gamemode", s.autoGamemode);
    mod->setSettingValue("auto-speed", s.autoSpeed);
}
}
