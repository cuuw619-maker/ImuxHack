#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/modify/EditorUI.hpp>
#include "../audio/WavLoader.hpp"
#include "../audio/AudioSourceResolver.hpp"
#include "../api/ImuxAPI.hpp"
#include "../core/Settings.hpp"
#include "../integration/ThirdPartyAPI.hpp"
#include "../generator/AgentBuilder.hpp"
#include "../thirdparty/EditorTabAPI.hpp"
#include "../thirdparty/AlphaUtils.hpp"
#include <alphalaneous.alphas-ui-pack/include/API.hpp>
#include "../thirdparty/UIScalingAPI.hpp"
#include "../thirdparty/BlurAPI.hpp"
#include <atomic>
#include <thread>

using namespace geode::prelude;

class ImuxEditorPanel final : public geode::Popup {
    LevelEditorLayer* m_levelEditor = nullptr;
    CCLabelBMFont* m_status = nullptr;
    CCLabelBMFont* m_source = nullptr;
    alpha::ui::AdvancedScrollLayer* m_infoScroll = nullptr;
    float m_editorScale = 1.f;
    CCMenuItemSpriteExtra* m_generateButton = nullptr;
    CCMenuItemSpriteExtra* m_levelSongButton = nullptr;
    CCMenuItemSpriteExtra* m_playtestButton = nullptr;
    CCMenuItemSpriteExtra* m_closeButton = nullptr;
    bool m_busy = false;
    std::atomic<bool> m_generationActive{false};
    std::uint64_t m_generationSerial = 0;

    static int objectID(imux::generator::ObjectType type) {
        switch (type) {
            case imux::generator::ObjectType::Block: return 1;
            case imux::generator::ObjectType::Spike: return 8;
            case imux::generator::ObjectType::Orb: return 36;
            case imux::generator::ObjectType::Decoration: return 1;
            default: return 0;
        }
    }

    void setStatus(std::string const& text) {
        if (!m_status) return;
        m_status->setString(text.c_str());
        m_status->limitLabelWidth(340.f, .40f, .01f);
        alpha::utils::cocos::setColorByHex(m_status, "E8E8E8");
    }

    void setSource(std::string const& text) {
        if (!m_source) return;
        m_source->setString(text.c_str());
        m_source->limitLabelWidth(340.f, .34f, .01f);
        alpha::utils::cocos::setColorByHex(m_source, "FFD84A");
    }

    void setBusy(bool busy) {
        m_busy = busy;
        if (m_generateButton) m_generateButton->setEnabled(!busy);
        if (m_levelSongButton) m_levelSongButton->setEnabled(!busy);
        if (m_playtestButton) m_playtestButton->setEnabled(!busy);
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
            if (loaded)
                analysis = imux::API::analyze(pcm.mono, pcm.sampleRate, sensitivity);

            geode::queueInMainThread([this, serial, loaded, error = std::move(error),
                analysis = std::move(analysis), settingsCopy]() mutable {
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

                // The live planner is now the authoritative generator. It
                // evaluates candidates against the real player state and commits
                // only actions that pass its predictive safety checks.
                if (!m_levelEditor) {
                    setBusy(false);
                    setStatus("Level editor is unavailable.");
                    this->release();
                    return;
                }

                auto agent = ImuxAgentBuilder::forEditor(m_levelEditor);
                if (!agent) {
                    setBusy(false);
                    setStatus("Agent runtime is unavailable.");
                    this->release();
                    return;
                }

                auto currentTab = alpha::editor_tabs::getCurrentTab();
                if (currentTab.isOk()) {
                    log::debug("ImuxHack: generating from editor tab {}", currentTab.unwrap());
                }

                // The planner now owns the build. It enters native playtest,
                // observes the real player, searches legal candidates and
                // commits objects online on musical decision points.
                setBusy(false);
                setStatus(fmt::format(
                    "AI planner started | BPM {:.1f} | beats {}",
                    analysis.bpm, analysis.beats.size()
                ));
                this->onClose(nullptr);
                agent->start(m_levelEditor, std::move(analysis), settingsCopy);

            });
        }).detach();

        return true;
    }

