#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace imux::audio {

enum class BeatType : std::uint8_t {
    Kick,
    Snare,
    Hat,
    Bass,
    Melody,
    Onset,
    Beat,
    Downbeat
};

struct BeatEvent {
    double time = 0.0;
    float strength = 0.0f;
    BeatType type = BeatType::Onset;
};

struct AudioFrame {
    double time = 0.0;
    float rms = 0.0f;
    float low = 0.0f;
    float mid = 0.0f;
    float high = 0.0f;
    float spectralFlux = 0.0f;
};

struct AudioAnalysis {
    double duration = 0.0;
    float sampleRate = 0.0f;
    float bpm = 0.0f;
    std::vector<AudioFrame> frames;
    std::vector<BeatEvent> beats;
};

}
