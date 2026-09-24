/// the CPU car on the racing line see RaceBots
#include "handlers/RaceBots.h"
#include "logging/Logger.h"

#include <algorithm>
#include <cmath>

namespace knc {

namespace {

constexpr float kPi = 3.14159265f;

// 0x59F450 the client builds its 0x40 anchor with fifty frames of velocity
constexpr float kPredictTicks = 50.0f;
// no receiver frame time on the wire sixty per second is what a stock client runs
constexpr float kFrameDtSeconds = 1.0f / 60.0f;

float segLen(const SpawnPackets::TrackPoint& a, const SpawnPackets::TrackPoint& b) {
    const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/// game heading from a ground direction measured from minus X like the shipped lines
float headingFromDir(float dx, float dy) {
    float deg = std::atan2(dy, dx) * 180.0f / kPi - 180.0f;
    while (deg < 0.0f) deg += 360.0f;
    while (deg >= 360.0f) deg -= 360.0f;
    return deg;
}

} // namespace

BotTrack BotTrack::load(const std::string& themeFolder, const std::string& trackFolder) {
    BotTrack t;
    if (themeFolder.empty() || trackFolder.empty()) return t;

    t.line = SpawnPackets::loadFollowPath(themeFolder, trackFolder, 1);
    if (t.line.size() < 3) {
        LOG_WARN("BOT", "no racing line under " + SpawnPackets::trackBase(themeFolder, trackFolder) +
                        " bots fall back to the clock");
        t.line.clear();
        return t;
    }
    for (size_t i = 0; i < t.line.size(); ++i) {
        t.lineLength += segLen(t.line[i], t.line[(i + 1) % t.line.size()]);
    }
    if (t.lineLength < 1.0f) {
        LOG_WARN("BOT", "racing line has no length under " + trackFolder);
        t.line.clear();
        return t;
    }

    // itembox ini rows are x y z the yaw column is absent and reads as zero
    t.boxes = SpawnPackets::loadIniPoints(
        SpawnPackets::trackBase(themeFolder, trackFolder) + "/itembox.ini",
        static_cast<size_t>(ItemPackets::kItemBoxMaxPositions));

    t.loaded = true;
    LOG_INFO("BOT", "track " + themeFolder + "/" + trackFolder + " line " +
             std::to_string(t.line.size()) + " nodes " +
             std::to_string(static_cast<int>(t.lineLength)) + " units, " +
             std::to_string(t.boxes.size()) + " item boxes");
    return t;
}

void BotDriver::arm(const std::vector<SpawnPackets::TrackPoint>* line, float lineLength,
                    float startX, float startY, float startZ, float startYawDeg,
                    float unitsPerSec, int32_t totalLaps, uint32_t seed) {
    m_line = line;
    m_lineLength = lineLength;
    m_x = startX; m_y = startY; m_z = startZ;
    m_yawDeg = startYawDeg;
    m_baseSpeed = unitsPerSec;
    m_paceScale = 1.0f;
    m_speedNow = 0.0f;
    m_turnDeg = 0.0f;
    m_dist = 0.0f;
    m_laps = 0;
    m_totalLaps = totalLaps > 0 ? totalLaps : 1;
    m_stunUntilMs = 0;
    m_boostUntilMs = 0;
    m_rng.seed(seed);
    heldItem = ItemPackets::ITEM_EMPTY;
    useAtMs = 0;
    lastBoxMs = 0;
    lastHitMs = 0;
    m_armed = line != nullptr && !line->empty();
    if (!m_armed) return;

    // the start yaw is measured from minus X so the grid faces this way
    const float rad = startYawDeg * kPi / 180.0f;
    const float faceX = -std::cos(rad), faceY = -std::sin(rad);
    m_dirX = faceX; m_dirY = faceY; m_dirZ = 0.0f;

    // aim at the nearest node ahead of the grid since the nearest overall node can be behind us
    size_t best = line->size();
    float bestD = 1e30f;
    for (size_t i = 0; i < line->size(); ++i) {
        const float dx = (*line)[i].x - startX, dy = (*line)[i].y - startY;
        if (dx * faceX + dy * faceY <= 0.0f) continue;
        const float d = dx * dx + dy * dy;
        if (d < bestD) { bestD = d; best = i; }
    }
    if (best == line->size()) {
        for (size_t i = 0; i < line->size(); ++i) {
            const float dx = (*line)[i].x - startX, dy = (*line)[i].y - startY;
            const float d = dx * dx + dy * dy;
            if (d < bestD) { bestD = d; best = i; }
        }
    }
    m_idx = best % line->size();
}

bool BotDriver::step(uint64_t nowMs, uint64_t dtMs) {
    if (!m_armed || !m_line || m_line->empty() || dtMs == 0) return false;

    float mult = m_paceScale;
    if (nowMs < m_stunUntilMs) mult *= m_stunMult;
    if (nowMs < m_boostUntilMs) mult *= m_boostMult;
    // a finished car cruises its cool down lap so the remotes see it roll on
    if (raceDone()) mult *= 0.55f;

    const float target = m_baseSpeed * mult;
    // ease toward the target so a hit or a boost reads as a ramp not a jump
    const float accel = (target > m_speedNow ? 55.0f : 140.0f) * static_cast<float>(dtMs) / 1000.0f;
    if (m_speedNow < target) m_speedNow = std::min(target, m_speedNow + accel);
    else                     m_speedNow = std::max(target, m_speedNow - accel);

    const float prevYaw = m_yawDeg;
    float budget = m_speedNow * static_cast<float>(dtMs) / 1000.0f;
    const auto& line = *m_line;
    int guard = 0;
    while (budget > 0.0f && guard++ < 64) {
        const SpawnPackets::TrackPoint& tgt = line[m_idx % line.size()];
        const float dx = tgt.x - m_x, dy = tgt.y - m_y, dz = tgt.z - m_z;
        const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= budget || dist < 1e-3f) {
            m_x = tgt.x; m_y = tgt.y; m_z = tgt.z;
            budget -= dist;
            m_dist += dist;
            m_idx = (m_idx + 1) % line.size();
        } else {
            const float k = budget / dist;
            m_x += dx * k; m_y += dy * k; m_z += dz * k;
            m_dist += budget;
            budget = 0.0f;
        }
    }

