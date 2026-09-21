#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../core/Settings.hpp"
using namespace geode::prelude;

class ImuxEditorPanel final : public FLAlertLayer {
protected:
    bool init() {
        if (!FLAlertLayer::init(
            nullptr,
            "IMUX GENERATOR",
            "",
            "CLOSE",
            "",
            260.f,
            false,
            190.f,
            1.f
        )) return false;

        auto title = CCLabelBMFont::create("IMUX GENERATOR", "goldFont.fnt");
        title->setScale(.65f);
        title->setPosition({130.f, 162.f});
        m_mainLayer->addChild(title);

        auto info = CCLabelBMFont::create(
            "Beat-driven generation core\nAudio -> beats -> LevelGraph",
            "bigFont.fnt"
        );
        info->setAlignment(kCCTextAlignmentCenter);
        info->setScale(.38f);
        info->setPosition({130.f, 128.f});
        m_mainLayer->addChild(info);

        auto add = [&](char const* text, SEL_MenuHandler sel, float y) {
            auto sprite = ButtonSprite::create(
                text,
                120,
                true,
                "goldFont.fnt",
                "GJ_button_01.png",
                0.f,
                1.f
            );
            auto button = CCMenuItemSpriteExtra::create(sprite, this, sel);
            button->setPosition({130.f, y});
            m_buttonMenu->addChild(button);
        };

        add("GENERATE", menu_selector(ImuxEditorPanel::onGenerate), 82.f);
        add("SETTINGS", menu_selector(ImuxEditorPanel::onSettings), 48.f);

        return true;
    }

public:
    static ImuxEditorPanel* create() {
        auto result = new ImuxEditorPanel();
        if (result && result->init()) {
            result->autorelease();
            return result;
        }
        CC_SAFE_DELETE(result);
        return nullptr;
    }

    void onGenerate(CCObject*) {
        auto const& s = imux::core::settings();
        auto message = fmt::format(
            "Generator ready\nDifficulty: {:.2f}\nDensity: {:.2f}\nSync: {:.2f}\nSeed: {}",
            s.difficulty,
            s.density,
            s.syncStrength,
            s.seed
        );
        FLAlertLayer::create("IMUX", message.c_str(), "OK")->show();
    }

    void onSettings(CCObject*) {
        FLAlertLayer::create(
            "IMUX SETTINGS",
            "Use Geode mod settings to configure generation.",
            "OK"
        )->show();
    }
};

struct $modify(ImuxEditorUI, EditorUI) {
    bool init(LevelEditorLayer* levelEditor) {
        if (!EditorUI::init(levelEditor)) return false;

        imux::core::load();

        auto menu = this->getChildByID("toolbar-categories-menu");
        if (!menu) {
            menu = this->getChildByType<CCMenu>(0);
        }

        if (!menu || menu->getChildByID("imux-generator-button")) {
            return true;
        }

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        if (!sprite) {
            return true;
        }

        sprite->setScale(.72f);

        auto button = CCMenuItemSpriteExtra::create(
            sprite,
            this,
            menu_selector(ImuxEditorUI::onImuxButton)
        );
        button->setID("imux-generator-button");

        menu->addChild(button);
        menu->updateLayout();

        return true;
    }

    void onImuxButton(CCObject*) {
        if (auto panel = ImuxEditorPanel::create()) {
            panel->show();
        }
    }
};
