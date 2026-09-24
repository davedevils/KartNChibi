/// one seat one car the race grid takes exactly the members the room screen drew

#include <gtest/gtest.h>
#include <asio.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "game/Room.h"
#include "handlers/RaceBots.h"
#include "net/Session.h"
#include "packets/gen/MotionPackets.h"
#include "packets/gen/SpawnPackets.h"

using namespace knc;

namespace {

// a session that never connects the room only reads its id
std::shared_ptr<Session> fakeSession(asio::io_context& io, uint32_t characterId) {
    auto s = std::make_shared<Session>(asio::ip::tcp::socket(io));
    s->characterId = characterId;
    return s;
}

// the roster RaceHandler handleStartRace builds one row per seat humans and bots together
std::vector<int32_t> raceRoster(const Room& room) {
    std::vector<int32_t> ids;
    for (const auto& p : room.participants()) ids.push_back(p.characterId);
    return ids;
}

// every seat the room holds must own a slot no other seat owns
void expectSeatsUnique(const Room& room) {
    std::set<uint8_t> slots;
    std::set<int32_t> ids;
    for (const auto& p : room.participants()) {
        EXPECT_TRUE(slots.insert(p.slot).second)
            << "slot " << static_cast<int>(p.slot) << " taken twice";
        EXPECT_TRUE(ids.insert(p.characterId).second)
            << "character " << p.characterId << " seated twice";
        EXPECT_LT(p.slot, Room::kMaxGridSlots);
    }
    EXPECT_EQ(slots.size(), room.playerCount());
}

// the repo ships the track folders the server reads through KNC DATA ROOT
void useRepoTrackData() {
    _putenv_s("KNC_DATA_ROOT", KNC_REPO_ROOT "/DevClient/Data/Public");
}

// distance from a point to the closed racing line in the ground plane
float offLine(const std::vector<SpawnPackets::TrackPoint>& line, float x, float y) {
    float best = 1e30f;
    for (size_t i = 0; i < line.size(); ++i) {
        const SpawnPackets::TrackPoint& a = line[i];
        const SpawnPackets::TrackPoint& b = line[(i + 1) % line.size()];
        const float dx = b.x - a.x, dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        float t = 0.0f;
        if (len2 > 1e-6f) {
            t = ((x - a.x) * dx + (y - a.y) * dy) / len2;
            t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        }
        const float px = a.x + dx * t, py = a.y + dy * t;
        const float d = std::sqrt((x - px) * (x - px) + (y - py) * (y - py));
        if (d < best) best = d;
    }
    return best;
}

// a square loop with a node every ten units so a lap is four hundred
std::vector<SpawnPackets::TrackPoint> squareLine() {
    std::vector<SpawnPackets::TrackPoint> line;
    auto push = [&line](float x, float y) {
        SpawnPackets::TrackPoint p;
        p.x = x; p.y = y; p.z = 0.0f; p.yawDegrees = 0.0f;
        line.push_back(p);
    };
    for (int i = 0; i < 10; ++i) push(100.0f - i * 10.0f, 0.0f);
    for (int i = 0; i < 10; ++i) push(0.0f, i * 10.0f);
    for (int i = 0; i < 10; ++i) push(i * 10.0f, 100.0f);
    for (int i = 0; i < 10; ++i) push(100.0f, 100.0f - i * 10.0f);
    return line;
}

float lineLengthOf(const std::vector<SpawnPackets::TrackPoint>& line) {
    float total = 0.0f;
    for (size_t i = 0; i < line.size(); ++i) {
        const SpawnPackets::TrackPoint& a = line[i];
        const SpawnPackets::TrackPoint& b = line[(i + 1) % line.size()];
        const float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        total += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    return total;
}

} // namespace

TEST(RoomSeats, AHumanNeverTakesTheSeatOfAlreadySeatedBot) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 8;
    Room room(1, st);

    // quick match opened the room and seated the partner first
    ASSERT_EQ(room.addBots(1), 1);
    const uint8_t botSlot = room.bots().front().slot;

    auto human = fakeSession(io, 42);
    ASSERT_TRUE(room.addPlayer(human, 42, u"human", 10010));
    const RoomPlayer* seat = room.getPlayer(human->id());
    ASSERT_NE(seat, nullptr);

    // sub 40D650 drops the second 0x21 on a taken slot so the bot would race unseen
    EXPECT_NE(seat->slot, botSlot);
    expectSeatsUnique(room);
    EXPECT_EQ(raceRoster(room).size(), 2u);
}

