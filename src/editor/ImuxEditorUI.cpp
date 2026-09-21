#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../audio/WavLoader.hpp"
#include "../audio/AudioSourceResolver.hpp"
#include "../api/ImuxAPI.hpp"
#include "../core/Settings.hpp"

using namespace geode::prelude;

class ImuxEditorPanel final : public FLAlertLayer {
    LevelEditorLayer* m_levelEditor = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_source = nullptr;
    CCMenu* m_actionMenu = nullptr;
    bool m_busy = false;

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
        m_status->limitLabelWidth(360.f, .40f, .01f);
    }

    void setSource(std::string const& text) {
        if (!m_source) return;
        m_source->setString(text.c_str());
        m_source->limitLabelWidth(360.f, .34f, .01f);
    }

    void setBusy(bool busy) {
        m_busy = busy;
        if (m_actionMenu) m_actionMenu->setEnabled(!busy);
        setStatus(busy ? "IMUX is working..." : "Ready.");
    }

    std::string levelSongWavPath() const {
        if (!m_levelEditor || !m_levelEditor->m_level) return {};

        const int songID = m_levelEditor->m_level->m_songID;
        if (songID < 0) return {};

        auto source = imux::audio::AudioSourceResolver::fromLevelSong(songID);
        return source.path;
    }

    bool generateLevel() {
        if (m_busy) return false;

        imux::core::load();
        auto const& settings = imux::core::settings();

        std::string path = settings.audioFile;
        bool fromLevel = false;

        // Empty Source Audio means: use the song assigned to the level.
        if (path.empty()) {
            path = levelSongWavPath();
            fromLevel = true;
        }

        if (path.empty()) {
            setStatus(
                "No usable audio found.\n"
                "Set Source audio or use a level song available as WAV."
            );
            return false;
        }

        imux::audio::PCMBuffer pcm;
        std::string error;
        if (!imux::audio::loadWav(path, pcm, error)) {
            setStatus(fmt::format(
                "{} audio error: {}",
                fromLevel ? "Level" : "Source",
                error
            ));
            return false;
        }

        setSource(fromLevel ? "SOURCE: LEVEL SONG" : "SOURCE: IMUX SETTING");
        setBusy(true);
        setStatus("Analyzing music...");

        auto analysis = imux::API::analyze(
            pcm.mono,
            pcm.sampleRate,
            settings.beatSensitivity
        );

        if (analysis.beats.empty()) {
            setBusy(false);
            setStatus("No usable beats. Lower Beat sensitivity and retry.");
            return false;
        }

        auto result = imux::API::generate(
            imux::GenerationRequest{&analysis, &settings}
        );

        if (result.graph.objects.empty()) {
            setBusy(false);
            setStatus("Generator produced no objects.");
            return false;
        }

        if (!m_levelEditor) {
            setBusy(false);
            setStatus("Level editor is unavailable.");
            return false;
        }

        std::size_t inserted = 0;
        for (auto const& object : result.graph.objects) {
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
        setBusy(false);

        setStatus(fmt::format(
            "Generated {} objects | BPM {:.1f} | beats {} | warnings {}",
            inserted,
            analysis.bpm,
            analysis.beats.size(),
            result.validation.warnings
        ));

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

        auto title = CCLabelBMFont::create("IMUX MUSIC GENERATOR", "goldFont.fnt");
        title->setScale(.72f);
        title->setPosition({210.f, 238.f});
        m_mainLayer->addChild(title);

        auto subtitle = CCLabelBMFont::create(
            "MUSIC -> ANALYSIS -> PATTERNS -> GAMEPLAY",
            "bigFont.fnt"
        );
        subtitle->setScale(.38f);
        subtitle->setPosition({210.f, 213.f});
        m_mainLayer->addChild(subtitle);

        m_source = CCLabelBMFont::create(
            "SOURCE: LEVEL SONG WHEN NO OVERRIDE IS SET",
            "bigFont.fnt"
        );
        m_source->setScale(.34f);
        m_source->setPosition({210.f, 184.f});
        m_mainLayer->addChild(m_source);

        m_status = CCLabelBMFont::create(
            "Ready.",
            "bigFont.fnt"
        );
        m_status->setAlignment(kCCTextAlignmentCenter);
        m_status->setScale(.40f);
        m_status->setPosition({210.f, 145.f});
        m_mainLayer->addChild(m_status);

        // FLAlertLayer's dedicated button menu receives touches reliably on Android.
        // Previously these controls lived directly under m_mainLayer, so only the
        // built-in CLOSE button was consistently clickable.
        m_actionMenu = CCMenu::create();
        m_actionMenu->setPosition({210.f, 78.f});
        m_buttonMenu->addChild(m_actionMenu);

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
        m_actionMenu->addChild(generate);

        auto levelSprite = ButtonSprite::create(
            "LEVEL SONG",
            150,
            true,
            "goldFont.fnt",
            "GJ_button_02.png",
            0.f,
            1.f
        );
        auto levelSong = CCMenuItemSpriteExtra::create(
            levelSprite,
            this,
            menu_selector(ImuxEditorPanel::onLevelSong)
        );
        levelSong->setPosition({92.f, 0.f});
        m_actionMenu->addChild(levelSong);

        auto help = CCLabelBMFont::create(
            "Source audio overrides the level song when configured.",
            "bigFont.fnt"
        );
        help->setScale(.32f);
        help->setPosition({210.f, 35.f});
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

    void onLevelSong(CCObject*) {
        if (!m_levelEditor || !m_levelEditor->m_level) {
            setStatus("No active level.");
            return;
        }

        const int songID = m_levelEditor->m_level->m_songID;
        auto path = levelSongWavPath();

        if (path.empty()) {
            setStatus(fmt::format(
                "Level song ID {} was found, but no local WAV was found.",
                songID
            ));
            return;
        }

        imux::core::settings().audioFile.clear();
        imux::core::save();
        setSource(fmt::format("LEVEL SONG: {}", songID));
        setStatus("Level song selected. Press GENERATE.");
    }
};

struct $modify(ImuxEditorUI, EditorUI) {
    bool init(LevelEditorLayer* levelEditor) {
        if (!EditorUI::init(levelEditor)) return false;

        imux::core::load();

        auto menu = this->getChildByID("toolbar-categories-menu");
        if (!menu) {
            log::warn("ImuxHack: toolbar-categories-menu not found");
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