    // the lap is the line length wherever on it the grid put us
    const int32_t laps = static_cast<int32_t>(m_dist / m_lineLength);
    if (laps > m_laps) m_laps = laps;

    const SpawnPackets::TrackPoint& tgt = line[m_idx % line.size()];
    float dx = tgt.x - m_x, dy = tgt.y - m_y, dz = tgt.z - m_z;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-3f) len = 1.0f;
    m_dirX = dx / len; m_dirY = dy / len; m_dirZ = dz / len;
    m_yawDeg = headingFromDir(dx, dy);

    float turn = m_yawDeg - prevYaw;
    while (turn > 180.0f) turn -= 360.0f;
    while (turn < -180.0f) turn += 360.0f;
    m_turnDeg = turn;
    return true;
}

void BotDriver::lookahead(float& tx, float& ty, float& tz) const {
    // 0x49BEB0 anchors fifty frames of velocity ahead a shorter lead draws the car behind itself
    const float lead = m_speedNow * kPredictTicks * kFrameDtSeconds;
    tx = m_x; ty = m_y; tz = m_z;
    if (lead <= 0.0f) return;

    // walk the line so the anchor stays on the road where a straight ray would cut the corner
    if (m_line != nullptr && m_line->size() >= 2) {
        const auto& line = *m_line;
        float budget = lead;
        float cx = m_x, cy = m_y, cz = m_z;
        size_t idx = m_idx;
        int guard = 0;
        while (budget > 0.0f && guard++ < 128) {
            const SpawnPackets::TrackPoint& n = line[idx % line.size()];
            const float dx = n.x - cx, dy = n.y - cy, dz = n.z - cz;
            const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (d < 1e-3f) { idx = (idx + 1) % line.size(); continue; }
            if (d > budget) {
                const float k = budget / d;
                cx += dx * k; cy += dy * k; cz += dz * k;
                budget = 0.0f;
                break;
            }
            cx = n.x; cy = n.y; cz = n.z;
            budget -= d;
            idx = (idx + 1) % line.size();
        }
        tx = cx; ty = cy; tz = cz;
        return;
    }

    tx = m_x + m_dirX * lead;
    ty = m_y + m_dirY * lead;
    tz = m_z + m_dirZ * lead;
}