TEST(RoomSeats, AJoinerNeverTakesTheSeatOfASeatedBot) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 8;
    Room room(2, st);

    auto host = fakeSession(io, 7);
    ASSERT_TRUE(room.addPlayer(host, 7, u"host", 10010));
    ASSERT_EQ(room.addBots(2), 2);

    auto guest = fakeSession(io, 8);
    ASSERT_TRUE(room.addPlayer(guest, 8, u"guest", 10010));

    expectSeatsUnique(room);
    EXPECT_EQ(room.playerCount(), 4u);
    EXPECT_EQ(raceRoster(room).size(), 4u);
}

TEST(RoomSeats, TheSameSessionCannotTakeTwoSeats) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 8;
    Room room(3, st);

    auto host = fakeSession(io, 5);
    ASSERT_TRUE(room.addPlayer(host, 5, u"host", 10010));
    // a second seat for the same socket puts two cars on the grid for one player
    EXPECT_FALSE(room.addPlayer(host, 5, u"host", 10010));
    EXPECT_EQ(room.playerCount(), 1u);
    EXPECT_EQ(raceRoster(room).size(), 1u);
    expectSeatsUnique(room);
}

TEST(RoomSeats, AFullRoomSeatsNoMoreBots) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 4;
    Room room(4, st);

    auto host = fakeSession(io, 11);
    ASSERT_TRUE(room.addPlayer(host, 11, u"host", 10010));

    // asks for nine the room only has three free seats
    EXPECT_EQ(room.addBots(9), 3);
    EXPECT_EQ(room.botCount(), 3u);
    EXPECT_EQ(room.playerCount(), 4u);
    EXPECT_TRUE(room.isFull());

    // a full room must add nothing rather than stack a second car on the last slot
    EXPECT_EQ(room.addBots(2), 0);
    EXPECT_EQ(room.botCount(), 3u);
    expectSeatsUnique(room);

    auto late = fakeSession(io, 12);
    EXPECT_FALSE(room.addPlayer(late, 12, u"late", 10010));
    expectSeatsUnique(room);
}

// RoomSettings fillWithBots used to be written and never read a room never topped up
TEST(RoomSeats, FillWithBotsIfEnabledTopsUpToSeatCap) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 4;
    st.fillWithBots = true;
    Room room(7, st);

    auto host = fakeSession(io, 31);
    ASSERT_TRUE(room.addPlayer(host, 31, u"host", 10010));

    EXPECT_EQ(room.fillWithBotsIfEnabled(), 3);
    EXPECT_EQ(room.playerCount(), 4u);
    EXPECT_TRUE(room.isFull());
    expectSeatsUnique(room);

    // a room already at cap has nothing left to add
    EXPECT_EQ(room.fillWithBotsIfEnabled(), 0);
}

TEST(RoomSeats, FillWithBotsIfEnabledDoesNothingWhenOff) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 4;
    st.fillWithBots = false;
    Room room(8, st);

    auto host = fakeSession(io, 32);
    ASSERT_TRUE(room.addPlayer(host, 32, u"host", 10010));

    EXPECT_EQ(room.fillWithBotsIfEnabled(), 0);
    EXPECT_EQ(room.playerCount(), 1u);
    EXPECT_EQ(room.botCount(), 0u);
}

TEST(RoomSeats, EveryBotIdIsReservedAndUnique) {
    RoomSettings st;
    st.maxPlayers = 16;
    Room room(5, st);

    EXPECT_EQ(room.addBots(16), 16);
    std::set<int32_t> ids;
    for (const auto& b : room.bots()) {
        EXPECT_TRUE(Room::isBotId(b.characterId)) << b.characterId;
        EXPECT_TRUE(ids.insert(b.characterId).second) << "bot id " << b.characterId << " twice";
    }
    expectSeatsUnique(room);
    EXPECT_EQ(room.addBots(4), 0);
}

TEST(RoomSeats, TheRaceRosterIsTheRoomMemberList) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 8;
    Room room(6, st);

    auto a = fakeSession(io, 21);
    auto b = fakeSession(io, 22);
    ASSERT_TRUE(room.addPlayer(a, 21, u"a", 10010));
    ASSERT_EQ(room.addBots(1), 1);
    ASSERT_TRUE(room.addPlayer(b, 22, u"b", 10010));
    ASSERT_EQ(room.addBots(2), 2);

    // what the room screen counts and what the grid spawns must be one number
    const std::vector<int32_t> roster = raceRoster(room);
    EXPECT_EQ(roster.size(), room.playerCount());
    EXPECT_EQ(roster.size(), room.humanCount() + room.botCount());

    int bots = 0;
    for (int32_t id : roster) if (Room::isBotId(id)) ++bots;
    EXPECT_EQ(static_cast<size_t>(bots), room.botCount());
    expectSeatsUnique(room);
}

