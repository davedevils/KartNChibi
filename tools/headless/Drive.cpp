#include "Drive.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace knc::headless {

std::vector<Waypoint> loadWaypoints(const std::string& path) {
    std::vector<Waypoint> out;
    FILE* f = nullptr;
    fopen_s(&f, path.c_str(), "r");
    if (!f) return out;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        Waypoint w{};
        if (sscanf_s(line, "%f,%f,%f,%f", &w.x, &w.y, &w.z, &w.heading) >= 3)
            out.push_back(w);
    }
    fclose(f);
    return out;
}

static void pushF32(std::vector<uint8_t>& b, float v) {
    uint32_t u; std::memcpy(&u, &v, 4);
    b.push_back(u & 0xFF); b.push_back((u >> 8) & 0xFF);
    b.push_back((u >> 16) & 0xFF); b.push_back((u >> 24) & 0xFF);
}

// sub 44E370 the sign combination of the three components as a code 1 to 8
static int signOctant(float a, float b, float c) {
    if (a == 0.0f) a = 1.0f;
    if (b == 0.0f) b = 1.0f;
    if (c == 0.0f) c = 1.0f;
    const float p = c * b * a;
    if (p > 0.0f) {
        if (a > 0.0f && b > 0.0f && c > 0.0f) return 1;
        int r = 2;
        if (b > 0.0f) r = 3;
        if (c > 0.0f) r = 4;
        return r;
    }
    if (a < 0.0f && b < 0.0f && c < 0.0f) return 5;
    int r = 6;
    if (b < 0.0f) r = 7;
    if (c < 0.0f) r = 8;
    return r;
}

uint64_t packVec3(float a, float b, float c) {
    const int oct = signOctant(a, b, c);
    // the client takes the magnitude the sign lives in the octant
    const float fa = a < 0 ? -a : a, fb = b < 0 ? -b : b, fc = c < 0 ? -c : c;
    auto whole = [](float v) -> uint32_t {
        int i = static_cast<int>(v);
        if (i < 0) i = 0;
        return static_cast<uint32_t>(i > 0xFFF ? 0xFFF : i);
    };
    auto frac = [](float v) -> uint32_t {
        int i = static_cast<int>(v * 100.0f) % 100;
        return static_cast<uint32_t>(i < 0 ? -i : i);
    };
    const uint32_t hi = (whole(fa) << 20) | (frac(fa) << 12) | whole(fb);
    const uint32_t lo = (frac(fb) << 24) | (whole(fc) << 12) | (frac(fc) << 4)
                      | (static_cast<uint32_t>(oct) & 0xF);
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

std::vector<uint8_t> buildMotionBodyPacked(float x, float y, float z,
                                           float tx, float ty, float tz,
                                           uint8_t yaw, uint16_t stateBits) {
    std::vector<uint8_t> b;
    b.reserve(19);
    // both words are positions not position and tangent magnitudes match every capture second word leads on a moving client
    const uint64_t w1 = packVec3(x, y, z);
    const uint64_t w2 = packVec3(tx, ty, tz);
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>((w1 >> (i * 8)) & 0xFF));
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>((w2 >> (i * 8)) & 0xFF));
    b.push_back(yaw);
    b.push_back(stateBits & 0xFF);
    b.push_back((stateBits >> 8) & 0xFF);
    return b;
}

std::vector<uint8_t> buildMotionBody(float x, float y, float z,
                                     float tx, float ty, float tz,
                                     uint8_t yaw, uint16_t stateBits) {
    std::vector<uint8_t> b;
    b.reserve(28);
    pushF32(b, x); pushF32(b, y); pushF32(b, z);
    pushF32(b, tx); pushF32(b, ty); pushF32(b, tz);
    b.push_back(yaw);
    b.push_back(0);                                  // pad the client never reads
    b.push_back(stateBits & 0xFF);
    b.push_back((stateBits >> 8) & 0xFF);
    return b;
}

int loadCheckpointCount(const std::string& followPath) {
    size_t slash = followPath.find_last_of("/\\");
    std::string dir = slash == std::string::npos ? std::string(".") : followPath.substr(0, slash);
    FILE* f = nullptr;
    fopen_s(&f, (dir + "/track.COL").c_str(), "rb");
    if (!f) return 0;
    uint8_t b[4] = {};
    size_t n = fread(b, 1, 4, f);
    fclose(f);
    if (n < 4) return 0;
    return b[0] | (b[1] << 8) | (b[2] << 16) | (b[3] << 24);
}

std::vector<uint8_t> encodeReplayFrame(float x, float y, float z, uint8_t yaw) {
    std::vector<uint8_t> r(28, 0);
    uint32_t u;
    std::memcpy(&u, &x, 4); std::memcpy(r.data() + 0x00, &u, 4);
    std::memcpy(&u, &y, 4); std::memcpy(r.data() + 0x04, &u, 4);
    std::memcpy(&u, &z, 4); std::memcpy(r.data() + 0x08, &u, 4);
    r[0x0C] = yaw;                       // 0x0D to 0x0F stay zero flags 0x10 steerSpeed 0x14 inputMask 0x18 left zero a coasting ghost
    return r;
}

void Driver::reset(std::vector<Waypoint> wp, float unitsPerSec) {
    m_wp = std::move(wp);
    m_idx = 0;
    m_speed = unitsPerSec;
    m_started = false;
    m_done = m_wp.empty();
    m_cpEmitted = 0;
    m_lapsRun = 0;
    m_pending.clear();
    if (!m_wp.empty()) { m_x = m_wp[0].x; m_y = m_wp[0].y; m_z = m_wp[0].z; }
}

