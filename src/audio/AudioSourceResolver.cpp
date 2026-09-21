#include "AudioSourceResolver.hpp"
#include "WavLoader.hpp"
#include <Geode/Geode.hpp>
#include <array>
#include <filesystem>

using namespace geode::prelude;

namespace imux::audio {

AudioSource AudioSourceResolver::fromExplicitPath(std::string const& path) {
    return {path, path.empty() ? "No audio source" : path, false};
}

AudioSource AudioSourceResolver::fromLevelSong(int songID) {
    AudioSource source;
    source.fromLevel = true;
    source.displayName = fmt::format("Level song {}", songID);

    if (songID < 0) return source;

    // Built-in/custom song locations differ between desktop and Android.
    // We deliberately probe through CCFileUtils instead of hard-coding an absolute path.
    std::array<std::string, 6> candidates = {
        fmt::format("Resources/music/{}.wav", songID),
        fmt::format("Resources/music/{}.WAV", songID),
        fmt::format("music/{}.wav", songID),
        fmt::format("music/{}.WAV", songID),
        fmt::format("songs/{}.wav", songID),
        fmt::format("songs/{}.WAV", songID)
    };

    auto* files = CCFileUtils::sharedFileUtils();
    if (!files) return source;

    for (auto const& candidate : candidates) {
        auto path = files->fullPathForFilename(candidate.c_str());
        if (!path.empty() && std::filesystem::exists(path)) {
            source.path = path;
            source.displayName = candidate;
            break;
        }
    }

    return source;
}

bool AudioSourceResolver::resolveWav(
    AudioSource const& source,
    std::vector<float>& mono,
    float& sampleRate,
    std::string& error
) {
    PCMBuffer pcm;
    if (!loadWav(source.path, pcm, error)) return false;
    mono = std::move(pcm.mono);
    sampleRate = pcm.sampleRate;
    return true;
}

}
