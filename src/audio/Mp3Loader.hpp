#pragma once
#include <string>
#include <vector>

namespace imux::audio {
struct PCMBuffer;
bool loadMp3(std::string const& path, PCMBuffer& out, std::string& error);
}
