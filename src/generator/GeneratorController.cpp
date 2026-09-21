#include "GeneratorController.hpp"
#include "LevelGenerator.hpp"
#include "../audio/BeatAnalyzer.hpp"
#include "../core/Settings.hpp"
#include <Geode/Geode.hpp>

using namespace geode::prelude;

void GeneratorController::show() {
    auto alert = FLAlertLayer::create(
        "IMUX GENERATOR",
        "Beat analysis and deterministic LevelGraph generation are available. Audio-file decoding and editor object insertion are the next integration layer.",
        "OK"
    );
    alert->show();
}

void GeneratorController::showSettings() {
    FLAlertLayer::create(
        "IMUX SETTINGS",
        "Use Geode mod settings to configure difficulty, density, sync strength, movement, decoration, beat sensitivity and seed.",
        "OK"
    )->show();
}

imux::audio::AudioAnalysis GeneratorController::analyze(std::vector<float> const& mono, float sampleRate) {
    imux::core::load();
    return imux::audio::BeatAnalyzer{}.analyze(mono, sampleRate, imux::core::settings().beatSensitivity);
}

imux::generator::LevelGraph GeneratorController::generate(imux::audio::AudioAnalysis const& analysis) {
    imux::core::load();
    return imux::generator::LevelGenerator{}.generate(analysis, imux::core::settings());
}
