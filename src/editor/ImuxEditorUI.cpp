#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../audio/WavLoader.hpp"
#include "../audio/AudioSourceResolver.hpp"
#include "../api/ImuxAPI.hpp"
#include "../core/Settings.hpp"
#include <atomic>
#include <thread>

using namespace geode::prelude;

class ImuxEditorPanel final : public geode::Popup {
    LevelEditorLayer* m_levelEditor = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_source = nullptr;
    CCMenuItemSpriteExtra* m_generateButton = nullptr;
    CCMenuItemSpriteExtra* m_levelSongButton = nullptr;
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
        if (m_generateButton) m_generateButton->setEnabled(!busy);
        if (m_levelSongButton) m_levelSongButton->setEnabled(!busy);
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

        // Use Geode's Popup layout: m_mainLayer and m_buttonMenu are both
        // sized to the exact popup dimensions, and Anchor placement keeps
        // rendering and touch coordinates identical on Android and desktop.
        if (!geode::Popup::init(460.f, 300.f))
            return false;

        this->setTitle("IMUX MUSIC GENERATOR", "goldFont.fnt", .62f, 20.f);

        auto subtitle = CCLabelBMFont::create(
            "TURN YOUR SONG INTO GAMEPLAY",
            "bigFont.fnt"
        );
        subtitle->setScale(.34f);
        m_mainLayer->addChildAtPosition(
            subtitle, Anchor::Top, ccp(0.f, -48.f)
        );

        auto sourceTitle = CCLabelBMFont::create(
            "AUDIO SOURCE",
            "goldFont.fnt"
        );
        sourceTitle->setScale(.38f);
        m_mainLayer->addChildAtPosition(
            sourceTitle, Anchor::Top, ccp(0.f, -82.f)
        );

        m_source = CCLabelBMFont::create(
            "LEVEL SONG - AUTOMATIC",
            "bigFont.fnt"
        );
        m_source->setAlignment(kCCTextAlignmentCenter);
        m_source->setScale(.34f);
        m_source->limitLabelWidth(400.f, .34f, .01f);
        m_mainLayer->addChildAtPosition(
            m_source, Anchor::Top, ccp(0.f, -104.f)
        );

        auto info = CCLabelBMFont::create(
            "Empty override = current level song",
            "bigFont.fnt"
        );
        info->setAlignment(kCCTextAlignmentCenter);
        info->setScale(.29f);
        m_mainLayer->addChildAtPosition(
            info, Anchor::Top, ccp(0.f, -126.f)
        );

        auto statusTitle = CCLabelBMFont::create(
            "STATUS",
            "goldFont.fnt"
        );
        statusTitle->setScale(.34f);
        m_mainLayer->addChildAtPosition(
            statusTitle, Anchor::Center, ccp(0.f, 20.f)
        );

        m_status = CCLabelBMFont::create(
            "READY - PRESS GENERATE",
            "bigFont.fnt"
        );
        m_status->setAlignment(kCCTextAlignmentCenter);
        m_status->setScale(.32f);
        m_status->limitLabelWidth(400.f, .32f, .01f);
        m_mainLayer->addChildAtPosition(
            m_status, Anchor::Center, ccp(0.f, -2.f)
        );

        auto generateSprite = ButtonSprite::create(
            "GENERATE", 170, true, "goldFont.fnt",
            "GJ_button_01.png", 0.f, 1.f
        );
        m_generateButton = CCMenuItemSpriteExtra::create(
            generateSprite,
            this,
            menu_selector(ImuxEditorPanel::onGenerate)
        );

        auto levelSprite = ButtonSprite::create(
            "LEVEL SONG", 170, true, "goldFont.fnt",
            "GJ_button_02.png", 0.f, 1.f
        );
        m_levelSongButton = CCMenuItemSpriteExtra::create(
            levelSprite,
            this,
            menu_selector(ImuxEditorPanel::onLevelSong)
        );

        // These are direct children of Geode's correctly-sized popup menu.
        // Do not manually change content size or use nested menu coordinates.
        m_buttonMenu->addChildAtPosition(
            m_generateButton, Anchor::Center, ccp(-92.f, -70.f)
        );
        m_buttonMenu->addChildAtPosition(
            m_levelSongButton, Anchor::Center, ccp(92.f, -70.f)
        );

        auto hint = CCLabelBMFont::create(
            "GENERATE   |   LEVEL SONG",
            "bigFont.fnt"
        );
        hint->setAlignment(kCCTextAlignmentCenter);
        hint->setScale(.25f);
        m_mainLayer->addChildAtPosition(
            hint, Anchor::Bottom, ccp(0.f, 24.f)
        );

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
