#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../audio/WavLoader.hpp"
#include "../api/ImuxAPI.hpp"
#include "../core/Settings.hpp"

using namespace geode::prelude;

class ImuxEditorPanel final : public FLAlertLayer {
    LevelEditorLayer* m_levelEditor = nullptr;
    CCLabelBMFont* m_status = nullptr;

    static int objectID(imux::generator::ObjectType type) {
        switch (type) {
            case imux::generator::ObjectType::Block: return 1;
            case imux::generator::ObjectType::Spike: return 8;
            case imux::generator::ObjectType::Orb: return 36;
            default: return 0;
        }
    }

    void setStatus(std::string const& text) {
        if (!m_status) return;
        m_status->setString(text.c_str());
        m_status->limitLabelWidth(360.f, .42f, .01f);
    }

    bool generateLevel() {
        imux::core::load();
        auto const& settings = imux::core::settings();

        if (settings.audioFile.empty()) {
            setStatus("Select a WAV file in Geode settings first.");
            return false;
        }

        imux::audio::PCMBuffer pcm;
        std::string error;
        if (!imux::audio::loadWav(settings.audioFile, pcm, error)) {
            setStatus(fmt::format("WAV error: {}", error));
            return false;
        }

        setStatus("Analyzing audio...");
        auto analysis = imux::API::analyze(
            pcm.mono,
            pcm.sampleRate,
            settings.beatSensitivity
        );

        if (analysis.beats.empty()) {
            setStatus("No usable beats detected. Lower Beat sensitivity and retry.");
            return false;
        }

        auto graph = imux::API::generate(analysis, settings);
        auto validation = imux::API::validate(graph);

        if (graph.objects.empty()) {
            setStatus("Generator produced no objects.");
            return false;
        }

        if (!m_levelEditor) {
            setStatus("Level editor is unavailable.");
            return false;
        }

        std::size_t inserted = 0;
        for (auto const& object : graph.objects) {
            const int id = objectID(object.type);
            if (id == 0) continue;

            auto gameObject = m_levelEditor->createObject(
                id,
                { object.x, object.y },
                true
            );
            if (!gameObject) continue;

            gameObject->setRotation(object.rotation);
            ++inserted;
        }

        m_levelEditor->updateEditor(0.f);

        auto status = fmt::format(
            "Generated {} objects\nBPM: {:.1f}\nBeats: {}\nWarnings: {}",
            inserted,
            analysis.bpm,
            analysis.beats.size(),
            validation.warnings
        );
        setStatus(status);
        return inserted > 0;
    }

protected:
    bool init(LevelEditorLayer* levelEditor) {
        m_levelEditor = levelEditor;

        if (!FLAlertLayer::init(
            nullptr,
            "IMUX GENERATOR",
            "",
            "CLOSE",
            "",
            420.f,
            false,
            280.f,
            1.f
        )) return false;

        auto title = CCLabelBMFont::create("IMUX BEAT GENERATOR", "goldFont.fnt");
        title->setScale(.72f);
        title->setPosition({210.f, 238.f});
        m_mainLayer->addChild(title);

        auto subtitle = CCLabelBMFont::create(
            "WAV -> analysis -> beat timeline -> playable objects",
            "bigFont.fnt"
        );
        subtitle->setScale(.36f);
        subtitle->setPosition({210.f, 213.f});
        m_mainLayer->addChild(subtitle);

        auto source = CCLabelBMFont::create(
            "Audio source: configured in Geode > ImuxHack settings",
            "bigFont.fnt"
        );
        source->setScale(.34f);
        source->setPosition({210.f, 184.f});
        m_mainLayer->addChild(source);

        m_status = CCLabelBMFont::create(
            "Ready. Select a WAV file and press GENERATE.",
            "bigFont.fnt"
        );
        m_status->setAlignment(kCCTextAlignmentCenter);
        m_status->setScale(.40f);
        m_status->setPosition({210.f, 145.f});
        m_mainLayer->addChild(m_status);

        auto menu = CCMenu::create();
        menu->setPosition({210.f, 75.f});
        m_mainLayer->addChild(menu);

        auto generateSprite = ButtonSprite::create(
            "GENERATE",
            150,
            true,
            "goldFont.fnt",
            "GJ_button_01.png",
            0.f,
            1.f
        );
        auto generate = CCMenuItemSpriteExtra::create(
            generateSprite,
            this,
            menu_selector(ImuxEditorPanel::onGenerate)
        );
        generate->setPosition({-92.f, 0.f});
        menu->addChild(generate);

        auto settingsSprite = ButtonSprite::create(
            "SETTINGS",
            150,
            true,
            "goldFont.fnt",
            "GJ_button_02.png",
            0.f,
            1.f
        );
        auto settings = CCMenuItemSpriteExtra::create(
            settingsSprite,
            this,
            menu_selector(ImuxEditorPanel::onSettings)
        );
        settings->setPosition({92.f, 0.f});
        menu->addChild(settings);

        auto help = CCLabelBMFont::create(
            "Supported now: WAV PCM 16/24/32-bit + float32",
            "bigFont.fnt"
        );
        help->setScale(.32f);
        help->setPosition({210.f, 34.f});
        m_mainLayer->addChild(help);

        return true;
    }

public:
    static ImuxEditorPanel* create(LevelEditorLayer* levelEditor) {
        auto result = new ImuxEditorPanel();
        if (result && result->init(levelEditor)) {
            result->autorelease();
            return result;
        }
        CC_SAFE_DELETE(result);
        return nullptr;
    }

    void onGenerate(CCObject*) {
        generateLevel();
    }

    void onSettings(CCObject*) {
        FLAlertLayer::create(
            "IMUX SETTINGS",
            "Open Geode mod settings and choose a WAV file under Source WAV.\n\n"
            "Difficulty, density, sync, movement, decoration, sensitivity and seed "
            "control the deterministic generator.",
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
            log::warn("ImuxHack: toolbar-categories-menu not found; generator button was not added");
            return true;
        }

        if (menu->getChildByID("imux-generator-button")) return true;

        auto sprite = CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
        if (!sprite) return true;

        sprite->setScale(.70f);

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
        if (auto panel = ImuxEditorPanel::create(m_editorLayer)) {
            panel->show();
        }
    }
};
