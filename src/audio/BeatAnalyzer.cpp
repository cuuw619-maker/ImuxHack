#include "BeatAnalyzer.hpp"
#include <algorithm>
#include <cmath>
namespace imux::audio {
AudioAnalysis BeatAnalyzer::analyze(std::vector<float> const& mono, float rate, float sensitivity) const {
    AudioAnalysis out;
    out.sampleRate = rate;
    if (mono.empty() || rate <= 0) return out;
    constexpr std::size_t window = 1024;
    constexpr std::size_t hop = 512;
    out.duration = static_cast<double>(mono.size()) / rate;
    std::vector<float> flux;
    float previous = 0.f;
    for (std::size_t i = 0; i + window <= mono.size(); i += hop) {
        double sum = 0.0;
        double low = 0.0, mid = 0.0, high = 0.0;
        for (std::size_t j = 0; j < window; ++j) {
            float v = std::abs(mono[i + j]);
            sum += v * v;
            double f = static_cast<double>(j) * rate / window;
            if (f < 180.0) low += v;
            else if (f < 2500.0) mid += v;
            else high += v;
        }
        float rms = static_cast<float>(std::sqrt(sum / window));
        float energy = static_cast<float>(low + mid + high) / window;
        float sf = std::max(0.f, energy - previous);
        previous = energy;
        out.frames.push_back({static_cast<double>(i) / rate, rms,
            static_cast<float>(low / window), static_cast<float>(mid / window),
            static_cast<float>(high / window), sf});
        flux.push_back(sf);
    }
    if (flux.size() < 4) return out;
    float mean = 0.f;
    for (float v : flux) mean += v;
    mean /= flux.size();
    float threshold = mean * (0.65f + sensitivity * 0.9f);
    double last = -10.0;
    for (std::size_t i = 1; i + 1 < flux.size(); ++i) {
        if (flux[i] >= threshold && flux[i] >= flux[i-1] && flux[i] >= flux[i+1]) {
            double t = out.frames[i].time;
            if (t - last >= 0.10) {
                out.beats.push_back({t, std::clamp(flux[i] / (threshold + 1e-6f), 0.f, 1.f), BeatType::Onset});
                last = t;
            }
        }
    }
    if (out.beats.size() >= 2) {
        std::vector<double> intervals;
        for (std::size_t i = 1; i < out.beats.size(); ++i) {
            double d = out.beats[i].time - out.beats[i-1].time;
            if (d >= 0.25 && d <= 1.0) intervals.push_back(d);
        }
        if (!intervals.empty()) {
            std::sort(intervals.begin(), intervals.end());
            double median = intervals[intervals.size()/2];
            out.bpm = static_cast<float>(60.0 / median);
            while (out.bpm < 70.f) out.bpm *= 2.f;
            while (out.bpm > 190.f) out.bpm *= 0.5f;
        }
    }
    for (std::size_t i = 0; i < out.beats.size(); ++i) {
        auto& b = out.beats[i];
        b.type = (i % 4 == 0) ? BeatType::Downbeat : BeatType::Beat;
    }
    return out;
}
}
