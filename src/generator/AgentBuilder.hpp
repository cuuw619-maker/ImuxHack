#pragma once

#include "../audio/AudioTypes.hpp"
#include "../core/Settings.hpp"
#include <Geode/Geode.hpp>
#include <cstddef>

class ImuxAgentBuilder final : public cocos2d::CCLayer {
public:
    static ImuxAgentBuilder* create();
    bool init() override;

    void start(LevelEditorLayer* editor, imux::audio::AudioAnalysis analysis,
               imux::core::Settings settings);
    void stop(bool keepObjects = true);
    bool isRunning() const { return m_running; }

private:
    LevelEditorLayer* m_editor = nullptr;
    imux::audio::AudioAnalysis m_analysis;
    imux::core::Settings m_settings;
    bool m_running = false;
    double m_clock = 0.0;
    std::size_t m_nextBeat = 0;
    float m_lastPlacedX = -10000.f;
    float m_lastSpikeX = -10000.f;
    float m_lastInputTime = -10.f;
    int m_actionIndex = 0;

    void update(float dt) override;
    void thinkAndBuild(float dt);
    void controlPlayer(float dt);
    bool chooseAndPlace(float beatTime, float strength, imux::audio::BeatType type);
    bool candidateSafe(int objectID, cocos2d::CCPoint pos, float width) const;
    void placeObject(int objectID, cocos2d::CCPoint pos, float rotation = 0.f);
    float beatTime(std::size_t index) const;
    float currentPlayerX() const;
    bool groundedCube() const;
    bool flyingMode() const;
    void emergencyInput();
};
