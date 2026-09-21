#pragma once
#include <string>
#include <vector>

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
    static bool resolveWav(AudioSource const& source, std::vector<float>& mono, float& sampleRate, std::string& error);
};

}
