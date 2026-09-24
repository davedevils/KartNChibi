/// the motion budget rebuilt on the client velocity clamp and replayed over three real races

#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "packets/gen/MotionPackets.h"

using namespace knc;

namespace {

struct Sample {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

// KCGR header then 28 byte samples the same reader the drift observer test uses
std::vector<Sample> loadGhost(const std::string& path) {
    std::vector<Sample> out;
    std::ifstream f(path, std::ios::binary);
    if (!f) return out;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    size_t off = 0;
    uint32_t frames = 0;
    if (raw.size() > 130 && std::memcmp(raw.data(), "KCGR", 4) == 0) {
        std::memcpy(&frames, raw.data() + 24, 4);
        uint16_t nameLen = 0;
        std::memcpy(&nameLen, raw.data() + 128, 2);
        off = 130 + nameLen;
    } else {
        frames = static_cast<uint32_t>(raw.size() / 28);
    }
    for (uint32_t i = 0; i < frames && off + 28 <= raw.size(); ++i, off += 28) {
        Sample s;
        std::memcpy(&s.x, raw.data() + off + 0x00, 4);
        std::memcpy(&s.y, raw.data() + off + 0x04, 4);
        std::memcpy(&s.z, raw.data() + off + 0x08, 4);
        out.push_back(s);
    }
    return out;
}

const char* const kRecordings[3] = {
    KNC_REPO_ROOT "/tools/replay/recordings/drift_t90_c7_20260915.ghost",
    KNC_REPO_ROOT "/tools/replay/recordings/plain_t90_c7_20260915.ghost",
    KNC_REPO_ROOT "/tools/replay/recordings/ragezone_t90_c8_20260911.ghost",
};

// the ghost writes one sample every 200 ms which is two wire periods of the 0x40 channel
constexpr uint64_t kGhostSampleMs = 200;

struct Counts {
    int ok = 0;
    int snap = 0;
    int teleport = 0;
    int first = 0;
};

Counts replay(const std::vector<Sample>& samples, const MotionGateLimits& limits) {
    Counts c;
    MotionGateState st;
    for (size_t i = 0; i < samples.size(); ++i) {
        const MotionVerdict v = MotionPackets::motionStep(
            st, limits, samples[i].x, samples[i].y, samples[i].z, kGhostSampleMs * i);
        switch (v) {
            case MotionVerdict::Ok:          ++c.ok; break;
            case MotionVerdict::Snap:        ++c.snap; break;
            case MotionVerdict::Teleport:    ++c.teleport; break;
            case MotionVerdict::FirstSample: ++c.first; break;
        }
    }
    return c;
}

} // namespace

// the default limits are the clamps body chassis integrate k1 applies not an inferred tier
TEST(MotionGate, DefaultsAreTheClientVelocityClamps) {
    MotionGateLimits lim;
    EXPECT_FLOAT_EQ(lim.axisXYWuS, 120.0f);
    EXPECT_FLOAT_EQ(lim.axisZWuS, 60.0f);
    EXPECT_EQ(lim.reportIntervalMs, 100u);
    EXPECT_FLOAT_EQ(lim.snapAxisWu, 80.0f);
}

// the client send period is fixed so two bunched frames still buy one whole period of travel
TEST(MotionGate, BunchedFramesStillGetAWholePeriod) {
    MotionGateLimits lim;
    EXPECT_EQ(MotionPackets::motionSteps(lim, 0), 1u);
    EXPECT_EQ(MotionPackets::motionSteps(lim, 10), 1u);
    EXPECT_EQ(MotionPackets::motionSteps(lim, 100), 1u);
    EXPECT_EQ(MotionPackets::motionSteps(lim, 200), 2u);
    EXPECT_EQ(MotionPackets::motionSteps(lim, 250), 3u);
    // a stall never hands out more than one second of free travel
    EXPECT_EQ(MotionPackets::motionSteps(lim, 5000), 10u);
}

// legal 100ms frame carries at most 12 units on x or y old 50ms floor gave 7-06 units
TEST(MotionGate, OneWirePeriodCoversTheClampedTopSpeed) {
    MotionGateLimits lim;
    MotionGateState st;
    ASSERT_EQ(MotionPackets::motionStep(st, lim, 0.0f, 0.0f, 0.0f, 0),
              MotionVerdict::FirstSample);

    float budget = 0.0f;
    // 12 0 units is the clamp itself and the gate must take it with room to spare
    EXPECT_EQ(MotionPackets::motionStep(st, lim, 12.0f, 0.0f, 0.0f, 100, &budget),
              MotionVerdict::Ok);
    EXPECT_GT(budget, 12.0f);

    // the same step arriving 10 ms after the last one is a bunched frame not a jump
    MotionGateState bunched;
    MotionPackets::motionStep(bunched, lim, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_EQ(MotionPackets::motionStep(bunched, lim, 12.0f, 0.0f, 0.0f, 10),
              MotionVerdict::Ok);
}

// a rescue drops the car on the nearest follow path node and that is never a strike
TEST(MotionGate, ARescueSnapsAndOnlyARealJumpIsAccused) {
    MotionGateLimits lim;
    MotionGateState st;
    MotionPackets::motionStep(st, lim, 0.0f, 0.0f, 0.0f, 0);

    // the largest respawn measured over the three recordings is 38 6 units on one axis
    EXPECT_EQ(MotionPackets::motionStep(st, lim, 38.6f, 0.0f, 3.0f, 100), MotionVerdict::Snap);

    // 0x5A32A0 bounds the rescue reprobe at 80 units so anything past it is a jump
    MotionGateState far;
    MotionPackets::motionStep(far, lim, 0.0f, 0.0f, 0.0f, 0);
    EXPECT_EQ(MotionPackets::motionStep(far, lim, 400.0f, 0.0f, 0.0f, 100),
              MotionVerdict::Teleport);

    // the sample is always taken so a gate can never wedge a car at the old pose
    EXPECT_FLOAT_EQ(far.x, 400.0f);
}

// the whole point a real race must pass with nothing accused
TEST(MotionGate, TheThreeRecordingsPassWithNoViolation) {
    MotionGateLimits lim;
    for (const char* path : kRecordings) {
        const std::vector<Sample> samples = loadGhost(path);
        ASSERT_GE(samples.size(), 600u) << "recording did not load " << path;
        const Counts c = replay(samples, lim);
        EXPECT_EQ(c.teleport, 0) << "recording accused of a teleport " << path;
        EXPECT_EQ(c.first, 1);
        EXPECT_EQ(c.ok + c.snap + c.first, static_cast<int>(samples.size()));
        // the only snaps a clean race may hold are its rescues
        EXPECT_LE(c.snap, 6) << "too many snaps in " << path;
    }
}

// the drift file is the long one 1171 samples of a real client
TEST(MotionGate, TheDriftRecordingIsTheLongOne) {
    const std::vector<Sample> samples = loadGhost(kRecordings[0]);
    ASSERT_EQ(samples.size(), 1171u);

    // the top step of a real race is the 120 clamp over two periods
    float maxAxis = 0.0f;
    for (size_t i = 1; i < samples.size(); ++i) {
        maxAxis = std::max(maxAxis, std::fabs(samples[i].x - samples[i - 1].x));
        maxAxis = std::max(maxAxis, std::fabs(samples[i].y - samples[i - 1].y));
    }
    EXPECT_GT(maxAxis, 24.0f);   // rescues sit above the clamp but under the rescue reach
    EXPECT_LT(maxAxis, 80.0f);
}

// the old tier gave 7 06 units at its 50 ms floor which bans a car driving the straight
TEST(MotionGate, TheOldInferredTierWouldHaveAccusedTheRecording) {
    MotionGateLimits old;
    old.axisXYWuS = 42.0f * 1.30f * 1.8f;   // the inferred grounded cap
    old.axisZWuS = old.axisXYWuS;
    old.reportIntervalMs = 50;              // old budget floor and no rescue reach
    old.snapAxisWu = 0.0f;

    const std::vector<Sample> samples = loadGhost(kRecordings[0]);
    ASSERT_FALSE(samples.empty());
    const Counts c = replay(samples, old);
    EXPECT_GT(c.teleport, 0) << "the old numbers were supposed to be too tight";
}
