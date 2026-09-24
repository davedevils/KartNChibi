/// a bot on the standings scores like a human client at the same spot so the 0x45 rank is true

#include <gtest/gtest.h>

#include <vector>

#include "handlers/RaceBots.h"
#include "packets/gen/SpawnPackets.h"

using namespace knc;

namespace {

// four checkpoints on a square 0 is START
std::vector<SpawnPackets::TrackVec3> square() {
    return {{0.f, 0.f, 0.f}, {100.f, 0.f, 0.f}, {100.f, 100.f, 0.f}, {0.f, 100.f, 0.f}};
}

} // namespace

TEST(BotCheckpointScore, MatchesTheClientFormulaOnTheStartSegment) {
    BotCheckpointFollower f;
    const uint32_t bot = f.update(square(), 50.f, 0.f);
    EXPECT_EQ(f.checkpoint, 0);
    EXPECT_EQ(bot, SpawnPackets::progressScore(0, 1, 0.5f, 4));
}

TEST(BotCheckpointScore, StepsPastEachPointAndClosesTheLapOnStart) {
    BotCheckpointFollower f;
    f.update(square(), 50.f, 0.f);
    EXPECT_EQ(f.update(square(), 100.f, 50.f), SpawnPackets::progressScore(0, 2, 0.5f, 4));
    EXPECT_EQ(f.checkpoint, 1);
    f.update(square(), 50.f, 100.f);
    f.update(square(), 0.f, 50.f);
    f.update(square(), 50.f, 0.f);
    EXPECT_EQ(f.checkpoint, 0);
    EXPECT_EQ(f.laps, 1);
}

TEST(BotCheckpointScore, ABotAheadOnTheTrackOutranksAHumanBehind) {
    BotCheckpointFollower f;
    f.update(square(), 50.f, 0.f);
    const uint32_t bot = f.update(square(), 90.f, 100.f);
    const uint32_t human = SpawnPackets::progressScore(0, 2, 0.2f, 4);
    EXPECT_GT(bot, human);
}

TEST(BotCheckpointScore, AGridBehindStartStaysOnTheFirstSegment) {
    BotCheckpointFollower f;
    EXPECT_EQ(f.update(square(), -20.f, 0.f), SpawnPackets::progressScore(0, 1, 0.f, 4));
    EXPECT_EQ(f.checkpoint, 0);
    EXPECT_EQ(f.laps, 0);
}

TEST(RaceRankKey, OnlyGrowsAroundTheLap) {
    const auto pts = square();
    const float start = raceRankKey(pts, 0, 0, 50.f, 0.f);
    const float side = raceRankKey(pts, 0, 1, 100.f, 50.f);
    const float last = raceRankKey(pts, 0, 3, 0.f, 50.f);
    const float lap = raceRankKey(pts, 1, 0, 50.f, 0.f);
    EXPECT_LT(start, side);
    EXPECT_LT(side, last);
    EXPECT_LT(last, lap);
}

TEST(RaceRankKey, ACarJustPastStartRanksBehindACarFurtherOn) {
    const auto pts = square();
    const float human = raceRankKey(pts, 0, 0, 20.f, 0.f);
    const float bot = raceRankKey(pts, 0, 2, 60.f, 100.f);
    EXPECT_GT(bot, human);
}

TEST(RaceRankKey, TrackerLapsFollowTheClientCount) {
    SpawnPackets::LapTracker t;
    t.reset(4, 3);
    EXPECT_EQ(clientLapsFromTracker(t), 0);
    t.onCheckpoint({0, 1});
    EXPECT_EQ(clientLapsFromTracker(t), 0);
    t.onCheckpoint({1, 2});
    t.onCheckpoint({2, 3});
    t.onCheckpoint({3, 0});
    EXPECT_EQ(t.nextCheckpoint(), 0);
    EXPECT_EQ(clientLapsFromTracker(t), 1);
    t.onCheckpoint({0, 1});
    EXPECT_EQ(clientLapsFromTracker(t), 1);
    EXPECT_EQ(t.lapsCompleted(), 1);
}
