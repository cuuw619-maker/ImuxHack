#include "AgentBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace geode::prelude;

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
    m_lastPlacedX = editor->m_player1
        ? editor->m_player1->getPositionX() + 30.f
        : 120.f;
    m_lastSpikeX = -10000.f;
    m_lastInputTime = -10.f;
    m_actionIndex = 0;

    // The agent itself owns the playtest. This is deliberately the same
    // native editor path used by Geometry Dash, not a fake preview window.
    if (editor->m_editorUI)
        editor->m_editorUI->onPlaytest(nullptr);
}

void ImuxAgentBuilder::stop(bool keepObjects) {
    if (!m_running)
        return;

    m_running = false;
    if (m_editor && m_editor->m_editorUI)
        m_editor->m_editorUI->onStopPlaytest(nullptr);

    if (!keepObjects && m_editor) {
        // We intentionally leave rollback to GD's normal undo system. The
        // agent only owns its decisions, not the editor's global history.
        m_editor->updateEditor(0.f);
    }
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
    return p->m_isShip || p->m_isBird || p->m_isDart ||
           p->m_isSwing;
}

bool ImuxAgentBuilder::candidateSafe(int objectID, CCPoint pos, float width) const {
    if (!m_editor || !m_editor->m_player1)
        return false;

    auto player = m_editor->m_player1;
    const float px = player->getPositionX();
    if (pos.x < px + 35.f)
        return false;

    if (std::abs(pos.x - m_lastPlacedX) < 24.f)
        return false;

    // The planner treats lethal objects much more conservatively than
    // decorative/support objects.
    if (objectID == 8) {
        if (pos.x - m_lastSpikeX < 72.f)
            return false;
        if (pos.y < 95.f || pos.y > 115.f)
            return false;
        if (!groundedCube())
            return false;
    }

    if (objectID == 36 && !groundedCube() && !flyingMode())
        return false;

    (void)width;
    return true;
}

void ImuxAgentBuilder::placeObject(int objectID, CCPoint pos, float rotation) {
    if (!m_editor)
        return;
    auto object = m_editor->createObject(objectID, pos, true);
    if (!object)
        return;
    object->setRotation(rotation);
    m_lastPlacedX = pos.x;
    if (objectID == 8)
        m_lastSpikeX = pos.x;
    ++m_actionIndex;
}

bool ImuxAgentBuilder::chooseAndPlace(
    float beatTimeValue,
    float strength,
    imux::audio::BeatType type
) {
    if (!m_editor || !m_editor->m_player1)
        return false;

    const float px = currentPlayerX();
    const float speed = std::max(
        45.f,
        static_cast<float>(m_editor->m_player1->getCurrentXVelocity())
    );
    const float lead = std::clamp(speed * 0.45f, 58.f, 125.f);
    const float x = std::max(px + lead, m_lastPlacedX + 28.f);
    const bool accent = type == imux::audio::BeatType::Downbeat || strength > .70f;
    const bool dense = m_settings.density > .55f;

    // Candidate set = deterministic search space. No neural network and no
    // random choice: score each legal action against rhythm, player state,
    // spacing and risk, then commit the highest-scoring candidate.
    struct Candidate {
        int id;
        float y;
        float score;
    };

    std::vector<Candidate> candidates;
    candidates.push_back({1, 105.f, .20f + strength * .25f});

    if (groundedCube()) {
        candidates.push_back({
            8, 105.f,
            accent && m_settings.difficulty > .20f ? .45f + strength * .55f : .05f
        });
        candidates.push_back({
            36, 133.f,
            strength > .45f && m_settings.difficulty > .15f ? .35f + strength * .35f : .12f
        });
    }

    if (flyingMode()) {
        candidates.push_back({1, 150.f, .45f + strength * .30f});
        candidates.push_back({1, 230.f, .25f + strength * .20f});
    }

    Candidate best{0, 0.f, -1.f};
    for (auto const& candidate : candidates) {
        if (!candidateSafe(candidate.id, {x, candidate.y}, 30.f))
            continue;

        float score = candidate.score;
        if (candidate.id == 8)
            score -= std::max(0.f, 0.35f - m_settings.difficulty);
        if (candidate.id == 36 && dense)
            score += .08f;
        if (candidate.id == 1 && !accent)
            score += .08f;

        // Prefer a visually coherent spacing sequence instead of stacking
        // objects at the same X.
        score -= std::abs((x - m_lastPlacedX) - 70.f) * .0015f;

        if (score > best.score)
            best = candidate;
    }

    if (best.id == 0)
        return false;

    placeObject(best.id, {x, best.y});

    // Strong beats can receive a deterministic support cluster. The cluster
    // is non-lethal so density can increase without compromising the route.
    if (dense && strength > .58f) {
        const float clusterX = x + 26.f;
        if (candidateSafe(1, {clusterX, 135.f}, 30.f))
            placeObject(1, {clusterX, 135.f});
    }

    (void)beatTimeValue;
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

    // Closed-loop controller. It does not predict a single hard-coded
    // trajectory; it reacts to the current velocity/height and to the next
    // generated obstacle.
    if (!groundedCube()) {
        if (p->m_isOnGround && m_clock - m_lastInputTime > .10f)
            p->releaseButton(PlayerButton::Jump);
        return;
    }

    if (m_nextBeat >= m_analysis.beats.size())
        return;

    const float nextX = std::max(
        currentPlayerX() + 30.f,
        m_lastSpikeX > currentPlayerX() ? m_lastSpikeX : m_lastPlacedX
    );
    const float distance = nextX - currentPlayerX();
    const float vx = std::max(
        45.f, static_cast<float>(p->getCurrentXVelocity())
    );

    // Trigger jump in a bounded approach window. This is the agent's
    // feedback loop: observe -> act -> observe again.
    if (distance > 35.f && distance < std::clamp(vx * .34f, 55.f, 115.f) &&
        m_clock - m_lastInputTime > .16f) {
        p->pushButton(PlayerButton::Jump);
        m_lastInputTime = static_cast<float>(m_clock);
    } else if (p->m_isOnGround &&
               m_clock - m_lastInputTime > .28f) {
        p->releaseButton(PlayerButton::Jump);
    }

    (void)dt;
}

void ImuxAgentBuilder::thinkAndBuild(float dt) {
    m_clock += dt;

    while (m_nextBeat < m_analysis.beats.size() &&
           beatTime(m_nextBeat) <= m_clock + .02f) {
        auto const& beat = m_analysis.beats[m_nextBeat];

        // The agent waits for the real player to reach a usable state before
        // committing the decision. If it cannot place safely, it retries on
        // the next frame rather than forcing an unsafe object.
        if (currentPlayerX() >= m_lastPlacedX - 5.f)
            chooseAndPlace(
                static_cast<float>(beat.time),
                beat.strength,
                beat.type
            );

        ++m_nextBeat;
    }

    controlPlayer(dt);

    // Once the analyzed song is consumed, stop building but keep the
    // generated objects in the editor for manual inspection.
    if (m_nextBeat >= m_analysis.beats.size() &&
        m_clock > static_cast<double>(m_analysis.duration) + .75) {
        m_running = false;
    }
}

void ImuxAgentBuilder::update(float dt) {
    CCLayer::update(dt);
    if (!m_running || !m_editor)
        return;

    thinkAndBuild(dt);
}
