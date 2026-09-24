#include "packets/gen/MotionPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

namespace knc {

namespace {

// octant nibble picked by the sign triple x y z from client sub 44E370
const uint32_t kOctantForSigns[8] = { 1, 8, 7, 2, 6, 3, 4, 5 };

// sign per octant 1 to 8 from client sub 44E500 index 0 is never emitted
const float kSignX[9] = { 0.0f,  1.0f,  1.0f, -1.0f, -1.0f, -1.0f, -1.0f,  1.0f,  1.0f };
const float kSignY[9] = { 0.0f,  1.0f, -1.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f };
const float kSignZ[9] = { 0.0f,  1.0f, -1.0f, -1.0f,  1.0f, -1.0f,  1.0f,  1.0f, -1.0f };

constexpr float  kYawToWire   = 0.70833331f;  // yaw scale 255 over 360 at 0x5A6ABC inverse 360 over 255 at 0x5A6B20
constexpr float  kYawFromWire = 1.4117647f;
constexpr float  kRpmFloor    = 1000.0f;
constexpr float  kRpmSpan     = 9000.0f;
constexpr float  kRpmStep     = 600.0f;
constexpr double kQuantGuard  = 1.0e12;       // past this the u64 cast is undefined

// only bits 7 to 1 are real bit 0 is never set by the client encoder
constexpr uint16_t kStateFlagMask = 0x00FE;

uint32_t readU32LE(const uint8_t* p) {
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24);
}

float readF32LE(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

void pushF32LE(std::vector<uint8_t>& out, float v) {
    uint8_t b[4];
    std::memcpy(b, &v, 4);
    out.insert(out.end(), b, b + 4);
}

int16_t safeInterpScale(int16_t scale) {
    // zero makes the client divide 1000 by zero and fling the car to NaN
    if (scale == 0) {
        LOG_WARN("PACKET", "motion interp scale zero forced to 1000");
        return MotionPackets::kInterpScaleDefault;
    }
    return scale;
}

void writeEntryHeader(Packet& pkt, uint32_t playerId, int16_t interpScale) {
    pkt.writeUInt32(playerId);
    pkt.writeInt16(safeInterpScale(interpScale));
}

void writeEntryBody(Packet& pkt, const CarState& s, bool rawWorld) {
    if (rawWorld) {
        pkt.writeFloat(s.x);
        pkt.writeFloat(s.y);
        pkt.writeFloat(s.z);
        pkt.writeFloat(s.tx);
        pkt.writeFloat(s.ty);
        pkt.writeFloat(s.tz);
        pkt.writeUInt8(s.yaw);
        pkt.writeUInt8(0);  // client never inits this pad and never reads it
        pkt.writeUInt16(s.stateBits);
        return;
    }
    uint8_t packed[8];
    MotionPackets::packVec3(s.x, s.y, s.z, packed);
    pkt.writeBytes(packed, 8);
    MotionPackets::packVec3(s.tx, s.ty, s.tz, packed);
    pkt.writeBytes(packed, 8);
    pkt.writeUInt8(s.yaw);
    pkt.writeUInt16(s.stateBits);
}

float rowFloat(const std::map<std::string, std::string>& row, const char* key) {
    return rowFloatNoThrow(row, key, 0.0f);
}

int32_t rowInt(const std::map<std::string, std::string>& row, const char* key) {
    return static_cast<int32_t>(rowInt64NoThrow(row, key, 0));
}

} // namespace

void MotionPackets::packVec3(float x, float y, float z, uint8_t out[8]) {
    auto split = [](float v, uint32_t& ip, uint32_t& fp) {
        double a = static_cast<double>(v);
        if (a < 0.0) a = -a;
        if (!(a >= 0.0 && a < kQuantGuard)) {
            LOG_WARN("PACKET", "motion component not encodable " + std::to_string(v));
            a = static_cast<double>(kQuantMax);
        }
        if (a > static_cast<double>(kQuantMax)) {
            // client saturates the integer part only so the fraction stays garbage
            LOG_WARN("PACKET", "motion component past quantizer cap " + std::to_string(v));
        }
        fp = static_cast<uint32_t>(static_cast<uint64_t>(a * 100.0) % 100u);
        ip = static_cast<uint32_t>(a);
        if (ip > 0xFFFu) ip = 0xFFFu;
    };

    uint32_t xi = 0, xf = 0, yi = 0, yf = 0, zi = 0, zf = 0;
    split(x, xi, xf);
    split(y, yi, yf);
    split(z, zi, zf);

    // exact zero counts as positive the same way sub 44E370 recurses on it
    const uint32_t key = (x < 0.0f ? 4u : 0u) | (y < 0.0f ? 2u : 0u) | (z < 0.0f ? 1u : 0u);
    const uint32_t sel = kOctantForSigns[key];

    const uint32_t lo = (sel & 0xFu) | ((zf & 0xFFu) << 4) | ((zi & 0xFFFu) << 12)
                      | ((yf & 0xFFu) << 24);
    const uint32_t hi = (yi & 0xFFFu) | ((xf & 0xFFu) << 12) | ((xi & 0xFFFu) << 20);

    out[0] = static_cast<uint8_t>(lo & 0xFF);
    out[1] = static_cast<uint8_t>((lo >> 8) & 0xFF);
    out[2] = static_cast<uint8_t>((lo >> 16) & 0xFF);
    out[3] = static_cast<uint8_t>((lo >> 24) & 0xFF);
    out[4] = static_cast<uint8_t>(hi & 0xFF);
    out[5] = static_cast<uint8_t>((hi >> 8) & 0xFF);
    out[6] = static_cast<uint8_t>((hi >> 16) & 0xFF);
    out[7] = static_cast<uint8_t>((hi >> 24) & 0xFF);
}

bool MotionPackets::unpackVec3(const uint8_t in[8], float& x, float& y, float& z) {
    const uint32_t lo = readU32LE(in);
    const uint32_t hi = readU32LE(in + 4);

    const uint32_t sel = lo & 0xFu;
    if (sel < 1u || sel > 8u) {
        // sub 44E500 writes nothing here so the client would keep stale stack signs
        LOG_WARN("PACKET", "motion packed vec3 bad octant " + std::to_string(sel));
        return false;
    }

    const float ax = static_cast<float>((hi >> 20) & 0xFFFu)
                   + static_cast<float>((hi >> 12) & 0xFFu) * kQuantStep;
    const float ay = static_cast<float>(hi & 0xFFFu)
                   + static_cast<float>((lo >> 24) & 0xFFu) * kQuantStep;
    const float az = static_cast<float>((lo >> 12) & 0xFFFu)
                   + static_cast<float>((lo >> 4) & 0xFFu) * kQuantStep;

    x = ax * kSignX[sel];
    y = ay * kSignY[sel];
    z = az * kSignZ[sel];
    return true;
}

float MotionPackets::normalizeYaw(float degrees) {
    if (!std::isfinite(degrees)) return 0.0f;
    float d = std::fmod(degrees, 360.0f);
    if (d < 0.0f) d += 360.0f;
    if (d >= 360.0f) d = 0.0f;
    return d;
}

uint8_t MotionPackets::yawToByte(float degrees) {
    int v = static_cast<int>(normalizeYaw(degrees) * kYawToWire);
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    return static_cast<uint8_t>(v);
}

float MotionPackets::yawFromByte(uint8_t value) {
    return static_cast<float>(value) * kYawFromWire;
}

uint8_t MotionPackets::steerIndexFromDegrees(float steerDegrees) {
    if (!std::isfinite(steerDegrees)) return 0;
    const float k = kMaxSteerDegrees;
    int n = static_cast<int>((15.0f / (2.0f * k)) * (k + steerDegrees));
    if (n < 0) n = 0;
    if (n > 15) n = 15;
    return static_cast<uint8_t>(n);
}

float MotionPackets::steerDegreesFromIndex(uint8_t index) {
    const float k = kMaxSteerDegrees;
    const float step = (2.0f * k) / 15.0f;
    // decoder stores twice so the extra step is real not a typo
    return step * static_cast<float>(index & 0x0F) - k + step;
}

uint8_t MotionPackets::engineIndexFromRpm(float rpm) {
    if (!std::isfinite(rpm)) return 0;
    float r = rpm - kRpmFloor;
    if (r < 0.0f) r = 0.0f;
    if (r > kRpmSpan) r = kRpmSpan;
    int n = static_cast<int>(r / kRpmStep);
    if (n < 0) n = 0;
    if (n > 15) n = 15;
    return static_cast<uint8_t>(n);
}

float MotionPackets::rpmFromEngineIndex(uint8_t index) {
    return static_cast<float>(index & 0x0F) * kRpmStep;
}

uint16_t MotionPackets::makeStateBits(uint8_t steerIndex, uint8_t engineIndex, uint16_t flags) {
    return static_cast<uint16_t>((static_cast<uint16_t>(steerIndex & 0x0F) << 12)
                               | (static_cast<uint16_t>(engineIndex & 0x0F) << 8)
                               | (flags & kStateFlagMask));
}

uint8_t MotionPackets::stateSteerIndex(uint16_t stateBits) {
    return static_cast<uint8_t>((stateBits & kStateSteerMask) >> 12);
}

uint8_t MotionPackets::stateEngineIndex(uint16_t stateBits) {
    return static_cast<uint8_t>((stateBits & kStateEngineMask) >> 8);
}

Packet MotionPackets::motionBroadcast(const std::vector<MotionEntry>& entries, bool rawWorld) {
    Packet pkt(kOpMotion);

    size_t n = entries.size();
    if (n > kMaxWireEntries) {
        LOG_ERROR("PACKET", "motionBroadcast count " + std::to_string(n) + " over int8 cap");
        n = kMaxWireEntries;
    }
    if (n > kMaxCars) {
        LOG_WARN("PACKET", "motionBroadcast count " + std::to_string(n) + " over car table 30");
    }

    pkt.writeInt8(static_cast<int8_t>(n));
    for (size_t i = 0; i < n; ++i) {
        writeEntryHeader(pkt, entries[i].playerId, entries[i].interpScale);
        writeEntryBody(pkt, entries[i].state, rawWorld);
    }

    // wrong size here silently corrupts every remote car loud is better
    const size_t expected = 1 + n * (rawWorld ? kEntrySizeRaw : kEntrySizeCompressed);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "motionBroadcast size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet MotionPackets::motionBroadcastFor(uint32_t recipientId,
                                         const std::vector<MotionEntry>& entries,
                                         bool rawWorld) {
    // local car runs sub 49C0D0 and never pops the 0x40 queue so its slot is wasted
    std::vector<MotionEntry> filtered;
    filtered.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.playerId != recipientId) filtered.push_back(e);
    }
    return motionBroadcast(filtered, rawWorld);
}

