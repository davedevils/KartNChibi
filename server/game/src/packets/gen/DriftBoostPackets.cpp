#include "packets/gen/DriftBoostPackets.h"
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

int32_t readI32LE(const uint8_t* p) {
    return static_cast<int32_t>(static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16)
         | (static_cast<uint32_t>(p[3]) << 24));
}

float clampf(float v, float lo, float hi) {
    if (!(v == v)) return lo;  // NaN falls to the low clamp same as the client ftol garbage
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

std::string rowStr(const std::map<std::string, std::string>& row, const char* key) {
    return rowStrCore(row, key);
}

int64_t rowInt(const std::map<std::string, std::string>& row, const char* key,
               int64_t fallback = 0) {
    return rowInt64NoThrow(row, key, fallback);
}

float rowFloat(const std::map<std::string, std::string>& row, const char* key,
               float fallback = 0.0f) {
    return rowFloatNoThrow(row, key, fallback);
}

// named stat columns not a blob so they tune by hand the index prefix follows record 0xA4
const char* const kStatColumns[17] = {
    "stat00_body_setup", "stat01_max_speed", "stat02_steering_gain", "stat03_mini_turbo_target",
    "stat04_boost_lean_lift", "stat05_turn_force", "stat06_wheel_spin", "stat07_wheel_steer_angle",
    "stat08_drift_charge_rate", "stat09_drift_steer", "stat10_mini_turbo_threshold", "stat11_mini_turbo_hold",
    "stat12_grip", "stat13", "stat14_camera_distance", "stat15_camera_pitch", "stat16_camera_height"
};

// hex not blob else the db layer truncates at the first NUL
bool decodeStatHex(const std::string& hex, KartStatBlock& out) {
    out.fill(0.0f);
    if (hex.empty()) return false;
    if (hex.size() != 136) {
        LOG_WARN("PACKET", "part stat hex length " + std::to_string(hex.size()) + " want 136");
        return false;
    }
    uint8_t raw[68];
    for (size_t i = 0; i < 68; ++i) {
        char pair[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        char* end = nullptr;
        const long v = std::strtol(pair, &end, 16);
        if (end != pair + 2) {
            LOG_WARN("PACKET", "part stat hex not hex at byte " + std::to_string(i));
            return false;
        }
        raw[i] = static_cast<uint8_t>(v);
    }
    std::memcpy(out.data(), raw, 68);
    return true;
}

} // namespace

DriftBoostState DriftBoostPackets::decodeStateBits(uint16_t stateBits) {
    DriftBoostState s;
    s.steerIndex       = static_cast<uint8_t>((stateBits & BIT_STEER) >> 12);
    s.engineIndex      = static_cast<uint8_t>((stateBits & BIT_ENGINE) >> 8);
    s.boostClass0      = (stateBits & BIT_BOOST0) != 0;
    s.boostClass1      = (stateBits & BIT_BOOST1) != 0;
    s.reverseGear      = (stateBits & BIT_REVERSE) != 0;
    s.drifting         = (stateBits & BIT_DRIFT) != 0;
    s.miniTurboCharged = (stateBits & BIT_CHARGED) != 0;
    s.steerInputLeft   = (stateBits & BIT_STEER_L) != 0;
    s.steerInputRight  = (stateBits & BIT_STEER_R) != 0;
    return s;
}

uint16_t DriftBoostPackets::encodeStateBits(const DriftBoostState& state) {
    uint16_t flags = 0;
    if (state.boostClass0)      flags |= BIT_BOOST0;
    if (state.boostClass1)      flags |= BIT_BOOST1;
    if (state.reverseGear)      flags |= BIT_REVERSE;
    if (state.drifting)         flags |= BIT_DRIFT;
    if (state.miniTurboCharged) flags |= BIT_CHARGED;
    if (state.steerInputLeft)   flags |= BIT_STEER_L;
    if (state.steerInputRight)  flags |= BIT_STEER_R;
    return makeStateBits(state.steerIndex, state.engineIndex, flags);
}

uint8_t DriftBoostPackets::stateSteerIndex(uint16_t stateBits) {
    return static_cast<uint8_t>((stateBits & BIT_STEER) >> 12);
}

uint8_t DriftBoostPackets::stateEngineIndex(uint16_t stateBits) {
    return static_cast<uint8_t>((stateBits & BIT_ENGINE) >> 8);
}

uint8_t DriftBoostPackets::steerIndexFromDegrees(float steerDegrees) {
    if (!std::isfinite(steerDegrees)) return 0;
    const float k = MAX_STEER_DEG;
    int n = static_cast<int>((15.0f / (2.0f * k)) * (k + steerDegrees));
    // client shifts left by 12 and keeps the low word so 16 wraps to hard left
    if (n < 0) n = 0;
    if (n > 15) n = 15;
    return static_cast<uint8_t>(n);
}

float DriftBoostPackets::steerDegreesFromIndex(uint8_t index) {
    const float step = (2.0f * MAX_STEER_DEG) / 15.0f;
    // decoder stores then adds one more step so the extra term is real
    return step * static_cast<float>(index & 0x0F) - MAX_STEER_DEG + step;
}

uint8_t DriftBoostPackets::engineIndexFromRpm(float rpm) {
    if (!std::isfinite(rpm)) return 0;
    const float r = clampf(rpm - RPM_FLOOR, 0.0f, RPM_SPAN);
    int n = static_cast<int>(r / RPM_STEP);
    if (n < 0) n = 0;
    if (n > 15) n = 15;
    return static_cast<uint8_t>(n);
}

float DriftBoostPackets::rpmFromEngineIndex(uint8_t index) {
    // decoder never adds the 1000 floor back so this is not the encoded rpm
    return static_cast<float>(index & 0x0F) * RPM_STEP;
}

uint16_t DriftBoostPackets::makeStateBits(uint8_t steerIndex, uint8_t engineIndex,
                                          uint16_t flags) {
    if (steerIndex > 15) {
        LOG_WARN("PACKET", "steer index " + std::to_string(steerIndex) + " wraps clamping");
        steerIndex = 15;
    }
    if (engineIndex > 15) {
        LOG_WARN("PACKET", "engine index " + std::to_string(engineIndex) + " wraps clamping");
        engineIndex = 15;
    }
    return static_cast<uint16_t>((static_cast<uint16_t>(steerIndex & 0x0F) << 12)
                               | (static_cast<uint16_t>(engineIndex & 0x0F) << 8)
                               | (flags & FLAG_MASK));
}

bool DriftBoostPackets::stateBitsFromSelfReport(const uint8_t* data, size_t len,
                                                bool rawWorld, uint16_t& out) {
    const size_t need = rawWorld ? SELF_REPORT_SIZE_RAW : SELF_REPORT_SIZE;
    const size_t off  = rawWorld ? STATE_OFFSET_RAW : STATE_OFFSET;
    if (data == nullptr || len < need) {
        LOG_WARN("PACKET", "driftboost self report short " + std::to_string(len) +
                           " need " + std::to_string(need));
        return false;
    }
    out = static_cast<uint16_t>(data[off] | (static_cast<uint16_t>(data[off + 1]) << 8));
    return true;
}

bool DriftBoostPackets::stateBitsFromSelfReport(const Packet& pkt, bool rawWorld,
                                                uint16_t& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    return stateBitsFromSelfReport(body.data(), body.size(), rawWorld, out);
}

uint32_t DriftBoostPackets::stateBitsAnomalies(uint16_t stateBits) {
    uint32_t mask = 0;
    if (stateBits & BIT_UNUSED) mask |= VIOL_RESERVED_BIT;
    // encoder picks one class or the other never both
    if ((stateBits & BIT_BOOST0) && (stateBits & BIT_BOOST1)) mask |= VIOL_BOTH_CLASSES;
    return mask;
}

bool DriftBoostPackets::parseItemUse(const uint8_t* data, size_t len, ItemUseRequest& out) {
    if (data == nullptr || len < ITEM_USE_C2S_SIZE) {
        LOG_WARN("PACKET", "C2S item use short " + std::to_string(len) +
                           " need " + std::to_string(ITEM_USE_C2S_SIZE));
        return false;
    }
    out.itemType = readI32LE(data + 0);
    out.p1       = readI32LE(data + 4);
    out.p2       = readI32LE(data + 8);
    out.p3       = readI32LE(data + 12);
    out.p4       = readI32LE(data + 16);
    return true;
}

bool DriftBoostPackets::parseItemUse(const Packet& pkt, ItemUseRequest& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    return parseItemUse(body.data(), body.size(), out);
}

bool DriftBoostPackets::isBoostItemType(int32_t itemType) {
    return itemType == 0 || itemType == 1;
}

bool DriftBoostPackets::s2cItemTypeIsHandled(int32_t itemType) {
    switch (itemType) {
        case 2: case 3: case 4: case 5: case 7: case 8: case 9:
        case 11: case 12: case 13: case 14: case 15:
        case 17: case 18: case 19: case 20: case 21:
            return true;
        default:
            return false;
    }
}

bool DriftBoostPackets::parseGameEvent(const uint8_t* data, size_t len,
                                       GameEventRequest& out) {
    if (data == nullptr || len < GAME_EVENT_C2S_SIZE) {
        LOG_WARN("PACKET", "C2S game event short " + std::to_string(len) +
                           " need " + std::to_string(GAME_EVENT_C2S_SIZE));
        return false;
    }
    out.kind    = readI32LE(data + 0);
    out.targetA = readI32LE(data + 4);
    out.targetB = readI32LE(data + 8);
    if (!isLiveEventKind(out.kind)) {
        LOG_WARN("PACKET", "C2S game event kind " + std::to_string(out.kind) +
                           " client only sends 10 or 16");
    }
    return true;
}

bool DriftBoostPackets::parseGameEvent(const Packet& pkt, GameEventRequest& out) {
    const std::vector<uint8_t>& body = pkt.payload();
    return parseGameEvent(body.data(), body.size(), out);
}

bool DriftBoostPackets::isLiveEventKind(int32_t kind) {
    return kind == 10 || kind == 16;
}

Packet DriftBoostPackets::gameEvent(uint32_t playerId, int32_t kind, int32_t p1, int32_t p2) {
    Packet pkt = Packet::fromCmdFull(OP_GAME_EVENT);

    if (!isLiveEventKind(kind)) {
        // receiver has no branch for anything else so it draws nothing
        LOG_WARN("PACKET", "game event kind " + std::to_string(kind) +
                           " is dropped by sub_47A460");
    }

    pkt.writeUInt32(playerId);
    pkt.writeInt32(kind);
    pkt.writeInt32(p1);
    pkt.writeInt32(p2);

    if (pkt.payload().size() != GAME_EVENT_S2C_SIZE) {
        LOG_ERROR("PACKET", "gameEvent size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(GAME_EVENT_S2C_SIZE));
    }
    return pkt;
}

int DriftBoostPackets::partLevelMultiplier(int32_t level) {
    // integer divide then FILD in the client so 0 to 49 all give exactly one
    return 1 + static_cast<int>(level / 50);
}

KartStatBlock DriftBoostPackets::effectiveStats(const KartDef& kart,
                                                const std::vector<DriftEquippedPart>& parts,
                                                const std::vector<KartStatBlock>& partStats) {
    KartStatBlock out = kart.stats;

    if (kart.modelScheme != 1) return out;

    if (parts.size() != partStats.size()) {
        LOG_ERROR("PACKET", "effectiveStats parts " + std::to_string(parts.size()) +
                            " stats " + std::to_string(partStats.size()) + " mismatch");
        return out;
    }
    if (parts.size() > PART_SLOTS) {
        LOG_WARN("PACKET", "effectiveStats " + std::to_string(parts.size()) +
                           " parts client applies seven");
    }

    const size_t n = parts.size() < PART_SLOTS ? parts.size() : PART_SLOTS;
    for (size_t p = 0; p < n; ++p) {
        if (parts[p].partId == 0) continue;
        const float mul = static_cast<float>(partLevelMultiplier(parts[p].level));
        for (size_t i = 0; i < out.size(); ++i) {
            const float bonus = partStats[p][i];
            out[i] += (i < STAT_LEVEL_SCALED_END) ? mul * bonus : bonus;
        }
    }
    return out;
}

float DriftBoostPackets::speedCeilingKmh(float effectiveSpeedStat) {
    return clampf(1.0f + effectiveSpeedStat, 1.0f, 2.0f) * SPEED_CEILING_BASE;
}

float DriftBoostPackets::targetSpeedKmh(float effectiveHandlingStat, bool drifting,
                                        float yawTerm) {
    const float v44 = drifting ? 1.6f : 1.0f;
    float v45 = effectiveHandlingStat + 1.0f;
    // collapses to v44 above two so handling over one LOWERS the target
    if (v45 < 1.0f) v45 = 1.0f;
    else if (v45 > 2.0f) v45 = v44;

    const float v43 = clampf(yawTerm, 0.0f, 50.0f);
    return v43 * (v45 + v44) * 0.3f + TARGET_SPEED_BASE;
}

DriftBoostTuning DriftBoostPackets::tuning(const KartStatBlock& e) {
    DriftBoostTuning t;
    // wire indices the 057 names and the old port enum sat two slots high
    t.speedCeilingKmh        = speedCeilingKmh(e[STAT_MAX_SPEED]);
    t.accelScale             = clampf(e[STAT_STEERING_GAIN] * 3.0f + 1.0f, 1.0f, 4.0f);
    t.torqueTrim             = clampf(e[STAT_BODY_SETUP] * 0.01f + 1.0f, 1.0f, 1.01f);
    t.miniTurboDurationMs    = clampf(1.0f + e[STAT_MINI_TURBO_TARGET], 1.0f, 2.0f) * MINI_TURBO_BASE_MS;
    t.miniTurboTargetKmh     = clampf(1.0f + e[STAT_MINI_TURBO_TARGET] * 0.2f, 1.0f, 1.2f)
                             * MINI_TURBO_BASE_KMH;
    t.driftRampRate          = clampf(e[STAT_DRIFT_CHARGE_RATE] * 0.5f + 0.3f, 0.3f, 0.8f);
    t.steerScale             = clampf(e[STAT_DRIFT_STEER] * 0.6f + 1.2f, 1.2f, 1.8f);
    t.driftSteerThresholdDeg = clampf(1.0f - e[STAT_MINI_TURBO_THRESHOLD] * 0.8f, 0.2f, 1.0f)
                             * DRIFT_STEER_BASE_DEG;
    t.driftChargeTimeMs      = clampf(1.0f - e[STAT_MINI_TURBO_HOLD] * 0.8f, 0.2f, 1.0f)
                             * DRIFT_CHARGE_BASE_MS;
    return t;
}

float DriftBoostPackets::boostDurationMs(int boostType, float effectiveBoostStat,
                                         bool hasExtendItem) {
    const float extend = hasExtendItem ? static_cast<float>(BOOST_EXTEND_MS) : 0.0f;
    switch (boostType) {
        case 0: return clampf(1.0f + effectiveBoostStat, 1.0f, 2.0f) * MINI_TURBO_BASE_MS;
        case 1: return 3800.0f + extend;
        case 2: return 6000.0f + extend;
        case 3: return 6000.0f;
        case 4: return 5000.0f;
        case 5: return 15000.0f;
        case 6: return 5.0f;
        case 7: return 1500.0f;
        default:
            LOG_WARN("PACKET", "boost type " + std::to_string(boostType) + " has no duration");
            return 0.0f;
    }
}

float DriftBoostPackets::boostWireMs(int boostType, float effectiveBoostStat,
                                     bool hasExtendItem) {
    const float duration = boostDurationMs(boostType, effectiveBoostStat, hasExtendItem);
    if (duration <= 0.0f) return 0.0f;
    // state 2 decays the strength by 0 86 a tick down to one then state 3 clears next tick
    const float tail = boostType == 0 ? static_cast<float>(BOOST_TAIL_MS_KIND0)
                                      : static_cast<float>(BOOST_TAIL_MS_OTHER);
    return duration + tail;
}

float DriftBoostPackets::boostTargetKmh(int boostType, float effectiveBoostStat) {
    switch (boostType) {
        case 0: return clampf(1.0f + effectiveBoostStat * 0.2f, 1.0f, 1.2f)
                     * MINI_TURBO_BASE_KMH;
        case 1: return 220.0f;
        case 2: return 260.0f;
        case 3: case 5: case 7: return 200.0f;
        case 6: return 1.0f;
        case 4:
            // sub 496BE0 never writes the target on this path
            LOG_WARN("PACKET", "boost type 4 target speed unknown");
            return 0.0f;
        default:
            LOG_WARN("PACKET", "boost type " + std::to_string(boostType) + " has no target");
            return 0.0f;
    }
}

bool DriftBoostPackets::boostCanPreempt(int activeType, int newType, bool rawWorld) {
    if (activeType < 0) return true;
    if (rawWorld) return newType != activeType;
    return newType >= activeType;
}

void DriftBoostPackets::resetTrack(DriftBoostTrack& track) {
    const SpeedLimits keep = track.limits;
    const uint32_t interval = track.reportIntervalMs;
    track = DriftBoostTrack{};
    track.limits = keep;
    track.reportIntervalMs = interval;
}

void DriftBoostPackets::resetTrack(DriftBoostTrack& track, const DriftBoostTuning& t) {
    SpeedLimits limits = track.limits;
    limits.ceilingWorldUnitsPerSec = t.speedCeilingKmh * KMH_TO_WORLD_UNPROVEN;
    const uint32_t interval = track.reportIntervalMs;
    track = DriftBoostTrack{};
    track.limits = limits;
    track.reportIntervalMs = interval;
}

DriftBoostPackets::PadHit DriftBoostPackets::padAt(const std::vector<SpawnPackets::ColPadCell>& pads,
                                                   float x, float y, float tol) {
    PadHit out;
    if (!std::isfinite(x) || !std::isfinite(y)) return out;
    for (const SpawnPackets::ColPadCell& cell : pads) {
        if (SpawnPackets::padCellContains(cell, x, y, tol)) {
            out.hit  = true;
            out.kind = cell.kind;
            out.row  = cell.row;
            return out;
        }
    }
    return out;
}

DriftBoostPackets::PadHit DriftBoostPackets::padOnPath(const std::vector<SpawnPackets::ColPadCell>& pads,
                                                       float x0, float y0, float x1, float y1,
                                                       float tol) {
    // the boost starts on the wheel touch the sample after it can sit a car length past the cell
    for (int i = PAD_PATH_STEPS; i >= 0; --i) {
        const float f = static_cast<float>(i) / static_cast<float>(PAD_PATH_STEPS);
        const PadHit hit = padAt(pads, x0 + (x1 - x0) * f, y0 + (y1 - y0) * f, tol);
        if (hit.hit) return hit;
    }
    return PadHit{};
}

bool DriftBoostPackets::padMatchesClass(const PadHit& hit, uint8_t boostClass) {
    if (!hit.hit) return false;
    // no ini means the kind is unknown so any class passes kind 6 cancels and never starts one
    if (hit.kind < 0) return true;
    if (hit.kind == 6) return false;
    return boostClass == 0 ? hit.kind == 0 : hit.kind != 0;
}

void DriftBoostPackets::resyncPosition(DriftBoostTrack& track) {
    track.havePos = false;
    track.lastSpeed = 0.0f;
    // the pad sweep must not span a teleport
    track.haveStatePos = false;
}

void DriftBoostPackets::noteItemUse(DriftBoostTrack& track, int32_t itemType,
                                    uint64_t serverMs) {
    if (!isBoostItemType(itemType)) return;
    track.pendingItemUse   = true;
    track.pendingItemType  = itemType;
    track.pendingItemUseMs = serverMs;
}

DriftBoostEvents DriftBoostPackets::observeState(DriftBoostTrack& track, uint16_t stateBits,
                                                 uint64_t serverMs,
                                                 const DriftBoostTuning& t,
                                                 const std::vector<SpawnPackets::ColPadCell>* pads,
                                                 float x, float y) {
    DriftBoostEvents ev;
    ev.violations = stateBitsAnomalies(stateBits);

    const DriftBoostState now = decodeStateBits(stateBits);

    // the client sends at a fixed period so sample counts hold where the receive clock jitters
    ++track.sampleIndex;
    const uint64_t sample = track.sampleIndex;
    const uint32_t interval = track.reportIntervalMs ? track.reportIntervalMs
                                                     : static_cast<uint32_t>(SELF_REPORT_MS);

    // the pad sweep runs from the previous sample position to this one
    const bool  havePrevPos = track.haveStatePos;
    const float prevX = track.stateX;
    const float prevY = track.stateY;
    if (pads != nullptr) {
        track.haveStatePos = true;
        track.stateX = x;
        track.stateY = y;
    }

    if (!track.haveState) {
        track.haveState = true;
        track.lastStateBits = stateBits;
        track.driftPhase = now.drifting
                         ? (now.miniTurboCharged ? DriftPhase::Charged : DriftPhase::Drifting)
                         : DriftPhase::Idle;
        if (now.drifting) {
            track.driftStartMs     = serverMs;
            track.driftStartSample = sample;
            track.driftCharged     = now.miniTurboCharged;
        }
        track.boostActive = now.boostClass0 || now.boostClass1;
        if (track.boostActive) {
            track.boostClass   = now.boostClass0 ? uint8_t{0} : uint8_t{1};
            track.boostStartMs = serverMs;
        }
        track.violations |= ev.violations;
        if (ev.violations) ++track.violationCount;
        return ev;
    }

    if (stateBits == track.lastStateBits) {
        track.violations |= ev.violations;
        return ev;
    }

    const DriftBoostState prev = decodeStateBits(track.lastStateBits);

    // stage 1 needs the kart hold time the wire shows it no sooner than this many samples in
    const uint64_t holdFloorSamples = t.driftChargeTimeMs > 0.0f
        ? static_cast<uint64_t>(t.driftChargeTimeMs / static_cast<float>(interval)) : 0;

    if (now.drifting && !prev.drifting) {
        ev.driftStarted          = true;
        track.driftPhase         = DriftPhase::Drifting;
        track.driftStartMs       = serverMs;
        track.driftStartSample   = sample;
        track.driftKeyUpSample   = 0;
        track.driftCharged       = false;
    }
    if (now.drifting && now.miniTurboCharged && !prev.miniTurboCharged) {
        ev.driftCharged     = true;
        track.driftPhase    = DriftPhase::Charged;
        track.driftChargeMs = serverMs;
        track.driftCharged  = true;
        // the charge cannot latch before the kart hold time counted in samples
        if (track.driftStartSample != 0 && sample >= track.driftStartSample) {
            const uint64_t held = sample - track.driftStartSample;
            if (held < holdFloorSamples) ev.violations |= VIOL_DRIFT_CHARGE;
        }
    }
    if (now.drifting && prev.drifting && prev.miniTurboCharged && !now.miniTurboCharged) {
        // the key went up stage 2 the gauge decays and the state stays until the zero crossing
        track.driftPhase       = DriftPhase::Releasing;
        track.driftKeyUpSample = sample;
        track.driftReleaseMs   = serverMs;
    }
    if (!now.drifting && prev.drifting) {
        ev.driftEnded = true;
        const uint64_t held = sample >= track.driftStartSample ? sample - track.driftStartSample : 0;
        // stage 1 can arm and the key lift between two samples so a long enough hold arms the window too
        const bool armed = track.driftCharged
                        || (holdFloorSamples > 0 ? held > holdFloorSamples : held >= 1);
        if (armed) {
            ev.driftReleased          = true;
            track.driftPhase          = DriftPhase::Released;
            track.driftReleaseSample  = sample;
            if (track.driftKeyUpSample == 0) track.driftReleaseMs = serverMs;
        } else {
            track.driftPhase = DriftPhase::Idle;
        }
    }

    const bool nowBoost  = now.boostClass0 || now.boostClass1;
    const bool prevBoost = prev.boostClass0 || prev.boostClass1;
    const uint8_t nowClass = now.boostClass0 ? uint8_t{0} : uint8_t{1};

    // the gas edge fires stage 3 in the tick that clears the state so the boost sits in that sample
    const uint64_t fireWindowSamples = static_cast<uint64_t>(FIRE_WINDOW_MS / static_cast<float>(interval)) + 1;
    const bool miniTurboWindow = track.driftPhase == DriftPhase::Released
        && sample >= track.driftReleaseSample
        && sample - track.driftReleaseSample <= static_cast<uint64_t>(RELEASE_GRACE_SAMPLES)
        && (track.driftKeyUpSample == 0 || sample - track.driftKeyUpSample <= fireWindowSamples);

    const bool itemPaired = track.pendingItemUse
                         && serverMs + PAIR_WINDOW_MS >= track.pendingItemUseMs
                         && track.pendingItemUseMs + PAIR_WINDOW_MS >= serverMs;

    PadHit pad;
    if (pads != nullptr && (nowBoost && (!prevBoost || nowClass != track.boostClass))) {
        pad = havePrevPos ? padOnPath(*pads, prevX, prevY, x, y, PAD_TOLERANCE_UNITS)
                          : padAt(*pads, x, y, PAD_TOLERANCE_UNITS);
    }

    if (nowBoost && !prevBoost) {
        ev.boostStarted   = true;
        track.boostActive = true;
        track.boostClass  = nowClass;
        track.boostStartMs = serverMs;

        BoostSource source = BoostSource::Unmatched;
        if (nowClass == 0) {
            if (miniTurboWindow) {
                source = BoostSource::MiniTurbo;
            } else if (pads == nullptr) {
                // no track knowledge a pad or a mini turbo cannot be told apart so no accusation
                source = BoostSource::Unmatched;
            } else if (padMatchesClass(pad, 0)) {
                source = BoostSource::Pad;
            } else {
                ev.violations |= VIOL_MINITURBO_FAST;
            }
        } else {
            if (itemPaired) {
                source = BoostSource::Item;
                track.pendingItemUse = false;
            } else if (pads != nullptr && padMatchesClass(pad, 1)) {
                source = BoostSource::Pad;
            } else if (miniTurboWindow) {
                // the pet loop of stage 3 rolls a kind 1 boost three times in a hundred
                source = BoostSource::MiniTurbo;
            } else if (pads != nullptr) {
                ev.violations |= VIOL_BOOST_UNMATCHED;
            }
        }
        if (source == BoostSource::MiniTurbo) track.driftPhase = DriftPhase::Idle;
        ev.source = source;
        track.boostSource = source;
    } else if (nowBoost && prevBoost && nowClass != track.boostClass) {
        // no wire edge between two boosts a pad or an item restarted it the state gate blocks a mini turbo
        ev.boostClassFlip = true;
        BoostSource source = BoostSource::Unmatched;
        if (nowClass == 1 && itemPaired) {
            source = BoostSource::Item;
            track.pendingItemUse = false;
        } else if (pads != nullptr && padMatchesClass(pad, nowClass)) {
            source = BoostSource::Pad;
        } else if (pads != nullptr) {
            ev.violations |= nowClass == 0 ? VIOL_BOOST_SPAM : VIOL_BOOST_UNMATCHED;
        }
        ev.source = source;
        track.boostSource = source;
        track.boostClass = nowClass;
        track.boostStartMs = serverMs;
    } else if (!nowBoost && prevBoost) {
        ev.boostEnded     = true;
        track.boostActive = false;
        track.boostSource = BoostSource::None;
        ev.source = BoostSource::None;
    }

    track.lastStateBits = stateBits;
    track.violations |= ev.violations;
    if (ev.violations) ++track.violationCount;
    return ev;
}

DriftBoostEvents DriftBoostPackets::observeState(DriftBoostTrack& track, uint16_t stateBits,
                                                 uint64_t serverMs,
                                                 const DriftBoostTuning& t) {
    // no track knowledge so a boost with no drift is never judged
    return observeState(track, stateBits, serverMs, t, nullptr, 0.0f, 0.0f);
}

DriftBoostEvents DriftBoostPackets::observeState(DriftBoostTrack& track, uint16_t stateBits,
                                                 uint64_t serverMs) {
    // no kart knowledge so the charge floor is zero and never fires
    DriftBoostTuning t;
    t.driftChargeTimeMs = 0.0f;
    return observeState(track, stateBits, serverMs, t);
}

SpeedVerdict DriftBoostPackets::checkSpeed(DriftBoostTrack& track, float x, float y, float z,
                                           uint64_t serverMs, float& speedOut) {
    speedOut = 0.0f;

    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) {
        LOG_WARN("PACKET", "driftboost speed sample not finite");
        return SpeedVerdict::BadSample;
    }

    if (!track.havePos) {
        track.havePos   = true;
        track.lastX     = x;
        track.lastY     = y;
        track.lastZ     = z;
        track.lastPosMs = serverMs;
        track.lastSpeed = 0.0f;
        return SpeedVerdict::FirstSample;
    }

    if (serverMs <= track.lastPosMs) {
        // duplicate or reordered sample carries no usable delta
        return SpeedVerdict::Rewind;
    }

    const uint64_t dtMs = serverMs - track.lastPosMs;

    if (dtMs >= track.limits.staleResyncMs) {
        // a long gap is a load screen or a lag spike never a cheat
        track.lastX = x; track.lastY = y; track.lastZ = z;
        track.lastPosMs = serverMs;
        track.lastSpeed = 0.0f;
        return SpeedVerdict::FirstSample;
    }

    const double dx = static_cast<double>(x) - track.lastX;
    const double dy = static_cast<double>(y) - track.lastY;
    const double dz = static_cast<double>(z) - track.lastZ;
    const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);

    if (dist > track.limits.teleportUnits) {
        track.violations |= VIOL_TELEPORT;
        ++track.violationCount;
        // baseline still advances else every later sample reads as a teleport too
        track.lastX = x; track.lastY = y; track.lastZ = z;
        track.lastPosMs = serverMs;
        track.lastSpeed = 0.0f;
        return SpeedVerdict::Teleport;
    }

    if (dtMs < track.limits.minDeltaMs) {
        // quantizer noise dominates below this so no speed can be trusted
        track.lastX = x; track.lastY = y; track.lastZ = z;
        track.lastPosMs = serverMs;
        return SpeedVerdict::Ok;
    }

    // travel per frame uses the fixed period not the receive gap else a bunched pair reads as 10x speed
    const uint32_t period = track.reportIntervalMs != 0 ? track.reportIntervalMs : 100;
    uint64_t judgeMs = (dtMs + period / 2) / period * period;
    if (judgeMs < period) judgeMs = period;
    const double speed = dist * 1000.0 / static_cast<double>(judgeMs);
    speedOut = static_cast<float>(speed);

    track.lastX = x; track.lastY = y; track.lastZ = z;
    track.lastPosMs = serverMs;
    track.lastSpeed = speedOut;

    if (track.limits.ceilingWorldUnitsPerSec > 0.0f) {
        const double cap = static_cast<double>(track.limits.ceilingWorldUnitsPerSec)
                         * static_cast<double>(track.limits.tolerance);
        if (speed > cap) {
            track.violations |= VIOL_SPEED;
            ++track.violationCount;
            return SpeedVerdict::OverCeiling;
        }
    }

    return SpeedVerdict::Ok;
}