TEST(RoomSeats, ASecondRaceDoesNotInheritTheFirstRaceBots) {
    asio::io_context io;
    RoomSettings st;
    st.maxPlayers = 8;
    Room room(7, st);

    auto host = fakeSession(io, 31);
    ASSERT_TRUE(room.addPlayer(host, 31, u"host", 10010));
    ASSERT_EQ(room.addBots(3), 3);
    EXPECT_EQ(raceRoster(room).size(), 4u);

    // endRace drops the CPU cars so the waiting room frees the seats
    room.clearBots();
    EXPECT_EQ(room.botCount(), 0u);
    const std::vector<int32_t> afterRace = raceRoster(room);
    ASSERT_EQ(afterRace.size(), 1u);
    EXPECT_EQ(afterRace.front(), 31);

    // the next race takes only the cars the room screen seated again
    ASSERT_EQ(room.addBots(1), 1);
    EXPECT_EQ(raceRoster(room).size(), 2u);
    expectSeatsUnique(room);
}

TEST(RoomSeats, TeamModeSeatsBotsOnBothSides) {
    RoomSettings st;
    st.mode = GameMode::ItemTeam;
    st.maxPlayers = 8;
    Room room(8, st);

    ASSERT_EQ(room.addBots(4), 4);
    int red = 0, blue = 0;
    for (const auto& b : room.bots()) {
        if (b.team == 1) ++red;
        else if (b.team == 2) ++blue;
    }
    EXPECT_EQ(red, 2);
    EXPECT_EQ(blue, 2);
}

// maps 100 and 101 hold one track spawn row a short grid used to drop every seat past it
TEST(SpawnGrid, PadGeneratedGridReachesTheWantedCount) {
    std::vector<GridSpawn> grid;
    GridSpawn only;
    only.gridIndex = 0;
    only.x = 870.398f;
    only.y = -47.741f;
    only.z = -88.951f;
    grid.push_back(only);

    const std::vector<GridSpawn> padded = MotionPackets::padGeneratedGrid(grid, 6);
    ASSERT_EQ(padded.size(), 6u);

    // the authored row never moves and every generated index is unique
    EXPECT_FLOAT_EQ(padded[0].x, only.x);
    std::set<int32_t> seen;
    for (const auto& s : padded) EXPECT_TRUE(seen.insert(s.gridIndex).second);

    // stacked rows never land exactly on the one they were copied from
    for (size_t i = 1; i < padded.size(); ++i) {
        EXPECT_NE(padded[i].z, padded[0].z);
    }
}

// an empty db grid and an empty ini both still produce a full grid nobody spawns at the origin twice
TEST(SpawnGrid, PadGeneratedGridFromEmptyStillFillsAndStaysUnique) {
    const std::vector<GridSpawn> padded = MotionPackets::padGeneratedGrid({}, 4);
    ASSERT_EQ(padded.size(), 4u);
    std::set<int32_t> seen;
    std::set<float> zs;
    for (const auto& s : padded) {
        EXPECT_TRUE(seen.insert(s.gridIndex).second);
        zs.insert(s.z);
    }
    EXPECT_EQ(zs.size(), padded.size()) << "generated rows must not sit on top of each other";
}

// the wire grid caps at sixteen sub 4B5160 skips anything past index 15
TEST(SpawnGrid, PadGeneratedGridNeverPassesTheWireCap) {
    const std::vector<GridSpawn> padded = MotionPackets::padGeneratedGrid({}, 30);
    EXPECT_EQ(padded.size(), 16u);
}

// a grid already at or past the target is left alone
TEST(SpawnGrid, PadGeneratedGridNeverShrinksAnAlreadyFullGrid) {
    std::vector<GridSpawn> grid;
    for (int i = 0; i < 8; ++i) {
        GridSpawn s;
        s.gridIndex = i;
        grid.push_back(s);
    }
    const std::vector<GridSpawn> padded = MotionPackets::padGeneratedGrid(grid, 4);
    EXPECT_EQ(padded.size(), 8u);
}