Packet MotionPackets::motionBroadcastVerbatim(const std::vector<MotionRelayEntry>& entries,
                                              bool rawWorld) {
    const size_t bodySize = rawWorld ? kSelfReportSizeRaw : kSelfReportSize;

    std::vector<const MotionRelayEntry*> good;
    good.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.body.size() != bodySize) {
            LOG_ERROR("PACKET", "motion relay body " + std::to_string(e.body.size()) +
                                " expected " + std::to_string(bodySize) +
                                " player " + std::to_string(e.playerId));
            continue;
        }
        if (good.size() >= kMaxWireEntries) {
            LOG_ERROR("PACKET", "motion relay over int8 cap dropping rest");
            break;
        }
        good.push_back(&e);
    }

    Packet pkt(kOpMotion);
    pkt.writeInt8(static_cast<int8_t>(good.size()));
    for (const MotionRelayEntry* e : good) {
        writeEntryHeader(pkt, e->playerId, e->interpScale);
        pkt.writeBytes(e->body.data(), e->body.size());
    }

    const size_t expected = 1 + good.size() * (rawWorld ? kEntrySizeRaw : kEntrySizeCompressed);
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "motionBroadcastVerbatim size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet MotionPackets::motionBroadcastVerbatimFor(uint32_t recipientId,
                                                 const std::vector<MotionRelayEntry>& entries,
                                                 bool rawWorld) {
    std::vector<MotionRelayEntry> filtered;
    filtered.reserve(entries.size());
    for (const auto& e : entries) {
        if (e.playerId != recipientId) filtered.push_back(e);
    }
    return motionBroadcastVerbatim(filtered, rawWorld);
}