void Driver::placeAt(float x, float y, float z) {
    m_x = x; m_y = y; m_z = z; m_started = true;
}

void Driver::startNear(float x, float y, float dirx, float diry) {
    if (m_wp.empty()) return;
    // only consider waypoints ahead of the grid the nearest overall can sit behind and spins the kart around
    size_t best = m_wp.size(); float bd = 1e30f;
    for (size_t i = 0; i < m_wp.size(); ++i) {
        const float dx = m_wp[i].x - x, dy = m_wp[i].y - y;
        if (dx * dirx + dy * diry <= 0.0f) continue;      // behind us
        const float d = dx * dx + dy * dy;
        if (d < bd) { bd = d; best = i; }
    }
    if (best == m_wp.size()) {                            // nothing ahead fall back
        for (size_t i = 0; i < m_wp.size(); ++i) {
            const float dx = m_wp[i].x - x, dy = m_wp[i].y - y;
            const float d = dx * dx + dy * dy;
            if (d < bd) { bd = d; best = i; }
        }
        best = best % m_wp.size();
    }
    m_idx = best;
}

void Driver::peek(float& x, float& y, float& z, float& tx, float& ty, float& tz,
                  uint8_t& yaw) const {
    if (m_wp.empty()) { x = y = z = 0; tx = 1; ty = tz = 0; yaw = 0; return; }
    const Waypoint& w = m_wp[m_idx % m_wp.size()];
    x = m_started ? m_x : w.x;
    y = m_started ? m_y : w.y;
    z = m_started ? m_z : w.z;
    const float rad = w.heading * 3.14159265f / 180.0f;
    // heading 0 points at minus X fitted over the whole shipped line against the negated pair
    tx = -cosf(rad); ty = -sinf(rad); tz = 0.0f;
    yaw = static_cast<uint8_t>(static_cast<int>(w.heading / 360.0f * 255.0f) & 0xFF);
}

void Driver::setCheckpointPattern(std::vector<double> gapsMs, double pace) {
    m_cpPattern.clear();
    if (pace <= 0.0) pace = 1.0;
    for (double g : gapsMs) {
        if (g >= 0.0) m_cpPattern.push_back(g * pace);
    }
}

void Driver::setRace(int checkpointCount, int totalLaps) {
    m_cpCount = checkpointCount > 0 ? checkpointCount : 0;
    m_totalLaps = totalLaps > 0 ? totalLaps : 1;
}

// how far along the lap 0 to 1 from the current waypoint index
static float lapFraction(size_t idx, size_t total) {
    if (total <= 1) return 0.0f;
    return static_cast<float>(idx % total) / static_cast<float>(total);
}

bool Driver::step(double dtMs, float& x, float& y, float& z,
                  float& tx, float& ty, float& tz, uint8_t& yaw) {
    if (m_done || m_wp.empty()) return false;
    if (!m_started) {
        m_started = true;
        m_idx = 1 % m_wp.size();
    }

    float budget = m_speed * static_cast<float>(dtMs / 1000.0);
    while (budget > 0.0f) {
        const Waypoint& tgt = m_wp[m_idx % m_wp.size()];
        float dx = tgt.x - m_x, dy = tgt.y - m_y, dz = tgt.z - m_z;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        if (dist <= budget || dist < 1e-3f) {
            m_x = tgt.x; m_y = tgt.y; m_z = tgt.z;
            budget -= dist;
            ++m_idx;                       // loop the path keep racing
        } else {
            float k = budget / dist;
            m_x += dx * k; m_y += dy * k; m_z += dz * k;
            budget = 0.0f;
        }
    }

    // a ghost finishes on honest waypoint laps so the time looks human
    if (m_ghostLaps > 0 && m_idx / m_wp.size() >= static_cast<size_t>(m_ghostLaps))
        m_done = true;

    // checkpoints are client authoritative step on a steady timer not position so the sequence never desyncs
    if (m_cpCount > 0) {
        m_cpTimer += dtMs;
        for (;;) {
            const double due = m_cpPattern.empty()
                ? m_cpIntervalMs
                : m_cpPattern[static_cast<size_t>(m_cpEmitted) % m_cpPattern.size()];
            if (m_cpTimer < due || m_done) break;
            m_cpTimer -= due;
            uint32_t prev = static_cast<uint32_t>(m_cpEmitted % m_cpCount);
            uint32_t next = static_cast<uint32_t>((m_cpEmitted + 1) % m_cpCount);
            m_pending.emplace_back(prev, next);
            ++m_cpEmitted;
            if (prev == 0 && m_cpEmitted > 1) {          // a prev 0 past the grid is a lap
                ++m_lapsRun;
                if (m_ghostLaps == 0 && m_lapsRun >= m_totalLaps) m_done = true;
            }
        }
    }

    const Waypoint& tgt = m_wp[m_idx % m_wp.size()];
    float dx = tgt.x - m_x, dy = tgt.y - m_y, dz = tgt.z - m_z;
    float len = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (len < 1e-3f) len = 1.0f;
    tx = dx / len; ty = dy / len; tz = dz / len;
    x = m_x; y = m_y; z = m_z;
    // Gamebryo is Z up so ground plane is X and Y heading is atan2 minus 180 degrees from minus X
    float deg = std::atan2(dy, dx) * 180.0f / 3.14159265f - 180.0f;
    while (deg < 0) deg += 360.0f;
    yaw = static_cast<uint8_t>(deg / 360.0f * 255.0f);
    return true;
}

bool Driver::popCheckpoint(uint32_t& prev, uint32_t& next) {
    if (m_pending.empty()) return false;
    prev = m_pending.front().first;
    next = m_pending.front().second;
    m_pending.erase(m_pending.begin());
    return true;
}

}
