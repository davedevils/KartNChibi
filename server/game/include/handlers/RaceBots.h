/// server driven CPU racers advanced along the shipped racing line and published as normal S2C 0x40 traffic
#pragma once

#include "packets/gen/SpawnPackets.h"
#include "packets/gen/ItemPackets.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

namespace knc {

/// walks the COL checkpoints like a client car so a bot scores as a human in its place
struct BotCheckpointFollower {
    int32_t checkpoint = 0;   ///< last checkpoint touched 0 is START as the client resets it at GO
    int32_t laps = 0;

    /// steps past each checkpoint the car went beyond then returns the client score for this spot
    uint32_t update(const std::vector<SpawnPackets::TrackVec3>& points, float x, float y);
};

/// place key that only grows laps then last checkpoint then the share of the next segment
float raceRankKey(const std::vector<SpawnPackets::TrackVec3>& points, int32_t laps, int32_t checkpoint, float x, float y);

/// laps as the client counts them the tracker closes a lap one report after START
int32_t clientLapsFromTracker(const SpawnPackets::LapTracker& tracker);

/// the shipped line and item boxes of one track loaded once per race
struct BotTrack {
    std::vector<SpawnPackets::TrackPoint> line;   ///< the shipped racing line and item box files x y z per row both closed loops
    std::vector<SpawnPackets::TrackPoint> boxes;
    float lineLength = 0.0f;                      ///< sum of the segment lengths loop closed
    bool  loaded = false;

    /// loads the racing line and item box files under dataRoot world theme track
    static BotTrack load(const std::string& themeFolder, const std::string& trackFolder);
};

/// a hazard dropped on the road spike bomb or ice waiting for a bot to hit it
struct BotHazard {
    int32_t  ownerId  = 0;
    int32_t  kind     = ItemPackets::ITEM_EMPTY;
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    uint64_t placedMs = 0;
};

/// a hit the server owes a bot a rocket in flight or a turtle on its way
struct BotPendingHit {
    int32_t  victimId = 0;
    int16_t  code     = ItemPackets::EFFECT_NONE;
    uint64_t dueMs    = 0;
};

/// one CPU car on the racing line uses Gamebryo z up like the client and track files
class BotDriver {
public:
    /// puts the car on the grid aimed at the nearest node ahead of it not overall nearest
    void arm(const std::vector<SpawnPackets::TrackPoint>* line, float lineLength,
             float startX, float startY, float startZ, float startYawDeg,
             float unitsPerSec, int32_t totalLaps, uint32_t seed);

    /// advances dtMs along the line false before arm or with no line
    bool step(uint64_t nowMs, uint64_t dtMs);

    float x() const { return m_x; }
    float y() const { return m_y; }
    float z() const { return m_z; }
    /// game heading in degrees zero points at minus x like the shipped lines
    float yawDeg() const { return m_yawDeg; }
    /// the 0x40 anchor pos plus fifty client frames of velocity walked along the line
    void lookahead(float& tx, float& ty, float& tz) const;
    /// world units per second right now effects and boosts folded in
    float speedNow() const { return m_speedNow; }
    /// heading change over the last step in degrees positive turns left
    float turnDeg() const { return m_turnDeg; }

    int32_t lapsDone() const { return m_laps; }
    bool    raceDone() const { return m_totalLaps > 0 && m_laps >= m_totalLaps; }
    /// 0 to 1 inside the current lap
    float   lapFraction() const;
    /// laps times 5000 plus the lap fraction times 5000 the client 0x67 scale
    uint32_t progressScore() const;
    /// distance driven since the green in world units
    float   distance() const { return m_dist; }

    /// takes a hit the same codes the client reports on 0x69
    void hit(int16_t code, uint64_t nowMs);
    /// a booster speed times mult for ms
    void boost(uint64_t nowMs, float mult, uint64_t ms);
    bool stunned(uint64_t nowMs) const { return nowMs < m_stunUntilMs; }
    /// rubber band that scales the base pace 1 is the armed pace
    void setPaceScale(float s) { m_paceScale = s; }

    int32_t  heldItem   = ItemPackets::ITEM_EMPTY;
    uint64_t useAtMs    = 0;     ///< when the held item goes off last item box pickup time a cooldown gates the next
    uint64_t lastBoxMs  = 0;
    uint64_t lastHitMs  = 0;     ///< hazard dedupe

    std::mt19937& rng() { return m_rng; }

private:
    const std::vector<SpawnPackets::TrackPoint>* m_line = nullptr;
    float    m_lineLength = 0.0f;
    size_t   m_idx = 0;
    float    m_x = 0.0f, m_y = 0.0f, m_z = 0.0f;
    float    m_yawDeg = 0.0f;
    float    m_dirX = -1.0f, m_dirY = 0.0f, m_dirZ = 0.0f;
    float    m_baseSpeed = 60.0f;
    float    m_paceScale = 1.0f;
    float    m_speedNow = 0.0f;
    float    m_turnDeg = 0.0f;
    float    m_dist = 0.0f;
    int32_t  m_laps = 0;
    int32_t  m_totalLaps = 1;
    uint64_t m_stunUntilMs = 0;
    float    m_stunMult = 1.0f;
    uint64_t m_boostUntilMs = 0;
    float    m_boostMult = 1.0f;
    bool     m_armed = false;
    std::mt19937 m_rng;
};

} // namespace knc
