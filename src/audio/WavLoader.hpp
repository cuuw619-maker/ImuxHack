#pragma once
#include <string>
#include <vector>
namespace imux::audio {
struct PCMBuffer {
    float sampleRate = 0.f;
    std::vector<float> mono;
    double duration() const { return sampleRate > 0.f ? mono.size() / sampleRate : 0.0; }
};
bool loadWav(std::string const& path, PCMBuffer& out, std::string& error);
}