protected:
    bool init(LevelEditorLayer* levelEditor) {
        m_levelEditor = levelEditor;

        if (!geode::Popup::init(420.f, 250.f))
            return false;

        if (BlurAPI::isBlurAPIEnabled())
            BlurAPI::addBlur(m_mainLayer);

        this->setTitle("IMUX GENERATOR", "goldFont.fnt", .62f, 20.f);

        m_infoScroll = alpha::ui::AdvancedScrollLayer::create({370.f, 58.f});
        if (m_infoScroll) {
            m_infoScroll->setVerticalScroll(false);
            m_infoScroll->setHorizontalScroll(false);
            m_infoScroll->setDraggingEnabled(false);
            m_infoScroll->setInnerContentSize({370.f, 58.f});
            m_mainLayer->addChildAtPosition(
                m_infoScroll, Anchor::Top, ccp(0.f, -69.f)
            );
        }

        m_source = CCLabelBMFont::create(
            "LEVEL SONG - AUTOMATIC", "bigFont.fnt"
        );
        m_source->setAlignment(kCCTextAlignmentCenter);
        m_source->setScale(.30f);
        if (m_infoScroll)
            m_infoScroll->getContentLayer()->addChildAtPosition(
                m_source, Anchor::Top, ccp(0.f, -7.f)
            );
        else
            m_mainLayer->addChildAtPosition(
                m_source, Anchor::Top, ccp(0.f, -52.f)
            );

        m_status = CCLabelBMFont::create(
            "READY", "bigFont.fnt"
        );
        m_status->setAlignment(kCCTextAlignmentCenter);
        m_status->setScale(.29f);
        if (m_infoScroll)
            m_infoScroll->getContentLayer()->addChildAtPosition(
                m_status, Anchor::Top, ccp(0.f, -31.f)
            );
        else
            m_mainLayer->addChildAtPosition(
                m_status, Anchor::Top, ccp(0.f, -76.f)
            );

        auto makeButton = [this](char const* text, char const* texture, cocos2d::SEL_MenuHandler selector) {
            auto sprite = ButtonSprite::create(
                text, 135, true, "goldFont.fnt", texture, 0.f, 1.f
            );
            return CCMenuItemSpriteExtra::create(sprite, this, selector);
        };

        m_playtestButton = makeButton(
            "PLAYTEST", "GJ_button_02.png",
            menu_selector(ImuxEditorPanel::onPlaytest)
        );
        m_generateButton = makeButton(
            "GENERATE", "GJ_button_01.png",
            menu_selector(ImuxEditorPanel::onGenerate)
        );
        m_levelSongButton = makeButton(
            "LEVEL SONG", "GJ_button_02.png",
            menu_selector(ImuxEditorPanel::onLevelSong)
        );
        m_closeButton = makeButton(
            "CLOSE", "GJ_button_03.png",
            menu_selector(ImuxEditorPanel::onClosePanel)
        );

        // Geode owns the popup button menu and sizes it to the popup.
        // Keep every interactive control in this menu.
        m_buttonMenu->addChildAtPosition(
            m_playtestButton, Anchor::Center, ccp(-72.f, -32.f)
        );
        m_buttonMenu->addChildAtPosition(
            m_generateButton, Anchor::Center, ccp(72.f, -32.f)
        );
        m_buttonMenu->addChildAtPosition(
            m_levelSongButton, Anchor::Bottom, ccp(-72.f, 22.f)
        );
        m_buttonMenu->addChildAtPosition(
            m_closeButton, Anchor::Bottom, ccp(72.f, 22.f)
        );

        return true;
    }

public:
    void onEnter() override {
        geode::Popup::onEnter();
        if (!m_mainLayer) return;
        m_mainLayer->setScale(0.92f);
        m_mainLayer->runAction(
            CCEaseBackOut::create(CCScaleTo::create(0.20f, 1.f))
        );
    }

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

    void onPlaytest(CCObject*) {
        if (m_busy || !m_levelEditor)
            return;

        auto editorUI = m_levelEditor->m_editorUI;
        if (!editorUI) {
            setStatus("Editor playtest is unavailable.");
            return;
        }

        // Use Geometry Dash's own playtest path. The popup closes first so
        // the player gets the real editor/playtest controls and movement.
        this->onClose(nullptr);
        editorUI->onPlaytest(nullptr);
    }

    void onClosePanel(CCObject*) {
        this->onClose(nullptr);
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
    struct Fields {
        float editorScale = 1.f;
    };

    bool init(LevelEditorLayer* levelEditor) {
        if (!EditorUI::init(levelEditor)) return false;

        imux::core::load();

        m_fields->editorScale = nwo5::uiscaling::EditorUI::getScale();
        this->addEventListener(
            nwo5::uiscaling::EditorUI::Changed(),
            [this](float scale) {
                m_fields->editorScale = scale;
                if (m_levelEditor) {
                    log::debug("ImuxHack: editor UI scale changed to {:.3f}", scale);
                }
            }
        );

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

        alpha::editor_tabs::addTab(
            "imux-ai-tab"_spr,
            alpha::editor_tabs::BUILD,
            [] {
                auto title = CCLabelBMFont::create("IMUX AI", "goldFont.fnt");
                title->setScale(.38f);
                std::vector<Ref<CCNode>> nodes;
                nodes.push_back(title);
                return alpha::editor_tabs::createEditButtonBar(nodes);
            },
            [] {
                return CCSprite::createWithSpriteFrameName("GJ_plusBtn_001.png");
            },
            [](bool active, CCNode*) {
                log::debug("ImuxHack EditorTab IMUX AI: {}", active ? "entered" : "left");
            }
        );

        return true;
    }

    void onImuxButton(CCObject*) {
        if (auto panel = ImuxEditorPanel::create(m_editorLayer))
            panel->show();
    }
};
