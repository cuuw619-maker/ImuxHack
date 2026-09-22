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
            setStatus(
                "No level audio found.\\n"
                "Set Source audio or make the level song available locally."
            );
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

        m_status = CCLabelBMFont::create("Ready.", "bigFont.fnt");
        m_status->setAlignment(kCCTextAlignmentCenter);
        m_status->setScale(.40f);
        m_status->setPosition({210.f, 145.f});
        m_mainLayer->addChild(m_status);

        // Do not put the action menu inside m_buttonMenu.
        // FLAlertLayer positions m_buttonMenu independently from m_mainLayer,
        // so nested coordinates become incorrect on Android and can move the
        // hitboxes outside the visible dialog.
        m_actionMenu = CCMenu::create();
        m_actionMenu->setPosition({210.f, 82.f});
        m_actionMenu->setContentSize({380.f, 76.f});
        m_actionMenu->setAnchorPoint({0.5f, 0.5f});
        m_mainLayer->addChild(m_actionMenu, 20);

        auto generateSprite = ButtonSprite::create(
            "GENERATE", 170, true, "goldFont.fnt",
            "GJ_button_01.png", 0.f, 1.f
        );
        auto generate = CCMenuItemSpriteExtra::create(
            generateSprite, this,
            menu_selector(ImuxEditorPanel::onGenerate)
        );
        generate->setPosition({115.f, 38.f});
        generate->setContentSize({170.f, 76.f});
        m_actionMenu->addChild(generate);

        auto levelSprite = ButtonSprite::create(
            "LEVEL SONG", 170, true, "goldFont.fnt",
            "GJ_button_02.png", 0.f, 1.f
        );
        auto levelSong = CCMenuItemSpriteExtra::create(
            levelSprite, this,
            menu_selector(ImuxEditorPanel::onLevelSong)
        );
        levelSong->setPosition({265.f, 38.f});
        levelSong->setContentSize({170.f, 76.f});
        m_actionMenu->addChild(levelSong);

        m_actionMenu->updateLayout();

        auto help = CCLabelBMFont::create(
            "No override = analyze the song assigned to this level.",
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
