#include "BeatAnalyzer.hpp"

#include <algorithm>
#include <cmath>
#include <complex>
#include <numeric>
#include <vector>

namespace imux::audio {
namespace {

void fft(std::vector<std::complex<float>>& a) {
    const std::size_t n = a.size();
    for (std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap(a[i], a[j]);
    }

    for (std::size_t len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * static_cast<float>(M_PI) /
            static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (std::size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.f, 0.f);
            for (std::size_t j = 0; j < len / 2; ++j) {
                const auto u = a[i + j];
                const auto v = a[i + j + len / 2] * w;
                a[i + j] = u + v;
                a[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

float median(std::vector<float> values) {
    if (values.empty()) return 0.f;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

}

AudioAnalysis BeatAnalyzer::analyze(
    std::vector<float> const& mono,
    float rate,
    float sensitivity
) const {
    AudioAnalysis out;
    out.sampleRate = rate;
    if (mono.empty() || rate <= 0.f) return out;

    constexpr std::size_t window = 1024;
    constexpr std::size_t hop = 256;
    constexpr float minBeatGap = 0.10f;
    out.duration = static_cast<double>(mono.size()) / rate;

    std::vector<float> flux;
    flux.reserve(mono.size() / hop);

    std::vector<float> previousSpectrum(window / 2 + 1, 0.f);
    std::vector<std::complex<float>> spectrum(window);

    for (std::size_t start = 0; start + window <= mono.size(); start += hop) {
        float rmsSum = 0.f;
        float low = 0.f, mid = 0.f, high = 0.f;
        std::fill(spectrum.begin(), spectrum.end(), std::complex<float>(0.f, 0.f));

        for (std::size_t j = 0; j < window; ++j) {
            const float phase = 2.f * static_cast<float>(M_PI) *
                static_cast<float>(j) / static_cast<float>(window - 1);
            const float hann = 0.5f * (1.f - std::cos(phase));
            const float sample = mono[start + j];
            rmsSum += sample * sample;
            spectrum[j] = std::complex<float>(sample * hann, 0.f);
        }

        fft(spectrum);

        float spectralFlux = 0.f;
        for (std::size_t k = 0; k <= window / 2; ++k) {
            const float magnitude = std::abs(spectrum[k]);
            spectralFlux += std::max(0.f, magnitude - previousSpectrum[k]);
            previousSpectrum[k] = magnitude;

            const float hz = static_cast<float>(k) * rate /
                static_cast<float>(window);
            if (hz < 180.f) low += magnitude;
            else if (hz < 2500.f) mid += magnitude;
            else high += magnitude;
        }

        const float scale = static_cast<float>(window / 2);
        out.frames.push_back({
            static_cast<double>(start) / rate,
            std::sqrt(rmsSum / static_cast<float>(window)),
            low / scale,
            mid / scale,
            high / scale,
            spectralFlux / scale
        });
        flux.push_back(spectralFlux / scale);
    }

    if (flux.size() < 8) return out;

    // Adaptive onset threshold: global statistics are combined with a
    // short local median so a quiet verse does not disappear under a loud
    // chorus.
    const float globalMedian = median(flux);
    std::vector<float> deviations;
    deviations.reserve(flux.size());
    for (float v : flux) deviations.push_back(std::abs(v - globalMedian));
    const float mad = median(deviations);
    const float sensitivityScale = 1.25f - 0.75f *
        std::clamp(sensitivity, 0.f, 1.f);

    std::vector<BeatEvent> onsets;
    double last = -10.0;
    for (std::size_t i = 2; i + 2 < flux.size(); ++i) {
        const std::size_t begin = i > 12 ? i - 12 : 0;
        const std::size_t end = std::min(flux.size(), i + 13);
        std::vector<float> local(flux.begin() + begin, flux.begin() + end);
        const float localMedian = median(std::move(local));
        const float threshold = localMedian +
            std::max(0.00001f, mad * sensitivityScale);

        if (flux[i] < threshold ||
            flux[i] < flux[i - 1] ||
            flux[i] < flux[i + 1])
            continue;

        const double time = out.frames[i].time;
        if (time - last < minBeatGap) continue;

        const float strength = std::clamp(
            (flux[i] - threshold) / (mad + 1e-6f), 0.f, 1.f
        );
        onsets.push_back({time, strength, BeatType::Onset});
        last = time;
    }

    if (onsets.size() < 2) return out;

    // Estimate tempo from onset intervals, then quantize onsets to the
    // nearest sixteenth-note grid. This makes generated X positions follow
    // musical time instead of a random spatial distribution.
    std::vector<double> intervals;
    for (std::size_t i = 1; i < onsets.size(); ++i) {
        const double d = onsets[i].time - onsets[i - 1].time;
        if (d >= 0.25 && d <= 1.0) intervals.push_back(d);
    }

    double beatLength = intervals.empty() ? 0.5 :
        static_cast<double>(median(
            std::vector<float>(intervals.begin(), intervals.end())
        ));
    float bpm = static_cast<float>(60.0 / std::max(0.25, beatLength));
    while (bpm < 70.f) {
        bpm *= 2.f;
        beatLength *= 0.5;
    }
    while (bpm > 190.f) {
        bpm *= 0.5f;
        beatLength *= 2.0;
    }
    out.bpm = bpm;

    const double subdivision = beatLength / 4.0;
    const double origin = onsets.front().time;
    std::vector<BeatEvent> quantized;
    quantized.reserve(onsets.size() + static_cast<std::size_t>(
        out.duration / subdivision
    ));

    for (auto const& onset : onsets) {
        const double step = std::round((onset.time - origin) / subdivision);
        const double snapped = std::max(0.0, origin + step * subdivision);

        if (!quantized.empty() &&
            std::abs(quantized.back().time - snapped) < subdivision * 0.35) {
            quantized.back().strength =
                std::max(quantized.back().strength, onset.strength);
            continue;
        }

        quantized.push_back({
            snapped,
            onset.strength,
            BeatType::Beat
        });
    }

    // Fill weak gaps on the musical grid using nearby frame energy. This
    // gives the generator a stable rhythmic skeleton while preserving the
    // detected accents for stronger gameplay objects.
    for (double t = origin; t < out.duration; t += subdivision) {
        auto it = std::lower_bound(
            out.frames.begin(), out.frames.end(), t,
            [](AudioFrame const& frame, double value) {
                return frame.time < value;
            }
        );
        if (it == out.frames.end()) break;

        const std::size_t frameIndex = static_cast<std::size_t>(
            std::distance(out.frames.begin(), it)
        );
        const float energy = std::clamp(
            it->spectralFlux / (globalMedian + mad + 1e-5f), 0.f, 1.f
        );

        bool exists = false;
        for (auto const& event : quantized) {
            if (std::abs(event.time - t) < subdivision * 0.30) {
                exists = true;
                break;
            }
        }
        if (!exists && energy > 0.18f) {
            quantized.push_back({t, energy, BeatType::Beat});
        }
        (void)frameIndex;
    }

    std::sort(quantized.begin(), quantized.end(),
        [](BeatEvent const& a, BeatEvent const& b) {
            return a.time < b.time;
        });

    if (quantized.empty()) return out;

    for (std::size_t i = 0; i < quantized.size(); ++i) {
        quantized[i].type = (i % 16 == 0)
            ? BeatType::Downbeat
            : ((i % 4 == 0) ? BeatType::Beat : BeatType::Onset);
    }

    out.beats = std::move(quantized);
    return out;
}

}
