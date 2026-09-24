#include <Geode/Geode.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../audio/WavLoader.hpp"
#include "../audio/AudioSourceResolver.hpp"
#include "../api/ImuxAPI.hpp"
#include "../core/Settings.hpp"
#include <atomic>
#include <thread>

using namespace geode::prelude;

class ImuxEditorPanel final : public FLAlertLayer {
    LevelEditorLayer* m_levelEditor = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_source = nullptr;
    CCMenu* m_actionMenu = nullptr;
    bool m_busy = false;
    std::atomic<bool> m_generationActive{false};
    std::uint64_t m_generationSerial = 0;

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

    imux::audio::AudioSource levelSongSource() const {
        if (!m_levelEditor || !m_levelEditor->m_level)
            return {};
        return imux::audio::AudioSourceResolver::fromLevelSong(
            m_levelEditor->m_level->m_songID
        );
    }

    bool generateLevel() {
        if (m_busy || m_generationActive.exchange(true))
            return false;

        imux::core::load();
        auto settings = imux::core::settings();

        imux::audio::AudioSource source;
        if (!settings.audioFile.empty())
            source = imux::audio::AudioSourceResolver::fromExplicitPath(settings.audioFile);
        else
            source = levelSongSource();

        if (source.path.empty()) {
            m_generationActive.store(false);
            setStatus("NO LEVEL AUDIO FOUND — SELECT AN AUDIO SOURCE");
            return false;
        }

        if (!m_levelEditor) {
            m_generationActive.store(false);
            setStatus("Level editor is unavailable.");
            return false;
        }

        const auto serial = ++m_generationSerial;
        const auto sourcePath = source.path;
        const auto sourceName = source.displayName;
        const bool fromLevel = source.fromLevel;
        const float sensitivity = settings.beatSensitivity;
        auto settingsCopy = settings;

        setSource(fromLevel
            ? fmt::format("LEVEL SONG: {}", sourceName)
            : "SOURCE: IMUX SETTING"
        );
        setBusy(true);
        setStatus("Loading and analyzing music...");

        this->retain();

        std::thread([this, serial, sourcePath, sourceName, fromLevel, sensitivity, settingsCopy]() mutable {
            imux::audio::AudioSource source =
                imux::audio::AudioSourceResolver::fromExplicitPath(sourcePath);

            imux::audio::PCMBuffer pcm;
            std::string error;
            bool loaded = imux::audio::AudioSourceResolver::load(source, pcm, error);

            imux::audio::AudioAnalysis analysis;
            imux::GenerationResult result;
            if (loaded) {
                analysis = imux::API::analyze(pcm.mono, pcm.sampleRate, sensitivity);
                if (!analysis.beats.empty())
                    result = imux::API::generate(
                        imux::GenerationRequest{&analysis, &settingsCopy}
                    );
            }

            geode::queueInMainThread([this, serial, loaded, error = std::move(error),
                analysis = std::move(analysis), result = std::move(result)]() mutable {
                if (serial != m_generationSerial) {
                    m_generationActive.store(false);
                    m_busy = false;
                    this->release();
                    return;
                }

                m_generationActive.store(false);

                if (!loaded) {
                    setBusy(false);
                    setStatus(fmt::format("Audio error: {}", error));
                    this->release();
                    return;
                }

                if (analysis.beats.empty()) {
                    setBusy(false);
                    setStatus("No usable beats. Lower Beat sensitivity and retry.");
                    this->release();
                    return;
                }

                if (result.graph.objects.empty()) {
                    setBusy(false);
                    setStatus("Generator produced no objects.");
                    this->release();
                    return;
                }

                if (!m_levelEditor) {
                    setBusy(false);
                    setStatus("Level editor is unavailable.");
                    this->release();
                    return;
                }

                std::size_t inserted = 0;
                for (auto const& object : result.graph.objects) {
                    const int id = objectID(object.type);
                    if (id == 0) continue;

                    auto gameObject = m_levelEditor->createObject(
                        id, {object.x, object.y}, true
                    );
                    if (!gameObject) continue;

                    gameObject->setRotation(object.rotation);
                    ++inserted;
                }

                m_levelEditor->updateEditor(0.f);
                setBusy(false);
                setStatus(fmt::format(
                    "Generated {} objects | BPM {:.1f} | beats {} | warnings {}",
                    inserted, analysis.bpm, analysis.beats.size(),
                    result.validation.warnings
                ));
                this->release();
            });
        }).detach();

        return true;
    }

protected:
    bool init(LevelEditorLayer* levelEditor) {
        m_levelEditor = levelEditor;

        // Keep FLAlertLayer's own button menu only for CLOSE.
        // All custom controls live in m_mainLayer so their visual bounds
        // and touch bounds share exactly the same coordinate space.
        if (!FLAlertLayer::init(
            nullptr,
            "",
            "",
            "CLOSE",
            "",
            460.f,
            false,
            300.f,
            1.f
        )) return false;

        auto title = CCLabelBMFont::create(
            "IMUX MUSIC GENERATOR",
            "goldFont.fnt"
        );
        title->setScale(.72f);
        title->setPosition({230.f, 252.f});
        m_mainLayer->addChild(title, 2);

        auto subtitle = CCLabelBMFont::create(
            "TURN YOUR SONG INTO GAMEPLAY",
            "bigFont.fnt"
        );
        subtitle->setScale(.38f);
        subtitle->setPosition({230.f, 226.f});
        m_mainLayer->addChild(subtitle, 2);

        auto sourceTitle = CCLabelBMFont::create(
            "AUDIO SOURCE",
            "goldFont.fnt"
        );
        sourceTitle->setScale(.42f);
        sourceTitle->setPosition({230.f, 194.f});
        m_mainLayer->addChild(sourceTitle, 2);

        m_source = CCLabelBMFont::create(
            "LEVEL SONG — AUTOMATIC",
            "bigFont.fnt"
        );
        m_source->setAlignment(kCCTextAlignmentCenter);
        m_source->setScale(.38f);
        m_source->setPosition({230.f, 174.f});
        m_mainLayer->addChild(m_source, 2);

        auto info = CCLabelBMFont::create(
            "Leave the audio override empty to use this level's song.",
            "bigFont.fnt"
        );
        info->setAlignment(kCCTextAlignmentCenter);
        info->setScale(.30f);
        info->setPosition({230.f, 151.f});
        m_mainLayer->addChild(info, 2);

        auto statusTitle = CCLabelBMFont::create(
            "STATUS",
            "goldFont.fnt"
        );
        statusTitle->setScale(.38f);
        statusTitle->setPosition({230.f, 126.f});
        m_mainLayer->addChild(statusTitle, 2);

        m_status = CCLabelBMFont::create(
            "READY — PRESS GENERATE",
            "bigFont.fnt"
        );
        m_status->setAlignment(kCCTextAlignmentCenter);
        m_status->setScale(.36f);
        m_status->setPosition({230.f, 108.f});
        m_mainLayer->addChild(m_status, 2);

        // This menu is intentionally a direct child of m_mainLayer.
        // Positions are local to the visible dialog, avoiding the old
        // m_buttonMenu coordinate mismatch that broke Android touches.
        m_actionMenu = CCMenu::create();
        m_actionMenu->setPosition({230.f, 67.f});
        m_mainLayer->addChild(m_actionMenu, 20);

        auto generateSprite = ButtonSprite::create(
            "GENERATE", 180, true, "goldFont.fnt",
            "GJ_button_01.png", 0.f, 1.f
        );
        auto generate = CCMenuItemSpriteExtra::create(
            generateSprite,
            this,
            menu_selector(ImuxEditorPanel::onGenerate)
        );
        generate->setContentSize({180.f, 58.f});
        generate->setPosition({-96.f, 0.f});
        m_actionMenu->addChild(generate);

        auto levelSprite = ButtonSprite::create(
            "LEVEL SONG", 180, true, "goldFont.fnt",
            "GJ_button_02.png", 0.f, 1.f
        );
        auto levelSong = CCMenuItemSpriteExtra::create(
            levelSprite,
            this,
            menu_selector(ImuxEditorPanel::onLevelSong)
        );
        levelSong->setContentSize({180.f, 58.f});
        levelSong->setPosition({96.f, 0.f});
        m_actionMenu->addChild(levelSong);

        m_actionMenu->setEnabled(true);

        auto hint = CCLabelBMFont::create(
            "GENERATE  •  LEVEL SONG  •  CLOSE",
            "bigFont.fnt"
        );
        hint->setAlignment(kCCTextAlignmentCenter);
        hint->setScale(.27f);
        hint->setPosition({230.f, 30.f});
        m_mainLayer->addChild(hint, 2);

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
        auto source = levelSongSource();
        if (source.path.empty()) {
            const int id = m_levelEditor && m_levelEditor->m_level
                ? m_levelEditor->m_level->m_songID : -1;
            setStatus(fmt::format(
                "Level song {} is not available as a local MP3/WAV.",
                id
            ));
            return;
        }

        imux::core::settings().audioFile.clear();
        imux::core::save();
        setSource(fmt::format("LEVEL SONG: {}", source.displayName));
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
            sprite, this,
            menu_selector(ImuxEditorUI::onImuxButton)
        );
        button->setID("imux-generator-button");
        menu->addChild(button);
        menu->updateLayout();

        return true;
    }

    void onImuxButton(CCObject*) {
        if (auto panel = ImuxEditorPanel::create(m_editorLayer))
            panel->show();
    }
};
