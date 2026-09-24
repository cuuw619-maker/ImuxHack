#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace imux::integration {

struct ModInfo {
    std::string id;
    bool enabled = false;
};

// Runtime discovery through the Geode Loader API. These helpers deliberately
// have no hard dependency on a particular third-party mod.
std::vector<ModInfo> loadedMods();
bool isModLoaded(std::string_view id);

// Convenience probe for optional integrations. A consumer can use this to
// select its own adapter without linking IMUX to an unrelated mod.
std::string findFirstLoaded(std::vector<std::string_view> const& ids);

struct APICapability {
    std::string id;
    std::string role;
    bool loaded = false;
};

std::vector<APICapability> availableAPIs();

} // namespace imux::integration
