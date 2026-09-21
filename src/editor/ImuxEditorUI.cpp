#include <Geode/Geode.hpp>
#include <Geode/binding/LevelEditorLayer.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../generator/GeneratorController.hpp"

using namespace geode::prelude;

class ImuxEditorPanel {
public:
    static void open() {
        auto lel = LevelEditorLayer::get();
        auto bpm = lel ? 0.f : 0.f;
        std::string text =
            "IMUX EDITOR\n\n"
            "Audio: waiting for analysis\n"
            "BPM: --\n"
            "Beat: --\n"
            "Energy: --\n\n"
            "Generation\n"
            "Density / Sync / Difficulty are controlled by IMUX settings.\n\n"
            "The generator works through the public IMUX API and produces a deterministic LevelGraph.";
        FLAlertLayer::create("IMUX", text, "CLOSE")->show();
    }
};

struct $modify(ImuxEditorUI, EditorUI) {
    bool init(LevelEditorLayer* lel) {
        if (!EditorUI::init(lel))
            return false;

        auto addButton = [this](CCMenu* menu) {
            if (!menu || menu->getChildByID("imux-generator-button"_spr))
                return;

            auto sprite = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
            if (!sprite) {
                log::warn("ImuxHack: editor button sprite unavailable");
                return;
            }

            sprite->setScale(0.72f);
            auto button = CCMenuItemSpriteExtra::create(
                sprite, this,
                menu_selector(ImuxEditorUI::onImuxButton)
            );
            button->setID("imux-generator-button"_spr);
            menu->addChild(button);
            menu->updateLayout();
        };

        if (auto menu = this->getChildByID("toolbar-categories-menu")) {
            addButton(menu);
        } else if (auto menu = this->getChildByType<CCMenu>(0)) {
            addButton(menu);
        }

        log::info("ImuxHack: IMUX editor interface attached");
        return true;
    }

    void onImuxButton(CCObject*) {
        ImuxEditorPanel::open();
    }
};