float BotDriver::lapFraction() const {
    if (m_lineLength <= 0.0f) return 0.0f;
    const float f = std::fmod(m_dist, m_lineLength) / m_lineLength;
    return f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
}

float raceRankKey(const std::vector<SpawnPackets::TrackVec3>& points, int32_t laps, int32_t checkpoint, float x, float y) {
    const int32_t n = static_cast<int32_t>(points.size());
    if (n < 2) return static_cast<float>(laps) * 5000.0f;
    if (checkpoint < 0 || checkpoint >= n) checkpoint = 0;
    const SpawnPackets::TrackVec3& a = points[static_cast<size_t>(checkpoint)];
    const SpawnPackets::TrackVec3& b = points[static_cast<size_t>((checkpoint + 1) % n)];
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len2 = dx * dx + dy * dy;
    float frac = len2 > 1e-3f ? ((x - a.x) * dx + (y - a.y) * dy) / len2 : 0.0f;
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    const float bucket = 5000.0f / static_cast<float>(n);
    return static_cast<float>(laps) * 5000.0f + (static_cast<float>(checkpoint) + frac) * bucket;
}

int32_t clientLapsFromTracker(const SpawnPackets::LapTracker& tracker) {
    // back on START after the grid the client already counts the lap the tracker waits for the next face
    if (tracker.nextCheckpoint() == 0 && tracker.crossings() >= 1) return tracker.lapsCompleted() + 1;
    return tracker.lapsCompleted();
}

uint32_t BotCheckpointFollower::update(const std::vector<SpawnPackets::TrackVec3>& points, float x, float y) {
    const int32_t n = static_cast<int32_t>(points.size());
    if (n < 2) return static_cast<uint32_t>(laps) * 5000u;
    float frac = 0.0f;
    // a car may pass more than one point in a tick the bound stops a loop on a broken head
    for (int32_t step = 0; step <= n; ++step) {
        const SpawnPackets::TrackVec3& a = points[static_cast<size_t>(checkpoint)];
        const SpawnPackets::TrackVec3& b = points[static_cast<size_t>((checkpoint + 1) % n)];
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        frac = len2 > 1e-3f ? ((x - a.x) * dx + (y - a.y) * dy) / len2 : 1.0f;
        if (frac < 1.0f) break;
        // the client only takes the expected next face and closes a lap on START
        checkpoint = (checkpoint + 1) % n;
        if (checkpoint == 0) ++laps;
    }
    if (frac < 0.0f) frac = 0.0f;
    if (frac > 1.0f) frac = 1.0f;
    return SpawnPackets::progressScore(static_cast<uint32_t>(laps), static_cast<uint32_t>((checkpoint + 1) % n), frac, n);
}

uint32_t BotDriver::progressScore() const {
    // C2S 0x67 is laps times 5000 plus the lap bucket so score over 5000 is the completed laps
    return static_cast<uint32_t>(m_laps) * 5000u +
           static_cast<uint32_t>(lapFraction() * 5000.0f);
}

void BotDriver::hit(int16_t code, uint64_t nowMs) {
    // sub 495C30 refuses to stack so a car already reeling ignores the next hit
    if (nowMs < m_stunUntilMs) return;
    uint64_t ms = 0;
    float mult = 1.0f;
    switch (code) {
        // the client plays the whole crash with no motion sample so hold it out then accelerate from where it stopped
        case ItemPackets::EFFECT_CRASH: ms = 2800; mult = 0.0f;  break;
        case ItemPackets::EFFECT_SPIN:  ms = ItemPackets::kSpinDurationMs; mult = 0.25f; break;
        case ItemPackets::EFFECT_ICE:   ms = 1500; mult = 0.55f; break;
        case ItemPackets::EFFECT_HIVE:  ms = 2000; mult = 0.70f; break;
        case ItemPackets::EFFECT_BUMP:  ms = 500;  mult = 0.80f; break;
        default: return;
    }
    m_stunUntilMs = nowMs + ms;
    m_stunMult = mult;
    m_boostUntilMs = 0;
    lastHitMs = nowMs;
}

void BotDriver::boost(uint64_t nowMs, float mult, uint64_t ms) {
    m_boostUntilMs = nowMs + ms;
    m_boostMult = mult;
}

} // namespace knc
