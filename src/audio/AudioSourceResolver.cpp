#include "AudioSourceResolver.hpp"
#include "Mp3Loader.hpp"
#include <Geode/Geode.hpp>
#include <array>
#include <algorithm>
#include <cctype>
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

    std::array<std::string, 12> candidates = {
        fmt::format("Resources/music/{}.mp3", songID),
        fmt::format("Resources/music/{}.wav", songID),
        fmt::format("Resources/music/{}.MP3", songID),
        fmt::format("Resources/music/{}.WAV", songID),
        fmt::format("music/{}.mp3", songID),
        fmt::format("music/{}.wav", songID),
        fmt::format("music/{}.MP3", songID),
        fmt::format("music/{}.WAV", songID),
        fmt::format("songs/{}.mp3", songID),
        fmt::format("songs/{}.wav", songID),
        fmt::format("songs/{}.MP3", songID),
        fmt::format("songs/{}.WAV", songID)
    };

    auto* files = CCFileUtils::sharedFileUtils();
    if (!files) return source;

    for (auto const& candidate : candidates) {
        auto path = files->fullPathForFilename(candidate.c_str(), false);
        if (!path.empty() && std::filesystem::exists(path)) {
            source.path = path;
            source.displayName = candidate;
            break;
        }
    }

    return source;
}

bool AudioSourceResolver::load(
    AudioSource const& source,
    PCMBuffer& out,
    std::string& error
) {
    if (source.path.empty()) {
        error = "Audio source path is empty.";
        return false;
    }

    auto ext = std::filesystem::path(source.path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    if (ext == ".mp3")
        return loadMp3(source.path, out, error);

    if (ext == ".wav")
        return loadWav(source.path, out, error);

    error = fmt::format("Unsupported audio format '{}'.", ext);
    return false;
}

}
