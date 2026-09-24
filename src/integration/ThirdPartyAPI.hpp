#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace imux::integration {

struct ModInfo {
    std::string id;
    bool enabled = false;
};

std::vector<ModInfo> loadedMods();
bool isModLoaded(std::string_view id);

} // namespace imux::integration
