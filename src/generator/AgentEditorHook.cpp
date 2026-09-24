#include <Geode/Geode.hpp>
#include <Geode/modify/LevelEditorLayer.hpp>
#include "AgentBuilder.hpp"

using namespace geode::prelude;

struct $modify(ImuxAgentLevelEditor, LevelEditorLayer) {
    bool init(GJGameLevel* level, bool noUI) {
        if (!LevelEditorLayer::init(level, noUI))
            return false;

        auto agent = ImuxAgentBuilder::create();
        if (agent) {
            agent->setID("imux-agent-builder");
            this->addChild(agent);
        }
        return true;
    }

    void onStopPlaytest() {
        if (auto agent = ImuxAgentBuilder::forEditor(this))
            agent->notifyPlaytestStopped();
        LevelEditorLayer::onStopPlaytest();
    }
};
