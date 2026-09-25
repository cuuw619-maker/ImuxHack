#pragma once

#include <Geode/loader/Dispatch.hpp>
#include <optional>

namespace nwo5::uiscaling {
    #undef MY_MOD_ID
    #define MY_MOD_ID "nwo5.ui-scaling"

    namespace impl {
        inline geode::Result<float> getEditorUIScale()
        GEODE_EVENT_EXPORT(&getEditorUIScale, ());
    }

    namespace EditorUI {
        struct Changed final : geode::Event<Changed, bool(float pScale)> {
            using Event::Event;
        };

        inline float getScale() {
            return impl::getEditorUIScale().unwrapOr(1.0f);
        }

        inline void setScale(float pScale, bool pVanillaPositioning = true,
            bool pScaleToolbar = true, bool pUseSafeArea = false,
            std::optional<float> pCustomSafeArea = std::nullopt)
        GEODE_EVENT_EXPORT_NORES(&setScale,
            (pScale, pVanillaPositioning, pScaleToolbar, pUseSafeArea, pCustomSafeArea));
    }

    #undef MY_MOD_ID
}