const char* DriftBoostPackets::verdictName(SpeedVerdict verdict) {
    switch (verdict) {
        case SpeedVerdict::Ok:          return "ok";
        case SpeedVerdict::FirstSample: return "first";
        case SpeedVerdict::Rewind:      return "rewind";
        case SpeedVerdict::Teleport:    return "teleport";
        case SpeedVerdict::OverCeiling: return "overceiling";
        case SpeedVerdict::BadSample:   return "bad";
    }
    return "unknown";
}

std::string DriftBoostPackets::violationNames(uint32_t mask) {
    std::string out;
    auto add = [&out](const char* name) {
        if (!out.empty()) out.push_back(' ');
        out += name;
    };
    if (mask & VIOL_SPEED)           add("speed");
    if (mask & VIOL_TELEPORT)        add("teleport");
    if (mask & VIOL_BOOST_UNMATCHED) add("boost_unmatched");
    if (mask & VIOL_BOOST_SPAM)      add("boost_spam");
    if (mask & VIOL_DRIFT_CHARGE)    add("drift_charge");
    if (mask & VIOL_BOTH_CLASSES)    add("both_classes");
    if (mask & VIOL_RESERVED_BIT)    add("reserved_bit");
    if (mask & VIOL_MINITURBO_FAST)  add("miniturbo_fast");
    return out;
}

