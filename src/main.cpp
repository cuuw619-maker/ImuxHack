#include <Geode/Geode.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include "generator/GeneratorController.hpp"

using namespace geode::prelude;

class $modify(ImuxMenuLayer, MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) {
            return false;
        }

        auto menu = this->getChildByID("bottom-menu");
        if (!menu) {
            log::warn("ImuxHack: bottom-menu was not found");
            return true;
        }

        auto button = CCMenuItemSpriteExtra::create(
            CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png"),
            this,
            menu_selector(ImuxMenuLayer::openGenerator)
        );

        button->setID("generator-button"_spr);
        menu->addChild(button);
        menu->updateLayout();

        log::info("ImuxHack loaded: music-driven generator initialized");
        return true;
    }

    void openGenerator(CCObject*) {
        GeneratorController::show();
    }
};
