// turns the headless into a waypoint follower building the 28 byte rawworld self report as C MOTION 0x40
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace knc::headless {

struct Waypoint { float x, y, z, heading; };

// parses x y z heading lines from a track follow ini
std::vector<Waypoint> loadWaypoints(const std::string& path);

// checkpoint count is the first u32 of track COL sitting next to the follow ini 0 when missing
int loadCheckpointCount(const std::string& followPath);

// 19 byte packed motion body sub 4818A0 uses sub 44E610 real clients unpack it so raw bodies render as nonsense
std::vector<uint8_t> buildMotionBodyPacked(float x, float y, float z,
                                           float tx, float ty, float tz,
                                           uint8_t yaw, uint16_t stateBits);
// sub 44E610 packs three floats into a 64 bit word 12 bit magnitude 2 decimals per axis
uint64_t packVec3(float a, float b, float c);

// one 28 byte rawworld motion body position and forward tangent yaw 0 to 255
std::vector<uint8_t> buildMotionBody(float x, float y, float z,
                                     float tx, float ty, float tz,
                                     uint8_t yaw, uint16_t stateBits);

// one 28 byte ghost replayframe using ghostpackets encodeframe layout
std::vector<uint8_t> encodeReplayFrame(float x, float y, float z, uint8_t yaw);

// walks the waypoint list emitting a smooth position each tick and reports checkpoint transitions for C2S 0x41
class Driver {
public:
    void reset(std::vector<Waypoint> wp, float unitsPerSec);
    // checkpoint count from the track COL and the number of laps to run
    void setRace(int checkpointCount, int totalLaps);
    // ghost run finishes on real waypoint laps not the checkpoint timer so lap time is the honest drive time
    void setGhostLaps(int laps) { m_ghostLaps = laps; }
    // replays a recorded human checkpoint cadence instead of a fixed timer pace scales it and the pattern cycles
    void setCheckpointPattern(std::vector<double> gapsMs, double pace = 1.0);
        // slow the checkpoint cadence so a room race lap is a human time not instant
    void setCheckpointInterval(double ms) { if (ms > 0) m_cpIntervalMs = ms; }
    // current pose without advancing anything for idle room motion
    void peek(float& x, float& y, float& z, float& tx, float& ty, float& tz,
              uint8_t& yaw) const;
    // begins the run from a given world point the grid slot rather than waypoint zero
    void placeAt(float x, float y, float z);
    // starts from the waypoint nearest a grid slot so the kart pulls away straight instead of cutting across the grid
    void startNear(float x, float y, float dirx, float diry);
    bool done() const { return m_done; }
    int  lapsRun() const { return m_lapsRun; }
    // advances dtMs and fills the next pose returns false when the race is finished
    bool step(double dtMs, float& x, float& y, float& z,
              float& tx, float& ty, float& tz, uint8_t& yaw);
    // pops the next pending checkpoint pair to send as 0x41 false when none queued
    bool popCheckpoint(uint32_t& prev, uint32_t& next);
private:
    std::vector<Waypoint> m_wp;
    size_t m_idx = 0;
    float m_speed = 60.0f;   // world units per second
    float m_x = 0, m_y = 0, m_z = 0;
    bool m_started = false, m_done = false;

    int m_cpCount = 0;       // checkpoints on this track
    int m_totalLaps = 1;
    int m_cpEmitted = 0;     // checkpoint faces crossed since the race began
    int m_lapsRun = 0;
    double m_cpTimer = 0.0;  // ms since the last checkpoint face
    double m_cpIntervalMs = 350.0;
    std::vector<double> m_cpPattern;   // empty falls back to the fixed interval
    int m_ghostLaps = 0;
    std::vector<std::pair<uint32_t, uint32_t>> m_pending;  // ghostLaps 0 uses the checkpoint timer otherwise real waypoint laps pending prev next pairs caller drains into 0x41 frames
};

}