bool MotionPackets::parseSelfReport(const uint8_t* data, size_t len, bool rawWorld, CarState& out) {
    const size_t need = rawWorld ? kSelfReportSizeRaw : kSelfReportSize;
    if (data == nullptr || len < need) {
        LOG_WARN("PACKET", "C2S motion short body " + std::to_string(len) +
                           " need " + std::to_string(need));
        return false;
    }

    out = CarState{};

    if (rawWorld) {
        out.x  = readF32LE(data + 0);
        out.y  = readF32LE(data + 4);
        out.z  = readF32LE(data + 8);
        out.tx = readF32LE(data + 12);
        out.ty = readF32LE(data + 16);
        out.tz = readF32LE(data + 20);
        out.yaw = data[24];
        // byte 25 is uninitialised client stack never trust it
        out.stateBits = static_cast<uint16_t>(data[26] | (data[27] << 8));
        return true;
    }

    if (!unpackVec3(data, out.x, out.y, out.z)) return false;
    if (!unpackVec3(data + 8, out.tx, out.ty, out.tz)) return false;
    out.yaw = data[16];
    out.stateBits = static_cast<uint16_t>(data[17] | (data[18] << 8));
    return true;
}

bool MotionPackets::parseSelfReport(const Packet& pkt, bool rawWorld, CarState& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    return parseSelfReport(body.data(), body.size(), rawWorld, out);
}

