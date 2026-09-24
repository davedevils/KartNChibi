/// Replays the stock client drift recording through the drift boost observer then three cheats

#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "packets/gen/DriftBoostPackets.h"
#include "packets/gen/SpawnPackets.h"

using namespace knc;

namespace {

// one 28 byte ghost sample as car ghost sample record 0x49FAD0 writes it
struct GhostSample {
    float    x = 0.0f, y = 0.0f, z = 0.0f;
    uint8_t  yaw = 0;
    uint16_t flags = 0;
    uint8_t  nibbles = 0;
    uint8_t  mask = 0;
};

// ghost flags 0x100 0x200 drift state 0x08 stage 1 0x80 kind 0 boost 0x40 other kind 0x04 0x02 steer
constexpr uint16_t GHOST_DRIFT_STATE = 0x0300;
constexpr uint16_t GHOST_STAGE_ONE   = 0x0008;
constexpr uint16_t GHOST_BOOST_KIND0 = 0x0080;
constexpr uint16_t GHOST_BOOST_OTHER = 0x0040;
constexpr uint16_t GHOST_STEER_LEFT  = 0x0004;
constexpr uint16_t GHOST_STEER_RIGHT = 0x0002;
constexpr uint8_t  MASK_ITEM_KEY     = 0x04;  // bit 2 is slot 4 the item key

// a ghost sample is one 0x40 self report the state word carries the same car fields
uint16_t stateBitsOf(const GhostSample& s) {
    uint16_t flags = 0;
    if (s.flags & GHOST_DRIFT_STATE) flags |= DriftBoostPackets::BIT_DRIFT;
    if (s.flags & GHOST_STAGE_ONE)   flags |= DriftBoostPackets::BIT_CHARGED;
    if (s.flags & GHOST_BOOST_KIND0) flags |= DriftBoostPackets::BIT_BOOST0;
    if (s.flags & GHOST_BOOST_OTHER) flags |= DriftBoostPackets::BIT_BOOST1;
    if (s.flags & GHOST_STEER_LEFT)  flags |= DriftBoostPackets::BIT_STEER_L;
    if (s.flags & GHOST_STEER_RIGHT) flags |= DriftBoostPackets::BIT_STEER_R;
    return DriftBoostPackets::makeStateBits(7, static_cast<uint8_t>(s.nibbles >> 4), flags);
}

uint32_t readU32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

// KCGR header then 28 byte samples docs tools README replay section
bool loadGhost(const std::string& path, std::vector<GhostSample>& out) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::vector<uint8_t> bytes;
    uint8_t buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
    std::fclose(f);
    if (bytes.size() < 132 || std::memcmp(bytes.data(), "KCGR", 4) != 0) return false;
    const uint32_t count = readU32(bytes.data() + 24);
    size_t off = 4 + 24 + 44 + 56;
    const size_t nameLen = bytes[off] | (static_cast<size_t>(bytes[off + 1]) << 8);
    off += 2 + nameLen;
    if (bytes.size() < off + 28u * count) return false;
    out.clear();
    out.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint8_t* p = bytes.data() + off + 28u * i;
        GhostSample s;
        std::memcpy(&s.x, p + 0, 4);
        std::memcpy(&s.y, p + 4, 4);
        std::memcpy(&s.z, p + 8, 4);
        s.yaw = p[12];
        s.flags = static_cast<uint16_t>(p[16] | (p[17] << 8));
        s.nibbles = p[20];
        s.mask = p[24];
        out.push_back(s);
    }
    return true;
}

// the 17 wire floats the login burst ships for template 10010 GHOST REFERENCE the recording
KartStatBlock template10010Stats() {
    return KartStatBlock{ 0.52f, 0.52f, 0.52f, 0.52f, 0.30f, 0.52f, 0.30f, 0.30f,
                          0.52f, 0.52f, 0.52f, 0.70f, 0.52f, 0.0f, 9.0f, 37.0f, 3.5f };
}

const char* const kRecording = KNC_REPO_ROOT "/tools/replay/recordings/drift_t90_c7_20260915.ghost";
const char* const kRace01    = KNC_REPO_ROOT "/DevClient/Data/Public/World/Race/Race_01";
// ghost sample step 200 ms ten 20 ms ticks the C2S 0x40 wire sends every 100 ms
constexpr uint64_t SAMPLE_MS = 200;
constexpr uint64_t WIRE_MS   = 100;

