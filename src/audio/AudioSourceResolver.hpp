#pragma once
#include "WavLoader.hpp"
#include <string>

namespace imux::audio {

struct AudioSource {
    std::string path;
    std::string displayName;
    bool fromLevel = false;
};

class AudioSourceResolver {
public:
    static AudioSource fromExplicitPath(std::string const& path);
    static AudioSource fromLevelSong(int songID);

    static bool load(
        AudioSource const& source,
        PCMBuffer& out,
        std::string& error
    );
};

}
