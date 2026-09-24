#include "ThirdPartyAPI.hpp"

#include <Geode/Geode.hpp>

namespace imux::integration {

std::vector<ModInfo> loadedMods() {
    std::vector<ModInfo> result;
    for (auto* mod : geode::Loader::get()->getAllMods()) {
        if (!mod) continue;
        result.push_back(ModInfo{
            std::string(mod->getID()),
            true
        });
    }
    return result;
}

bool isModLoaded(std::string_view id) {
    auto* mod = geode::Loader::get()->getLoadedMod(id);
    return mod != nullptr;
}

std::string findFirstLoaded(std::vector<std::string_view> const& ids) {
    for (auto id : ids) {
        if (isModLoaded(id))
            return std::string(id);
    }
    return {};
}

} // namespace imux::integration