bool DriftBoostPackets::loadPartStats(uint32_t partId, KartStatBlock& out) {
    out.fill(0.0f);
    if (partId == 0) return false;

    auto rows = Database::instance().queryPrepared(
        "SELECT stat_block_hex FROM carcraft_part_def WHERE part_key = ? LIMIT 1",
        { partId });
    if (rows.empty()) {
        LOG_WARN("PACKET", "carcraft_part_def miss for part " + std::to_string(partId));
        return false;
    }
    return decodeStatHex(rowStr(rows[0], "stat_block_hex"), out);
}

std::vector<DriftEquippedPart> DriftBoostPackets::loadEquippedParts(uint32_t characterId,
                                                               uint32_t kartId) {
    std::vector<DriftEquippedPart> out;

    auto rows = Database::instance().queryPrepared(
        "SELECT slot_index, part_id, part_level FROM player_kart_parts "
        "WHERE character_id = ? AND kart_id = ? ORDER BY slot_index LIMIT 7",
        { characterId, kartId });

    out.reserve(rows.size());
    for (const auto& r : rows) {
        DriftEquippedPart p;
        p.partId = static_cast<uint32_t>(rowInt(r, "part_id"));
        p.level  = static_cast<int32_t>(rowInt(r, "part_level"));
        if (p.level < 0) {
            // negative level would flip the sign of every scaled bonus
            LOG_WARN("PACKET", "part " + std::to_string(p.partId) +
                               " level " + std::to_string(p.level) + " clamped to zero");
            p.level = 0;
        }
        out.push_back(p);
    }
    return out;
}