bool loadRace01Pads(std::vector<SpawnPackets::ColPadCell>& pads) {
    SpawnPackets::ColCheckpoints col;
    if (!SpawnPackets::loadColCheckpoints(std::string(kRace01) + "/track.COL", col)) return false;
    SpawnPackets::loadBoostPadKinds(std::string(kRace01) + "/boost.ini", col.pads);
    pads = col.pads;
    return true;
}

uint16_t bits(uint16_t flags) {
    return DriftBoostPackets::makeStateBits(7, 8, flags);
}

// a fresh track on the 100 ms wire far from any pad of Race 01
struct WireFeed {
    DriftBoostTuning tuning = DriftBoostPackets::tuning(template10010Stats());
    DriftBoostTrack  track;
    std::vector<SpawnPackets::ColPadCell> pads;
    uint64_t ms = 0;
    float x = 0.0f, y = 0.0f;
    uint32_t violations = 0;
    int flagged = 0;

    WireFeed() {
        DriftBoostPackets::resetTrack(track, tuning);
        track.reportIntervalMs = static_cast<uint32_t>(WIRE_MS);
    }

    // one sample the car rolls 8 units a sample along x
    DriftBoostEvents feed(uint16_t flags) {
        x += 8.0f;
        const DriftBoostEvents ev = DriftBoostPackets::observeState(track, bits(flags), ms, tuning,
                                                                    &pads, x, y);
        ms += WIRE_MS;
        if (ev.violations) { ++flagged; violations |= ev.violations; }
        return ev;
    }
};

} // namespace

TEST(DriftBoostObserver, TuningReadsTheWireIndices) {
    const DriftBoostTuning t = DriftBoostPackets::tuning(template10010Stats());
    // wire 11 hold 0 70 gives 1 minus 0 56 times 800
    EXPECT_NEAR(t.driftChargeTimeMs, 352.0f, 0.5f);
    // wire 10 threshold 0 52 gives 1 minus 0 416 times 10
    EXPECT_NEAR(t.driftSteerThresholdDeg, 5.84f, 0.01f);
    // wire 8 charge rate 0 52 gives 0 26 plus 0 3
    EXPECT_NEAR(t.driftRampRate, 0.56f, 0.001f);
    // wire 3 mini turbo target 0 52 gives 1 52 times 400 and 1 104 times 120
    EXPECT_NEAR(t.miniTurboDurationMs, 608.0f, 0.5f);
    EXPECT_NEAR(t.miniTurboTargetKmh, 132.48f, 0.05f);
}

TEST(DriftBoostObserver, BoostDurationsMatchTheTick) {
    // kind 0 is 400 times clamp of stat 3 plus 1 the others are immediates of car boost update 0x496E50
    EXPECT_NEAR(DriftBoostPackets::boostDurationMs(0, 0.52f, false), 608.0f, 0.5f);
    EXPECT_NEAR(DriftBoostPackets::boostDurationMs(0, 5.0f, false), 800.0f, 0.5f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(1, 0.0f, false), 3800.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(1, 0.0f, true), 4300.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(2, 0.0f, false), 6000.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(3, 0.0f, false), 6000.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(4, 0.0f, false), 5000.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(5, 0.0f, false), 15000.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(6, 0.0f, false), 5.0f);
    EXPECT_EQ(DriftBoostPackets::boostDurationMs(7, 0.0f, false), 1500.0f);
    // the wire bit outlives the duration by the decay tail the recording shows four to five samples of 200 ms
    const float wire0 = DriftBoostPackets::boostWireMs(0, 0.52f, false);
    EXPECT_GE(wire0, 800.0f);
    EXPECT_LE(wire0, 1000.0f);
    EXPECT_NEAR(DriftBoostPackets::boostWireMs(1, 0.0f, false), 3840.0f, 0.5f);
}

