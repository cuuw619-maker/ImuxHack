#pragma once
#include "../audio/AudioTypes.hpp"
#include "../generator/LevelGraph.hpp"

class GeneratorController {
public:
    static void show();
    static void showSettings();
    static imux::audio::AudioAnalysis analyze(std::vector<float> const& mono, float sampleRate);
    static imux::generator::LevelGraph generate(imux::audio::AudioAnalysis const& analysis);
};
