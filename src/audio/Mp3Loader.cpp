#define DR_MP3_IMPLEMENTATION
#include "dr_mp3.h"
#include "Mp3Loader.hpp"
#include "WavLoader.hpp"
#include <algorithm>
#include <cstdint>

namespace imux::audio {

bool loadMp3(std::string const& path, PCMBuffer& out, std::string& error) {
    drmp3 mp3;
    if (!drmp3_init_file(&mp3, path.c_str(), nullptr)) {
        error = "Unable to open MP3 file.";
        return false;
    }

    drmp3_uint64 frames = 0;
    auto* data = drmp3_open_file_and_read_pcm_frames_f32(
        path.c_str(),
        nullptr,
        &frames,
        nullptr
    );
    if (!data || frames == 0 || mp3.channels == 0 || mp3.sampleRate == 0) {
        if (data) drmp3_free(data, nullptr);
        drmp3_uninit(&mp3);
        error = "Unable to decode MP3.";
        return false;
    }

    out.sampleRate = static_cast<float>(mp3.sampleRate);
    out.mono.resize(static_cast<std::size_t>(frames));

    for (drmp3_uint64 i = 0; i < frames; ++i) {
        float sum = 0.f;
        for (drmp3_uint32 ch = 0; ch < mp3.channels; ++ch)
            sum += data[i * mp3.channels + ch];
        out.mono[static_cast<std::size_t>(i)] =
            sum / static_cast<float>(mp3.channels);
    }

    drmp3_free(data, nullptr);
    drmp3_uninit(&mp3);
    return true;
}

}