TEST(DriftBoostObserver, Race01PadCellsLoadWithTheirKinds) {
    std::vector<SpawnPackets::ColPadCell> pads;
    ASSERT_TRUE(loadRace01Pads(pads)) << kRace01;
    // 14 pads over 34 cells kind 0 on rows 1 to 5 10 12 to 14 kind 1 elsewhere
    EXPECT_EQ(pads.size(), 34u);
    int kind0 = 0, kind1 = 0;
    for (const auto& c : pads) {
        if (c.kind == 0) ++kind0;
        if (c.kind == 1) ++kind1;
    }
    EXPECT_EQ(kind0, 22);
    EXPECT_EQ(kind1, 12);
    // the recording crossed BOOST 001 at sample 23 and BOOST 011 at sample 262 wire frame
    const DriftBoostPackets::PadHit a = DriftBoostPackets::padAt(pads, -660.4f, 239.5f,
                                                                 DriftBoostPackets::PAD_TOLERANCE_UNITS);
    EXPECT_TRUE(a.hit);
    EXPECT_EQ(a.row, 1);
    EXPECT_EQ(a.kind, 0);
    const DriftBoostPackets::PadHit b = DriftBoostPackets::padAt(pads, 821.8f, -92.7f, 0.0f);
    EXPECT_TRUE(b.hit);
    EXPECT_EQ(b.row, 11);
    EXPECT_EQ(b.kind, 1);
    // the start line is no pad
    EXPECT_FALSE(DriftBoostPackets::padAt(pads, -321.3f, 228.6f, 4.0f).hit);
}

TEST(DriftBoostObserver, StockClientDriftRecordingHasNoViolation) {
    std::vector<GhostSample> samples;
    ASSERT_TRUE(loadGhost(kRecording, samples)) << kRecording;
    ASSERT_EQ(samples.size(), 1171u);
    std::vector<SpawnPackets::ColPadCell> pads;
    ASSERT_TRUE(loadRace01Pads(pads)) << kRace01;

    const DriftBoostTuning tuning = DriftBoostPackets::tuning(template10010Stats());
    DriftBoostTrack track;
    DriftBoostPackets::resetTrack(track, tuning);
    track.reportIntervalMs = static_cast<uint32_t>(SAMPLE_MS);

    uint32_t violations = 0;
    int flagged = 0;
    int miniTurbo = 0;
    int padBoosts = 0;
    int itemBoosts = 0;
    int boosts = 0;
    int drifts = 0;
    int charged = 0;
    bool itemHeld = false;
    for (size_t i = 0; i < samples.size(); ++i) {
        const GhostSample& s = samples[i];
        const uint64_t ms = SAMPLE_MS * i;
        // the item key edge is the C2S 0x47 the client sends when it starts an item boost
        const bool item = (s.mask & MASK_ITEM_KEY) != 0;
        if (item && !itemHeld) DriftBoostPackets::noteItemUse(track, 0, ms);
        itemHeld = item;

        float speed = 0.0f;
        DriftBoostPackets::checkSpeed(track, s.x, s.y, s.z, ms, speed);
        const DriftBoostEvents ev = DriftBoostPackets::observeState(track, stateBitsOf(s), ms, tuning,
                                                                    &pads, s.x, s.y);
        if (ev.driftStarted) ++drifts;
        if (ev.driftCharged) ++charged;
        if (ev.boostStarted) ++boosts;
        if (ev.boostStarted && ev.source == BoostSource::MiniTurbo) ++miniTurbo;
        if (ev.boostStarted && ev.source == BoostSource::Pad) ++padBoosts;
        if (ev.boostStarted && ev.source == BoostSource::Item) ++itemBoosts;
        if (ev.violations) {
            ++flagged;
            violations |= ev.violations;
            if (flagged <= 40) {
                std::printf("sample %zu t %.1f flags %#05x violation %s\n", i, ms / 1000.0,
                            s.flags, DriftBoostPackets::violationNames(ev.violations).c_str());
            }
        }
    }
    EXPECT_EQ(track.violations & (DriftBoostPackets::VIOL_SPEED | DriftBoostPackets::VIOL_TELEPORT), 0u)
        << DriftBoostPackets::violationNames(track.violations);
    EXPECT_EQ(violations, 0u) << DriftBoostPackets::violationNames(violations);
    EXPECT_EQ(flagged, 0);
    // 37 drift runs 12 mini turbos 20 pad boosts and the one item boost at 145 s
    EXPECT_EQ(drifts, 37);
    EXPECT_GE(charged, 30);
    EXPECT_EQ(boosts, 33);
    EXPECT_EQ(miniTurbo, 12);
    EXPECT_EQ(padBoosts, 20);
    EXPECT_EQ(itemBoosts, 1);
}