TEST(BotDriverLine, TheScoreIsTheClientLapSpace) {
    const std::vector<SpawnPackets::TrackPoint> line = squareLine();
    const float length = lineLengthOf(line);
    ASSERT_NEAR(length, 400.0f, 0.1f);

    BotDriver d;
    d.arm(&line, length, line[0].x, line[0].y, line[0].z, 0.0f, 40.0f, 3, 1234u);

    // C2S 0x67 is laps times 5000 plus the bucket so score over 5000 is the completed laps
    EXPECT_EQ(d.lapsDone(), 0);
    EXPECT_LT(d.progressScore(), 5000u);

    uint64_t now = 0;
    for (int i = 0; i < 400 && d.lapsDone() < 1; ++i) {
        now += 50;
        d.step(now, 50);
    }
    ASSERT_EQ(d.lapsDone(), 1);
    EXPECT_GE(d.progressScore(), 5000u);
    EXPECT_LT(d.progressScore(), 10000u);
    EXPECT_EQ(d.progressScore() / 5000u, static_cast<uint32_t>(d.lapsDone()));

    for (int i = 0; i < 2000 && !d.raceDone(); ++i) {
        now += 50;
        d.step(now, 50);
    }
    EXPECT_TRUE(d.raceDone());
    EXPECT_EQ(d.lapsDone(), 3);
}

TEST(BotDriverLine, TheAnchorIsFiftyClientFramesAheadOnTheLine) {
    const std::vector<SpawnPackets::TrackPoint> line = squareLine();
    const float length = lineLengthOf(line);

    BotDriver d;
    d.arm(&line, length, line[0].x, line[0].y, line[0].z, 0.0f, 40.0f, 3, 99u);

    uint64_t now = 0;
    for (int i = 0; i < 40; ++i) { now += 50; d.step(now, 50); }

    float tx = 0.0f, ty = 0.0f, tz = 0.0f;
    d.lookahead(tx, ty, tz);
    const float lead = std::sqrt((tx - d.x()) * (tx - d.x()) + (ty - d.y()) * (ty - d.y()));

    // motion send 0x40 writes pos plus frameDt times fifty times velocity a stock client runs sixty frames
    const float want = d.speedNow() * 50.0f / 60.0f;
    EXPECT_GT(want, 1.0f);
    EXPECT_NEAR(lead, want, want * 0.25f);
    // the anchor walks the line so a corner never puts the car into the scenery
    EXPECT_LT(offLine(line, tx, ty), 1.0f);
}

TEST(BotDriverLine, TheCarDrivesTheLineOfThePickedTrack) {
    useRepoTrackData();

    const BotTrack forest = BotTrack::load("Forest", "Forest_01");
    ASSERT_TRUE(forest.loaded) << "no racing line under the repo track data";
    ASSERT_GE(forest.line.size(), 3u);

    const std::vector<SpawnPackets::TrackPoint> grid =
        SpawnPackets::loadStartGrid("Forest", "Forest_01");
    ASSERT_FALSE(grid.empty());

    BotDriver d;
    d.arm(&forest.line, forest.lineLength,
          grid[1].x, grid[1].y, grid[1].z, grid[1].yawDegrees, 76.0f, 3, 7u);

    uint64_t now = 0;
    float maxStep = 0.0f;
    float worstOff = 0.0f;
    float px = d.x(), py = d.y();
    for (int i = 0; i < 6000 && !d.raceDone(); ++i) {
        now += 50;
        d.step(now, 50);
        const float step = std::sqrt((d.x() - px) * (d.x() - px) + (d.y() - py) * (d.y() - py));
        maxStep = std::max(maxStep, step);
        px = d.x(); py = d.y();
        // the first seconds roll off the grid onto the line
        if (now > 4000) worstOff = std::max(worstOff, offLine(forest.line, d.x(), d.y()));
    }

    EXPECT_TRUE(d.raceDone());
    // no teleport a fifty ms step can never move more than the pace allows
    EXPECT_LT(maxStep, 76.0f * 0.05f * 2.0f);
    // the car sits on the shipped line of this track not near it
    EXPECT_LT(worstOff, 1.0f);
}

TEST(BotDriverLine, ADifferentTrackGivesItsOwnLine) {
    useRepoTrackData();

    const BotTrack forest = BotTrack::load("Forest", "Forest_01");
    const BotTrack desert = BotTrack::load("Desert", "Desert_01");
    ASSERT_TRUE(forest.loaded);
    ASSERT_TRUE(desert.loaded);
    EXPECT_NE(forest.line.size(), desert.line.size());

    BotDriver d;
    d.arm(&desert.line, desert.lineLength,
          desert.line[0].x, desert.line[0].y, desert.line[0].z,
          desert.line[0].yawDegrees, 80.0f, 1, 3u);

    uint64_t now = 0;
    for (int i = 0; i < 400; ++i) { now += 50; d.step(now, 50); }

    // a bot armed on one track must never wander onto the nodes of another
    EXPECT_LT(offLine(desert.line, d.x(), d.y()), 1.0f);
    EXPECT_GT(offLine(forest.line, d.x(), d.y()), 1.0f);
}

