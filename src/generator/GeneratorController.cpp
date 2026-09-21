#include "GeneratorController.hpp"

#include <Geode/Geode.hpp>

using namespace geode::prelude;

void GeneratorController::show() {
    FLAlertLayer::create(
        "IMUX MUSIC GENERATOR",
        "The generator core is initialized. Audio analysis, beat tracking and level generation will be added in the next pipeline stage.",
        "OK"
    )->show();
}