TEST(DriftBoostObserver, RealCadenceOnTheWirePasses) {
    // the recording cadence at 100 ms drift three samples stage 1 from the fourth key up gauge decay gas edge
    WireFeed w;
    const uint16_t D = DriftBoostPackets::BIT_DRIFT;
    const uint16_t C = DriftBoostPackets::BIT_CHARGED;
    const uint16_t B0 = DriftBoostPackets::BIT_BOOST0;
    for (int i = 0; i < 5; ++i) w.feed(0);
    w.feed(D); w.feed(D); w.feed(D);
    DriftBoostEvents ev = w.feed(D | C);
    EXPECT_TRUE(ev.driftCharged);
    w.feed(D | C); w.feed(D | C); w.feed(D | C); w.feed(D | C);
    // the key up drops stage 1 the state lives while the gauge decays two samples
    w.feed(D); w.feed(D);
    ev = w.feed(B0);
    EXPECT_TRUE(ev.driftEnded);
    EXPECT_TRUE(ev.boostStarted);
    EXPECT_EQ(ev.source, BoostSource::MiniTurbo);
    for (int i = 0; i < 8; ++i) w.feed(B0);
    for (int i = 0; i < 5; ++i) w.feed(0);
    // a short hold keeps stage 1 off the wire yet the drift lasted long enough for it
    w.feed(D); w.feed(D); w.feed(D); w.feed(D); w.feed(D);
    ev = w.feed(B0);
    EXPECT_EQ(ev.source, BoostSource::MiniTurbo);
    for (int i = 0; i < 8; ++i) w.feed(B0);
    for (int i = 0; i < 5; ++i) w.feed(0);
    // the mini turbo and the bit 4 drop in one sample right after the last charged sample
    w.feed(D); w.feed(D); w.feed(D); w.feed(D | C); w.feed(D | C); w.feed(D | C);
    ev = w.feed(B0);
    EXPECT_EQ(ev.source, BoostSource::MiniTurbo);
    for (int i = 0; i < 8; ++i) w.feed(B0);
    for (int i = 0; i < 5; ++i) w.feed(0);
    // the 3 percent lucky roll gives a class 1 boost off the same release
    w.feed(D); w.feed(D); w.feed(D); w.feed(D | C); w.feed(D | C); w.feed(D | C); w.feed(D);
    ev = w.feed(DriftBoostPackets::BIT_BOOST1);
    EXPECT_EQ(ev.source, BoostSource::MiniTurbo);
    EXPECT_EQ(w.violations, 0u) << DriftBoostPackets::violationNames(w.violations);
    EXPECT_EQ(w.flagged, 0);
}

TEST(DriftBoostObserver, BoostWithNoDriftAndNoPadIsFlagged) {
    WireFeed w;
    for (int i = 0; i < 10; ++i) w.feed(0);
    const DriftBoostEvents ev = w.feed(DriftBoostPackets::BIT_BOOST0);
    EXPECT_TRUE(ev.boostStarted);
    EXPECT_EQ(ev.source, BoostSource::Unmatched);
    EXPECT_NE(ev.violations & DriftBoostPackets::VIOL_MINITURBO_FAST, 0u);
    for (int i = 0; i < 8; ++i) w.feed(DriftBoostPackets::BIT_BOOST0);
    for (int i = 0; i < 5; ++i) w.feed(0);
    // a class 1 boost with no item no pad and no drift is unmatched
    const DriftBoostEvents ev1 = w.feed(DriftBoostPackets::BIT_BOOST1);
    EXPECT_EQ(ev1.source, BoostSource::Unmatched);
    EXPECT_NE(ev1.violations & DriftBoostPackets::VIOL_BOOST_UNMATCHED, 0u);
    EXPECT_EQ(w.flagged, 2);
}

TEST(DriftBoostObserver, MiniTurboEverySecondIsFlagged) {
    // one drift sample then a boost every second the hold never reaches stage 1
    WireFeed w;
    const uint16_t D = DriftBoostPackets::BIT_DRIFT;
    const uint16_t B0 = DriftBoostPackets::BIT_BOOST0;
    int flaggedStarts = 0;
    for (int round = 0; round < 10; ++round) {
        w.feed(D);
        const DriftBoostEvents ev = w.feed(B0);
        EXPECT_TRUE(ev.boostStarted);
        if (ev.violations & DriftBoostPackets::VIOL_MINITURBO_FAST) ++flaggedStarts;
        for (int i = 0; i < 8; ++i) w.feed(B0);
    }
    EXPECT_EQ(flaggedStarts, 10);
    // a charged bit on the first drift sample is faster than any kart allows
    w.feed(0); w.feed(0);
    const DriftBoostEvents fast = w.feed(D | DriftBoostPackets::BIT_CHARGED);
    EXPECT_NE(fast.violations & DriftBoostPackets::VIOL_DRIFT_CHARGE, 0u);
}

