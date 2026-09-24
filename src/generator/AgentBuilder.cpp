#include "AgentBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace geode::prelude;

ImuxAgentBuilder* ImuxAgentBuilder::forEditor(LevelEditorLayer* editor) {
    if (!editor) return nullptr;
    return typeinfo_cast<ImuxAgentBuilder*>(editor->getChildByID("imux-agent-builder"));
}

ImuxAgentBuilder* ImuxAgentBuilder::create() {
    auto ret = new ImuxAgentBuilder();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool ImuxAgentBuilder::init() {
    if (!CCLayer::init())
        return false;
    scheduleUpdate();
    setID("imux-agent-builder");
    return true;
}

void ImuxAgentBuilder::start(
    LevelEditorLayer* editor,
    imux::audio::AudioAnalysis analysis,
    imux::core::Settings settings
) {
    if (!editor || analysis.beats.empty())
        return;

    m_editor = editor;
    m_analysis = std::move(analysis);
    m_settings = settings;
    m_running = true;
    m_clock = 0.0;
    m_nextBeat = 0;
    m_lastPlacedX = editor->m_player1 ? editor->m_player1->getPositionX() + 30.f : 120.f;
    m_generationStartX = m_lastPlacedX;
    m_lastDecisionX = m_lastPlacedX;
    m_lastSpikeX = -10000.f;
    m_lastInputTime = -10.f;
    m_lastBeatTime = -1.f;
    m_failedDecisions = 0;
    m_successfulDecisions = 0;
    m_actionIndex = 0;

    const float initialSpeed = editor->m_player1
        ? std::max(45.f, static_cast<float>(editor->m_player1->getCurrentXVelocity()))
        : 120.f;
    const float unitsPerSecond = std::clamp(initialSpeed * 1.05f, 90.f, 300.f);
    m_endX = m_generationStartX +
        std::max(180.f, static_cast<float>(m_analysis.duration) * unitsPerSecond);

    if (editor->m_editorUI)
        editor->m_editorUI->onPlaytest(nullptr);
}

void ImuxAgentBuilder::stop(bool keepObjects) {
    if (!m_running)
        return;

    m_running = false;
    if (m_editor && m_editor->m_editorUI)
        m_editor->m_editorUI->onStopPlaytest(nullptr);

    if (!keepObjects && m_editor)
        m_editor->updateEditor(0.f);
}

float ImuxAgentBuilder::beatTime(std::size_t index) const {
    if (index >= m_analysis.beats.size())
        return std::numeric_limits<float>::max();
    return static_cast<float>(m_analysis.beats[index].time);
}

float ImuxAgentBuilder::currentPlayerX() const {
    if (!m_editor || !m_editor->m_player1)
        return m_lastPlacedX;
    return m_editor->m_player1->getPositionX();
}

bool ImuxAgentBuilder::groundedCube() const {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    if (!p)
        return false;
    return !p->m_isShip && !p->m_isBird && !p->m_isDart &&
           !p->m_isBall && !p->m_isRobot && !p->m_isSpider &&
           !p->m_isSwing && p->m_isOnGround;
}

bool ImuxAgentBuilder::flyingMode() const {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    if (!p)
        return false;
    return p->m_isShip || p->m_isBird || p->m_isDart || p->m_isSwing;
}

float ImuxAgentBuilder::musicX(float time) const {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    const float liveSpeed = p
        ? std::max(45.f, static_cast<float>(p->getCurrentXVelocity()))
        : 120.f;

    // Map musical time into editor distance using observed playtest speed.
    // The clamp prevents a temporary velocity spike from destroying spacing.
    const float unitsPerSecond = std::clamp(liveSpeed * 1.05f, 90.f, 300.f);
    return m_generationStartX + time * unitsPerSecond;
}

bool ImuxAgentBuilder::candidateSafe(int objectID, CCPoint pos, float width) const {
    if (!m_editor || !m_editor->m_player1)
        return false;

    const float px = m_editor->m_player1->getPositionX();
    if (pos.x < px + 30.f)
        return false;

    if (std::abs(pos.x - m_lastPlacedX) < 26.f)
        return false;

    if (objectID == 8) {
        if (pos.x - m_lastSpikeX < 78.f)
            return false;
        if (pos.y < 95.f || pos.y > 115.f)
            return false;
        if (!groundedCube())
            return false;
    }

    if (objectID == 36 && !groundedCube() && !flyingMode())
        return false;

    if (width < 1.f)
        return false;

    return true;
}

bool ImuxAgentBuilder::predictedReachable(CCPoint pos, int objectID, float beatTimeValue) const {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    if (!p)
        return false;

    const float dx = pos.x - p->getPositionX();
    const float vx = std::max(45.f, static_cast<float>(p->getCurrentXVelocity()));
    const float eta = dx / vx;

    if (eta < .12f || eta > 1.85f)
        return false;

    if (objectID == 8) {
        // A spike requires a grounded approach followed by a jump window.
        // The exact GD physics remain authoritative; this is a conservative
        // feasibility envelope used before committing an object.
        if (!groundedCube())
            return false;
        const float jumpWindow = std::clamp(.22f + m_settings.movement * .28f, .20f, .48f);
        if (eta < jumpWindow * .55f || eta > jumpWindow * 2.8f)
            return false;
    }

    if (objectID == 36) {
        const float verticalDelta = std::abs(pos.y - p->getPositionY());
        if (verticalDelta > 120.f)
            return false;
        if (!groundedCube() && !flyingMode())
            return false;
    }

    (void)beatTimeValue;
    return true;
}

float ImuxAgentBuilder::actionScore(
    int objectID, CCPoint pos, float beatTimeValue, float strength,
    imux::audio::BeatType type
) const {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    if (!p)
        return -1000.f;

    const bool accent =
        type == imux::audio::BeatType::Downbeat || strength >= .72f;
    const float vx = std::max(45.f, static_cast<float>(p->getCurrentXVelocity()));
    const float eta = std::max(0.f, (pos.x - p->getPositionX()) / vx);

    float score = strength * 1.25f;
    score += accent ? .30f : 0.f;
    score -= std::abs((pos.x - m_lastPlacedX) - 72.f) * .0018f;
    score -= eta > 1.25f ? .08f : 0.f;

    switch (objectID) {
        case 8:
            score += m_settings.difficulty * .65f;
            score += accent ? .40f : -.35f;
            score -= (1.f - m_settings.movement) * .30f;
            break;
        case 36:
            score += strength * .35f;
            score += m_settings.density * .12f;
            break;
        case 1:
            score += accent ? .12f : .24f;
            break;
        default:
            break;
    }

    // Keep repeated decisions from producing the same object rhythmically.
    if (beatTimeValue - m_lastBeatTime < .001f)
        score -= .25f;

    return score;
}

bool ImuxAgentBuilder::chooseAndPlace(
    float beatTimeValue,
    float strength,
    imux::audio::BeatType type
) {
    if (!m_editor || !m_editor->m_player1)
        return false;

    const float px = currentPlayerX();
    const float targetX = std::max(
        musicX(beatTimeValue),
        std::max(px + 45.f, m_lastPlacedX + 30.f)
    );
    const bool accent =
        type == imux::audio::BeatType::Downbeat || strength > .70f;
    const bool dense = m_settings.density > .55f;

    struct Candidate {
        int id;
        float y;
    };

    std::vector<Candidate> candidates;
    candidates.push_back({1, 105.f});

    if (groundedCube()) {
        if (accent || m_settings.difficulty > .58f)
            candidates.push_back({8, 105.f});
        if (strength > .42f)
            candidates.push_back({36, 133.f});
    }

    if (flyingMode()) {
        candidates.push_back({1, 150.f});
        if (strength > .62f)
            candidates.push_back({1, 230.f});
    }

    int bestID = 0;
    CCPoint bestPos{};
    float bestScore = -std::numeric_limits<float>::infinity();

    for (auto const& candidate : candidates) {
        const CCPoint pos{targetX, candidate.y};
        if (!candidateSafe(candidate.id, pos, 30.f))
            continue;
        if (!predictedReachable(pos, candidate.id, beatTimeValue))
            continue;

        float score = actionScore(candidate.id, pos, beatTimeValue, strength, type);

        // Avoid consecutive lethal decisions unless the music explicitly
        // accents them and the difficulty budget permits it.
        if (candidate.id == 8 && m_actionIndex > 0 &&
            m_lastSpikeX > -1000.f && targetX - m_lastSpikeX < 120.f)
            score -= .55f;

        if (candidate.id == 1 && dense && strength > .55f)
            score += .10f;

        if (score > bestScore) {
            bestScore = score;
            bestID = candidate.id;
            bestPos = pos;
        }
    }

    if (bestID == 0)
        return false;

    placeObject(bestID, bestPos);

    if (dense && strength > .60f) {
        const float supportX = bestPos.x + 30.f;
        const CCPoint support{supportX, bestPos.y + 30.f};
        if (candidateSafe(1, support, 30.f))
            placeObject(1, support);
    }

    m_lastDecisionX = bestPos.x;
    m_lastBeatTime = beatTimeValue;
    ++m_successfulDecisions;
    return true;
}

void ImuxAgentBuilder::emergencyInput() {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    if (!p)
        return;
    p->pushButton(PlayerButton::Jump);
    m_lastInputTime = static_cast<float>(m_clock);
}

void ImuxAgentBuilder::controlPlayer(float dt) {
    auto p = m_editor ? m_editor->m_player1 : nullptr;
    if (!p)
        return;

    if (!groundedCube()) {
        if (p->m_isOnGround && m_clock - m_lastInputTime > .10f)
            p->releaseButton(PlayerButton::Jump);
        return;
    }

    const float obstacleX = m_lastSpikeX > p->getPositionX()
        ? m_lastSpikeX
        : m_lastDecisionX;
    const float distance = obstacleX - p->getPositionX();
    const float vx = std::max(45.f, static_cast<float>(p->getCurrentXVelocity()));

    const float jumpLead = std::clamp(
        vx * (.24f + m_settings.movement * .12f),
        38.f, 105.f
    );

    if (distance > 18.f && distance < jumpLead &&
        m_clock - m_lastInputTime > .13f) {
        p->pushButton(PlayerButton::Jump);
        m_lastInputTime = static_cast<float>(m_clock);
    } else if (p->m_isOnGround &&
               m_clock - m_lastInputTime > .30f) {
        p->releaseButton(PlayerButton::Jump);
    }

    (void)dt;
}

void ImuxAgentBuilder::thinkAndBuild(float dt) {
    m_clock += dt;

    // Rolling horizon: decisions are made ahead of the real player instead
    // of only when a beat has already arrived. This is what makes the agent
    // a planner rather than a beat-to-object lookup table.
    constexpr double LOOK_AHEAD = .95;

    while (m_nextBeat < m_analysis.beats.size() &&
           beatTime(m_nextBeat) <= m_clock + LOOK_AHEAD) {
        auto const& beat = m_analysis.beats[m_nextBeat];

        if (musicX(static_cast<float>(beat.time)) > currentPlayerX() + 40.f) {
            if (!chooseAndPlace(
                    static_cast<float>(beat.time),
                    beat.strength,
                    beat.type)) {
                ++m_failedDecisions;
            }
        }

        ++m_nextBeat;
    }

    controlPlayer(dt);

    if (m_nextBeat >= m_analysis.beats.size()) {
        if (m_endX <= currentPlayerX() + 90.f || m_clock > m_analysis.duration + 1.5) {
            stop(true);
            return;
        }
    }

    // If the planner repeatedly finds no legal action, slow down its decision
    // pressure rather than filling the editor with unsafe objects.
    if (m_failedDecisions > m_successfulDecisions + 32 &&
        m_nextBeat > 32) {
        log::warn("ImuxHack agent stopped after excessive rejected decisions");
        stop(true);
    }
}

void ImuxAgentBuilder::update(float dt) {
    CCLayer::update(dt);
    if (!m_running || !m_editor)
        return;

    thinkAndBuild(dt);
}