namespace {

KartDef kartDefFromRow(const std::map<std::string, std::string>& r) {
    KartDef d;
    d.visibleFlag    = static_cast<uint32_t>(rowInt(r, "visible_flag"));
    d.badge          = static_cast<uint32_t>(rowInt(r, "badge"));
    d.kartId         = static_cast<uint32_t>(rowInt(r, "kart_id"));
    d.unk0c          = static_cast<uint8_t>(rowInt(r, "unk0c") & 0xFF);
    d.vehicleKind    = static_cast<int32_t>(rowInt(r, "vehicle_kind"));
    d.modelScheme    = static_cast<int32_t>(rowInt(r, "model_scheme", 1));
    d.unk18          = static_cast<int32_t>(rowInt(r, "unk18"));
    d.requiredLevel  = static_cast<int32_t>(rowInt(r, "required_level"));
    d.modelName      = rowStr(r, "model_name");
    d.displayNameKey = rowStr(r, "display_name_key");
    d.descriptionKey = rowStr(r, "description_key");
    for (size_t i = 0; i < 17; ++i) d.stats[i] = rowFloat(r, kStatColumns[i]);
    return d;
}

const char* const kKartSelect =
    "SELECT kart_id, visible_flag, badge, unk0c, vehicle_kind, model_scheme, unk18, required_level, "
    "model_name, display_name_key, description_key, "
    "stat00_body_setup, stat01_max_speed, stat02_steering_gain, stat03_mini_turbo_target, "
    "stat04_boost_lean_lift, stat05_turn_force, stat06_wheel_spin, stat07_wheel_steer_angle, "
    "stat08_drift_charge_rate, stat09_drift_steer, stat10_mini_turbo_threshold, stat11_mini_turbo_hold, "
    "stat12_grip, stat13, stat14_camera_distance, stat15_camera_pitch, stat16_camera_height "
    "FROM kart_catalog ";

} // namespace

// used only by the effective stats physics path the 0x00C0 packet writer is PacketBuilder vehicleCatalog
bool DriftBoostPackets::loadKartDef(uint32_t kartId, KartDef& out) {
    auto rows = Database::instance().queryPrepared(
        std::string(kKartSelect) + "WHERE kart_id = ? LIMIT 1", { kartId });
    if (rows.empty()) {
        LOG_WARN("PACKET", "kart_catalog miss for kart " + std::to_string(kartId));
        return false;
    }
    out = kartDefFromRow(rows[0]);
    return true;
}

bool DriftBoostPackets::loadEffectiveStats(uint32_t characterId, uint32_t kartId,
                                           KartStatBlock& out) {
    out.fill(0.0f);

    KartDef def;
    if (!loadKartDef(kartId, def)) return false;

    const auto parts = loadEquippedParts(characterId, kartId);

    std::vector<KartStatBlock> stats;
    stats.reserve(parts.size());
    for (const auto& p : parts) {
        KartStatBlock b{};
        loadPartStats(p.partId, b);
        stats.push_back(b);
    }

    out = effectiveStats(def, parts, stats);
    return true;
}

} // namespace knc