TEST(DriftBoostObserver, SpeedOverTheCapIsFlagged) {
    const DriftBoostTuning tuning = DriftBoostPackets::tuning(template10010Stats());
    DriftBoostTrack track;
    DriftBoostPackets::resetTrack(track, tuning);
    // the cap is 1 52 times 320 kmh in world units times the tolerance 120 a second passes
    const float cap = track.limits.ceilingWorldUnitsPerSec * track.limits.tolerance;
    EXPECT_GT(cap, 130.0f);
    float speed = 0.0f;
    EXPECT_EQ(DriftBoostPackets::checkSpeed(track, 0.0f, 0.0f, 1.0f, 0, speed), SpeedVerdict::FirstSample);
    EXPECT_EQ(DriftBoostPackets::checkSpeed(track, 12.0f, 0.0f, 1.0f, 100, speed), SpeedVerdict::Ok);
    EXPECT_NEAR(speed, 120.0f, 0.01f);
    // a step of cap plus ten percent over one wire period
    const float over = (cap * 1.1f) * 0.1f;
    EXPECT_EQ(DriftBoostPackets::checkSpeed(track, 12.0f + over, 0.0f, 1.0f, 200, speed), SpeedVerdict::OverCeiling);
    EXPECT_NE(track.violations & DriftBoostPackets::VIOL_SPEED, 0u);
    // a single step past the hard cap is a teleport
    EXPECT_EQ(DriftBoostPackets::checkSpeed(track, 1000.0f, 0.0f, 1.0f, 300, speed), SpeedVerdict::Teleport);
}

TEST(DriftBoostObserver, PadBoostNeedsTheMatchingKind) {
    std::vector<SpawnPackets::ColPadCell> pads;
    ASSERT_TRUE(loadRace01Pads(pads)) << kRace01;
    const DriftBoostTuning tuning = DriftBoostPackets::tuning(template10010Stats());
    DriftBoostTrack track;
    DriftBoostPackets::resetTrack(track, tuning);
    // rolling over BOOST 001 kind 0 a class 0 boost is the pad a class 1 boost is not
    uint64_t ms = 0;
    DriftBoostPackets::observeState(track, bits(0), ms, tuning, &pads, -640.0f, 239.5f); ms += WIRE_MS;
    DriftBoostPackets::observeState(track, bits(0), ms, tuning, &pads, -652.0f, 239.5f); ms += WIRE_MS;
    DriftBoostEvents ev = DriftBoostPackets::observeState(track, bits(DriftBoostPackets::BIT_BOOST0), ms,
                                                          tuning, &pads, -664.0f, 239.5f); ms += WIRE_MS;
    EXPECT_EQ(ev.source, BoostSource::Pad);
    EXPECT_EQ(ev.violations, 0u);
    for (int i = 0; i < 9; ++i) {
        DriftBoostPackets::observeState(track, bits(DriftBoostPackets::BIT_BOOST0), ms, tuning, &pads,
                                        -676.0f - 12.0f * i, 239.5f); ms += WIRE_MS;
    }
    DriftBoostPackets::observeState(track, bits(0), ms, tuning, &pads, -800.0f, 239.5f); ms += WIRE_MS;
    DriftBoostPackets::observeState(track, bits(0), ms, tuning, &pads, -812.0f, 239.5f); ms += WIRE_MS;

    DriftBoostTrack other;
    DriftBoostPackets::resetTrack(other, tuning);
    ms = 0;
    DriftBoostPackets::observeState(other, bits(0), ms, tuning, &pads, -640.0f, 239.5f); ms += WIRE_MS;
    DriftBoostPackets::observeState(other, bits(0), ms, tuning, &pads, -652.0f, 239.5f); ms += WIRE_MS;
    ev = DriftBoostPackets::observeState(other, bits(DriftBoostPackets::BIT_BOOST1), ms, tuning, &pads,
                                         -664.0f, 239.5f);
    EXPECT_EQ(ev.source, BoostSource::Unmatched);
    EXPECT_NE(ev.violations & DriftBoostPackets::VIOL_BOOST_UNMATCHED, 0u);
}