std::vector<uint8_t> MotionPackets::buildSelfReportBody(const CarState& state, bool rawWorld) {
    std::vector<uint8_t> body;

    if (rawWorld) {
        body.reserve(kSelfReportSizeRaw);
        pushF32LE(body, state.x);
        pushF32LE(body, state.y);
        pushF32LE(body, state.z);
        pushF32LE(body, state.tx);
        pushF32LE(body, state.ty);
        pushF32LE(body, state.tz);
        body.push_back(state.yaw);
        body.push_back(0);  // client leaves this pad uninitialised and never reads it
        body.push_back(static_cast<uint8_t>(state.stateBits & 0xFF));
        body.push_back(static_cast<uint8_t>((state.stateBits >> 8) & 0xFF));
    } else {
        body.reserve(kSelfReportSize);
        uint8_t packed[8];
        packVec3(state.x, state.y, state.z, packed);
        body.insert(body.end(), packed, packed + 8);
        packVec3(state.tx, state.ty, state.tz, packed);
        body.insert(body.end(), packed, packed + 8);
        body.push_back(state.yaw);
        body.push_back(static_cast<uint8_t>(state.stateBits & 0xFF));
        body.push_back(static_cast<uint8_t>((state.stateBits >> 8) & 0xFF));
    }

    const size_t expected = rawWorld ? kSelfReportSizeRaw : kSelfReportSize;
    if (body.size() != expected) {
        LOG_ERROR("PACKET", "buildSelfReportBody size " + std::to_string(body.size()) +
                            " expected " + std::to_string(expected));
    }
    return body;
}