TEST(BotDriverLine, ATrackWithNoLineNeverArms) {
    useRepoTrackData();

    const BotTrack none = BotTrack::load("Forest", "no_such_track");
    EXPECT_FALSE(none.loaded);
    EXPECT_TRUE(none.line.empty());

    BotDriver d;
    d.arm(nullptr, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 60.0f, 3, 1u);
    EXPECT_FALSE(d.step(50, 50));
}

// every track a room can pick the CPU car must join the line and run the laps
TEST(BotDriverLine, EveryShippedTrackDrivesCleanFromItsGrid) {
    useRepoTrackData();

    struct Pick { const char* theme; const char* track; };
    static const Pick kTracks[] = {
        {"Forest", "Forest_01"}, {"Forest", "Forest_02"}, {"Forest", "Forest_03"}, {"Forest", "Forest_04"},
        {"Desert", "Desert_01"}, {"Desert", "Desert_02"}, {"Desert", "Desert_03"}, {"Desert", "Desert_04"},
        {"Snow", "Snow_01"}, {"Snow", "Snow_02"}, {"Snow", "Snow_03"}, {"Snow", "Snow_04"},
        {"Palace", "Palace_01"}, {"Palace", "Palace_02"}, {"Palace", "Palace_03"},
        {"Palace", "Palace_04"}, {"Palace", "Palace_05"},
        {"Cookie", "Cookie_01"}, {"Cookie", "Cookie_02"}, {"Cookie", "Cookie_03"}, {"Cookie", "Cookie_04"},
        {"Toy", "Toy_01"}, {"Toy", "Toy_02"}, {"Toy", "Toy_03"}, {"Toy", "Toy_04"},
        {"Devil", "Devil_01"}, {"Devil", "Devil_02"}, {"Devil", "Devil_03"},
        {"Devil", "Devil_04"}, {"Devil", "Devil_07"},
        {"Swamp", "Swamp_01"}, {"Swamp", "Swamp_02"}, {"Swamp", "Swamp_03"},
        {"Race", "Race_01"}, {"Race", "Race_02"},
    };

    for (const Pick& pick : kTracks) {
        const BotTrack track = BotTrack::load(pick.theme, pick.track);
        ASSERT_TRUE(track.loaded) << pick.theme << "/" << pick.track << " has no racing line";
        const std::vector<SpawnPackets::TrackPoint> grid =
            SpawnPackets::loadStartGrid(pick.theme, pick.track);
        ASSERT_GE(grid.size(), 8u) << pick.theme << "/" << pick.track;

        for (size_t row : {size_t(0), size_t(7)}) {
            const float pace = std::max(42.0f, std::min(180.0f, track.lineLength / 48.0f));
            BotDriver d;
            d.arm(&track.line, track.lineLength,
                  grid[row].x, grid[row].y, grid[row].z, grid[row].yawDegrees, pace, 3, 5u);

            uint64_t now = 0;
            float joinDist = -1.0f;
            float maxStep = 0.0f;
            float worstOff = 0.0f;
            float px = d.x(), py = d.y();
            for (int i = 0; i < 8000 && !d.raceDone(); ++i) {
                now += 50;
                d.step(now, 50);
                const float step = std::sqrt((d.x() - px) * (d.x() - px) + (d.y() - py) * (d.y() - py));
                maxStep = std::max(maxStep, step);
                px = d.x(); py = d.y();
                const float off = offLine(track.line, d.x(), d.y());
                if (joinDist < 0.0f && off < 1.0f) joinDist = d.distance();
                else if (joinDist >= 0.0f) worstOff = std::max(worstOff, off);
            }

            const std::string where = std::string(pick.theme) + "/" + pick.track +
                                      " row " + std::to_string(row);
            // the grid run must reach the line not cross the map to the far side of it
            EXPECT_GE(joinDist, 0.0f) << where << " never reached the line";
            EXPECT_LT(joinDist, 200.0f) << where << " drove " << joinDist << " units to join the line";
            // once on it the car never leaves the shipped nodes
            EXPECT_LT(worstOff, 1.0f) << where;
            // a fifty ms step can never move more than the pace allows
            EXPECT_LT(maxStep, pace * 0.05f * 2.0f) << where;
            EXPECT_TRUE(d.raceDone()) << where << " never finished three laps";
        }
    }
}
