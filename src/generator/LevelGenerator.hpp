#pragma once
#include "LevelGraph.hpp"
#include "../audio/AudioTypes.hpp"
#include "../core/Settings.hpp"
namespace imux::generator {
class LevelGenerator {
public:
    LevelGraph generate(audio::AudioAnalysis const& analysis, core::Settings const& settings) const;
};
}