Packet MotionPackets::teleport(uint32_t playerId, float x, float y, float z, float yawDegrees) {
    Packet pkt(kOpTeleport);
    pkt.writeUInt32(playerId);
    pkt.writeFloat(x);
    pkt.writeFloat(y);
    pkt.writeFloat(z);
    pkt.writeFloat(normalizeYaw(yawDegrees));

    if (pkt.payload().size() != kTeleportSize) {
        LOG_ERROR("PACKET", "teleport size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(kTeleportSize));
    }
    return pkt;
}

Packet MotionPackets::teleportToGrid(uint32_t playerId, const GridSpawn& spawn) {
    return teleport(playerId, spawn.x, spawn.y, spawn.z, spawn.yawDegrees);
}

Packet MotionPackets::effect(uint32_t playerId, uint32_t kind,
                             uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4) {
    Packet pkt(kOpEffect);
    pkt.writeUInt32(playerId);
    pkt.writeUInt32(kind);
    pkt.writeUInt32(p1);
    pkt.writeUInt32(p2);
    pkt.writeUInt32(p3);
    pkt.writeUInt32(p4);

    if (pkt.payload().size() != kEffectSize) {
        LOG_ERROR("PACKET", "effect size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(kEffectSize));
    }
    return pkt;
}

Packet MotionPackets::effectAt(uint32_t playerId, uint32_t kind,
                               float p1, float p2, float p3, float p4) {
    uint32_t r1 = 0, r2 = 0, r3 = 0, r4 = 0;
    std::memcpy(&r1, &p1, 4);
    std::memcpy(&r2, &p2, 4);
    std::memcpy(&r3, &p3, 4);
    std::memcpy(&r4, &p4, 4);
    return effect(playerId, kind, r1, r2, r3, r4);
}

Packet MotionPackets::motionBlock(uint32_t playerId, int16_t delayMs) {
    Packet pkt(kOpBlock);
    pkt.writeUInt32(playerId);
    pkt.writeInt16(delayMs);

    if (pkt.payload().size() != kBlockSize) {
        LOG_ERROR("PACKET", "motionBlock size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(kBlockSize));
    }
    return pkt;
}

Packet MotionPackets::carEffect(uint32_t playerId, int16_t code) {
    Packet pkt(kOpCarEffect);
    pkt.writeUInt32(playerId);
    pkt.writeInt16(code);
    pkt.writeUInt8(0);  // trailing byte read then never used by sub 47AF00

    if (pkt.payload().size() != kCarEffectSize) {
        LOG_ERROR("PACKET", "carEffect size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(kCarEffectSize));
    }
    return pkt;
}

Packet MotionPackets::positionFix(const std::vector<PositionFixEntry>& entries) {
    Packet pkt(kOpPositionFix);

    size_t n = entries.size();
    if (n > kMaxWireEntries) {
        LOG_ERROR("PACKET", "positionFix count " + std::to_string(n) + " over int8 cap");
        n = kMaxWireEntries;
    }

    pkt.writeInt8(static_cast<int8_t>(n));
    for (size_t i = 0; i < n; ++i) {
        pkt.writeUInt32(entries[i].playerId);
        uint8_t packed[8];
        packVec3(entries[i].x, entries[i].y, entries[i].z, packed);
        pkt.writeBytes(packed, 8);
    }

    const size_t expected = 1 + n * kPositionFixEntrySize;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "positionFix size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

std::vector<GridSpawn> MotionPackets::gridSpawns(int32_t trackId) {
    std::vector<GridSpawn> out;

    auto rows = Database::instance().queryPrepared(
        "SELECT grid_index, spawn_x, spawn_y, spawn_z, spawn_yaw_deg FROM track_spawn "
        "WHERE track_id = ? ORDER BY grid_index",
        { trackId });

    out.reserve(rows.size());
    for (const auto& row : rows) {
        GridSpawn s;
        s.gridIndex  = rowInt(row, "grid_index");
        s.x          = rowFloat(row, "spawn_x");
        s.y          = rowFloat(row, "spawn_y");
        s.z          = rowFloat(row, "spawn_z");
        s.yawDegrees = normalizeYaw(rowFloat(row, "spawn_yaw_deg"));
        out.push_back(s);
    }

    if (out.empty()) {
        LOG_INFO("PACKET", "no track_spawn rows for track " + std::to_string(trackId));
    }
    return out;
}

std::vector<GridSpawn> MotionPackets::padGeneratedGrid(std::vector<GridSpawn> grid, size_t wanted) {
    if (wanted > 16) wanted = 16;   // sub 4B5160 skips any grid index outside 0 to 15
    const GridSpawn base = grid.empty() ? GridSpawn{} : grid.back();
    size_t extra = 1;
    while (grid.size() < wanted) {
        GridSpawn s = base;
        s.gridIndex = static_cast<int32_t>(grid.size());
        s.z += 2.0f * static_cast<float>(extra);   // stacked on the up axis so cars never overlap
        grid.push_back(s);
        ++extra;
    }
    return grid;
}

uint32_t MotionPackets::motionSteps(const MotionGateLimits& limits, uint64_t gapMs) {
    const uint32_t period = limits.reportIntervalMs != 0 ? limits.reportIntervalMs : 100;
    // client sends on a fixed period gap is only a hint bunched frames still carry one period
    uint64_t steps = (gapMs + period / 2) / period;
    if (steps < 1) steps = 1;
    if (limits.maxSteps != 0 && steps > limits.maxSteps) steps = limits.maxSteps;
    return static_cast<uint32_t>(steps);
}

MotionVerdict MotionPackets::motionStep(MotionGateState& state, const MotionGateLimits& limits,
                                        float x, float y, float z, uint64_t nowMs,
                                        float* budgetXYOut) {
    const uint32_t period = limits.reportIntervalMs != 0 ? limits.reportIntervalMs : 100;

    if (!state.primed) {
        state.primed = true;
        state.x = x;
        state.y = y;
        state.z = z;
        state.lastAcceptedMs = nowMs;
        if (budgetXYOut) *budgetXYOut = 0.0f;
        return MotionVerdict::FirstSample;
    }

    const uint64_t gap = nowMs > state.lastAcceptedMs ? nowMs - state.lastAcceptedMs : 0;
    const uint32_t steps = motionSteps(limits, gap);
    const float span = static_cast<float>(steps) * static_cast<float>(period) / 1000.0f;
    const float budgetXY = limits.axisXYWuS * span * limits.tolerance;
    const float budgetZ  = limits.axisZWuS  * span * limits.tolerance;
    // a rescue drops the car on the nearest follow path node which no budget can predict
    const float snapXY = budgetXY > limits.snapAxisWu ? budgetXY : limits.snapAxisWu;
    const float snapZ  = budgetZ  > limits.snapAxisWu ? budgetZ  : limits.snapAxisWu;

    const float dx = std::fabs(x - state.x);
    const float dy = std::fabs(y - state.y);
    const float dz = std::fabs(z - state.z);

    // the sample always becomes the new truth the server is not authoritative on position
    state.x = x;
    state.y = y;
    state.z = z;
    state.lastAcceptedMs = nowMs;
    if (budgetXYOut) *budgetXYOut = budgetXY;

    if (dx > snapXY || dy > snapXY || dz > snapZ) return MotionVerdict::Teleport;
    if (dx > budgetXY || dy > budgetXY || dz > budgetZ) return MotionVerdict::Snap;
    return MotionVerdict::Ok;
}

} // namespace knc
