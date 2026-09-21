#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../core/Settings.hpp"
using namespace geode::prelude;

class ImuxEditorPanel final : public FLAlertLayer {
protected:
    bool init() {
        if (!FLAlertLayer::init(260.f, 190.f, "GJ_square01.png")) return false;
        auto title = CCLabelBMFont::create("IMUX GENERATOR", "goldFont.fnt");
        title->setScale(.65f); title->setPosition({130.f,162.f}); m_mainLayer->addChild(title);
        auto info = CCLabelBMFont::create("Beat-driven generation core\nAudio -> beats -> LevelGraph", "bigFont.fnt");
        info->setAlignment(kCCTextAlignmentCenter); info->setScale(.38f); info->setPosition({130.f,128.f}); m_mainLayer->addChild(info);
        auto add = [&](char const* text, SEL_MenuHandler sel, float y) {
            auto s = ButtonSprite::create(text, 120, true, "goldFont.fnt", "GJ_button_01.png", 0.f, 1.f);
            auto b = CCMenuItemSpriteExtra::create(s, this, sel); b->setPosition({130.f,y}); m_buttonMenu->addChild(b);
        };
        add("GENERATE", menu_selector(ImuxEditorPanel::onGenerate),82.f);
        add("SETTINGS", menu_selector(ImuxEditorPanel::onSettings),48.f);
        add("CLOSE", menu_selector(ImuxEditorPanel::onClose),14.f);
        return true;
    }
public:
    static ImuxEditorPanel* create() {
        auto r=new ImuxEditorPanel();
        if (r && r->init()) { r->autorelease(); return r; }
        CC_SAFE_DELETE(r); return nullptr;
    }
    void onGenerate(CCObject*) {
        auto const& s=imux::core::settings();
        auto msg=fmt::format("Generator ready\nDifficulty: {:.2f}\nDensity: {:.2f}\nSync: {:.2f}\nSeed: {}",s.difficulty,s.density,s.syncStrength,s.seed);
        FLAlertLayer::create("IMUX",msg.c_str(),"OK")->show();
    }
    void onSettings(CCObject*) { FLAlertLayer::create("IMUX SETTINGS","Use Geode mod settings to configure generation.","OK")->show(); }
    void onClose(CCObject*) { this->keyBackClicked(); }
};

struct $modify(ImuxEditorUI, EditorUI) {
    bool init(LevelEditorLayer* levelEditor) {
        if (!EditorUI::init(levelEditor)) return false;
        imux::core::load();
        auto menu=this->getChildByID("toolbar-categories-menu");
        if (!menu) menu=this->getChildByType<CCMenu>(0);
        if (!menu || menu->getChildByID("imux-generator-button")) return true;
        auto sprite=CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        if (!sprite) return true;
        sprite->setScale(.72f);
        auto button=CCMenuItemSpriteExtra::create(sprite,this,menu_selector(ImuxEditorUI::onImuxButton));
        button->setID("imux-generator-button");
        menu->addChild(button); menu->updateLayout();
        return true;
    }
    void onImuxButton(CCObject*) { ImuxEditorPanel::create()->show(); }
};