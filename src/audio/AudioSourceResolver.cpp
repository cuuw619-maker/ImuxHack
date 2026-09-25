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

    auto* manager = MusicDownloadManager::sharedState();
    if (manager) {
        auto downloaded = manager->pathForSong(songID);
        if (!downloaded.empty() &&
            std::filesystem::exists(std::filesystem::path(downloaded.c_str()))) {
            source.path = downloaded.c_str();
            source.displayName = source.path;
            return source;
        }
    }

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
        if (!path.empty() &&
            std::filesystem::exists(std::filesystem::path(path.c_str()))) {
            source.path = path;
            source.displayName = candidate;
            break;
        }
    }
    return source;
}

AudioSource AudioSourceResolver::fromLevelSong(GJGameLevel* level) {
    if (!level) return {};

    // GJGameLevel is the authoritative source for the actual selected song
    // filename. This covers custom songs and avoids guessing from songID.
    auto filename = level->getAudioFileName();
    if (!filename.empty()) {
        std::string name = filename.c_str();
        std::vector<std::string> candidates = {
            name,
            "Resources/music/" + name,
            "music/" + name,
            "songs/" + name
        };

        auto* files = CCFileUtils::sharedFileUtils();
        for (auto const& candidate : candidates) {
            std::string path = candidate;
            if (files) {
                auto resolved = files->fullPathForFilename(candidate.c_str(), false);
                if (!resolved.empty()) path = resolved.c_str();
            }
            if (std::filesystem::exists(std::filesystem::path(path))) {
                AudioSource source;
                source.path = path;
                source.displayName = name;
                source.fromLevel = true;
                return source;
            }
        }
    }

    return fromLevelSong(level->m_songID);
}

